#include "sys/page_map.h"
#include "sys/buddy_allocator.h"
#include "sys/log.h"
#include "libc/string.h"

uint64_t ToHighMem(uint64_t paddr)
{
    return paddr + KERNEL_DATA_BASE;
}

void *ToHighMem(void *mem)
{
    return (void*) ((uintptr_t) mem + KERNEL_DATA_BASE);
}

uint64_t FromHighMem(uint64_t vaddr)
{
    return vaddr - KERNEL_DATA_BASE * (vaddr > KERNEL_DATA_BASE);
}

void *FromHighMem(void *mem)
{
    return (void*) FromHighMem((uintptr_t) mem);
}

PageMap::PageMap()
    : root_((uint64_t*) ToHighMem(BuddyAllocator::Allocate(FRAME_SIZE)))
{}

PageMap::PageMap(struct stivale2_struct_tag_memmap *memmap,
                 struct stivale2_struct_tag_kernel_base_address *kern_base_addr,
                 struct stivale2_struct_tag_pmrs *pmrs)
     : root_((uint64_t*) ToHighMem(BuddyAllocator::Allocate(FRAME_SIZE)))
{
    // Map 0-4GiB to higher half and identity map as well.
    const uint64_t four_gib = 0x100000000;
    MapRange({ 0x1000,  four_gib }, 0);
    MapRange({ 0x1000,  four_gib }, KERNEL_DATA_BASE);

    uint64_t kern_phys_base = kern_base_addr->physical_base_address;
    uint64_t kern_virt_base = kern_base_addr->virtual_base_address;
    for(size_t i = 0; i < pmrs->entries; ++i) {
        struct stivale2_pmr pmr = pmrs->pmrs[i];
        uint64_t virt = pmr.base;
        uint64_t phys = kern_phys_base + (pmr.base - kern_virt_base);
        uint64_t len  = pmr.length;
        uint64_t perms = pmr.permissions;

        uint16_t flags = PRESENT;
        flags |= (perms == (1 << 0) ? EXECUTABLE    : 0);
        flags |= (perms == (1 << 1) ? READ_WRITABLE : 0);

        // Map all page frames in PMR to a virtual address determined by the
        // offset_ between the PMR's physical and virtual address.
        uint64_t offset	= virt - phys;
        MapRange({ phys, phys + len }, offset, flags);
    }

    for(size_t i = 0; i < memmap->entries; ++i) {
        uint64_t base = memmap->memmap[i].base;
        uint64_t bound = base + memmap->memmap[i].length;

        // Identity map framebuffer and bootloader-reclaimable regions, mark as
        // executable. For everything else, map to higher half.
        if(memmap->memmap[i].type == STIVALE2_MMAP_FRAMEBUFFER ||
           memmap->memmap[i].type == STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE)
        {
            MapRange({ base, bound }, 0, KERNEL_PAGE & EXECUTABLE);
        } else if(base >= four_gib) {
            MapRange({ base, bound }, 0, KERNEL_PAGE);
            MapRange({ base, bound }, vmem_direct_mapping_base_, KERNEL_PAGE);
        }
    }
}

PageMap::~PageMap()
{
    DeepFree(root_, 4);
}

PageMap::PageMap(const PageMap &rhs)
    : root_((uint64_t*) ToHighMem(BuddyAllocator::Allocate(FRAME_SIZE)))
{
    DeepCopy(rhs.root_, root_, 4);
}

PageMap &PageMap::operator=(const PageMap &rhs)
{
    if(this == &rhs) {
        return *this;
    }

    DeepFree(root_, 4);
    root_ = (uint64_t*) ToHighMem(BuddyAllocator::Allocate(FRAME_SIZE));
    DeepCopy(rhs.root_, root_, 4);
    return *this;
}

bool PageMap::Map(uint64_t paddr, uint64_t vaddr, uint16_t flags)
{
    uint64_t *page_table_entry = CreatePage(root_, vaddr, flags);
    if(! page_table_entry) {
        return false;
    }
    *page_table_entry = paddr | flags;
    return true;
}

bool PageMap::MapRange(const AddrRange &phys_range, size_t virt_offset,
                       uint16_t flags)
{
    //Log("Mapping phys 0x%x-0x%x -> virt 0x%x-0x%x\n",
    //    phys_range.base_, phys_range.bound_,
    //    phys_range.base_ + virt_offset,
    //    phys_range.bound_ + virt_offset);
    for(uint64_t addr = phys_range.base_; addr < phys_range.bound_;
        addr += FRAME_SIZE)
    {
        if(! Map(addr, addr + virt_offset, flags)) {
            return false;
        }
    }
    return true;
}

bool PageMap::Remap(uint64_t vaddr, uint64_t new_vaddr, uint16_t flags)
{
    uint64_t paddr = VAddrToPAddr(vaddr);
    bool unmap_code = Unmap(vaddr);
    bool map_code = Map(paddr, new_vaddr, flags);
    __asm__ __volatile__("invlpg [%0]" :: "r"(vaddr) : "memory");
    return unmap_code && map_code;
}

bool PageMap::RemapRange(const AddrRange &vaddr_range, uint64_t new_vaddr_base,
                         uint16_t flags)
{
    for(uint64_t vaddr = vaddr_range.base_; vaddr < vaddr_range.bound_;
        vaddr += FRAME_SIZE)
    {
        size_t offset = vaddr - vaddr_range.base_;
        if(! Remap(vaddr, new_vaddr_base + offset, flags)) {
            return false;
        }
    }
    return true;
}

bool PageMap::Unmap(uint64_t vaddr)
{
    uint64_t *page_table_entry = GetPage(root_, vaddr);
    if(! page_table_entry) {
        return false;
    }

    *page_table_entry = 0;
    __asm__ volatile("invlpg [%0]" :: "r" (vaddr) : "memory");
    return true;
}

bool PageMap::UnmapRange(const AddrRange &vaddr_range)
{
    for(uint64_t vaddr = vaddr_range.base_; vaddr < vaddr_range.bound_;
        vaddr += FRAME_SIZE)
    {
        if(!Unmap(vaddr)) {
            return false;
        }
    }
    return true;
}

uint64_t PageMap::VAddrToPAddr(uint64_t vaddr)
{
    return VAddrToPAddr(root_, vaddr);
}

void PageMap::Load()
{
    __asm__ volatile("mov %%cr3, %0" :: "r"((uint64_t) root_ - KERNEL_DATA_BASE));
}

size_t PageMap::VAddrIndex(uint64_t vaddr, uint8_t level)
{
    return (vaddr >> (LOG2_FRAME_SIZE + LOG2_ENTRIES_PER_TABLE * (level - 1))) &
           MAX_PAGE_IND;
}

bool PageMap::GetPageFlag(uint64_t page, uint64_t flag)
{
    return (page & flag) != 0;
}

uint64_t *PageMap::GetPageTable(uint64_t *parent, uint64_t index)
{
    if(GetPageFlag(parent[index], PRESENT)) {
        return (uint64_t *) (ToHighMem(parent[index] & TAB_ADDR_MASK));
    }
    return NULL;
}

uint64_t *PageMap::GetOrCreatePageTable(uint64_t*parent, uint64_t index,
                                        uint16_t flags)
{
    if(GetPageFlag(parent[index], PRESENT)) {
        return (uint64_t *) (ToHighMem(parent[index] & TAB_ADDR_MASK));
    }

    void *free_frame = BuddyAllocator::Allocate(FRAME_SIZE);
    if(! free_frame) {
        return NULL;
    }

    parent[index] = ((uint64_t) free_frame) | flags;
    return (uint64_t*) ToHighMem((uintptr_t) free_frame);
}

uint64_t *PageMap::GetPage(uint64_t *page_table_root, uint64_t vaddr)
{
    uint64_t *parent_table = page_table_root;
    for(int i = 4; i > 1; --i) {
        uint64_t tab_index = VAddrIndex(vaddr, i);
        uint64_t *child_table = GetPageTable(parent_table, tab_index);
        if(! child_table) {
            return NULL;
        }
        parent_table = child_table;
    }

    return &parent_table[VAddrIndex(vaddr, 1)];
}

uint64_t *PageMap::CreatePage(uint64_t *page_table_root, uint64_t vaddr,
                              uint16_t flags)
{
    uint64_t *parent_table = page_table_root;
    for(int i = 4; i > 1; --i) {
        uint64_t tab_index = VAddrIndex(vaddr, i);
        uint64_t *child_table = GetOrCreatePageTable(parent_table, tab_index,
                                                     flags);
        if(child_table == NULL) {
            return NULL;
        }
        parent_table = child_table;
    }

    return &parent_table[VAddrIndex(vaddr, 1)];
}

uint64_t PageMap::VAddrToPAddr(uint64_t *table, uint64_t vaddr)
{
    uint64_t *parent_table = table, *child_table;
    for(int i = 4; i > 1; --i) {
        uint64_t tab_index = VAddrIndex(vaddr, i);
        child_table = GetPageTable(parent_table, tab_index);
        if(!child_table) {
            return 0;
        }
        parent_table = child_table;
    }
    // Get rid of flags.
    return (child_table[VAddrIndex(vaddr, 1)] & TAB_ADDR_MASK);
}

void PageMap::DeepCopy(uint64_t *table, uint64_t *copy, size_t level)
{
    if(level == 1) {
        memmove(copy, table, FRAME_SIZE);
        return;
    }

    for(size_t i = 0; i < FRAME_SIZE / sizeof(uint64_t); ++i) {
        if(table[i] & PRESENT) {
            uint16_t flags = table[i] & ~(TAB_ADDR_MASK);
            uint64_t table_addr = ToHighMem(table[i] & TAB_ADDR_MASK);
            uint64_t copy_addr = (uint64_t) BuddyAllocator::Allocate(FRAME_SIZE);
            copy[i] = copy_addr | flags;

            DeepCopy((uint64_t *) table_addr, (uint64_t *) ToHighMem(copy_addr),
                     level - 1);
        }
    }
}

void PageMap::DeepFree(uint64_t *table, size_t level)
{
    if(level > 1) {
        for (size_t i = 0; i < FRAME_SIZE / sizeof(uint64_t); ++i) {
            if (table[i] & PRESENT) {
                uint64_t table_addr = table[i] & TAB_ADDR_MASK;
                DeepFree((uint64_t *) ToHighMem(table_addr), level - 1);
            }
        }
    }
    BuddyAllocator::Free((char*) table - KERNEL_DATA_BASE);
}

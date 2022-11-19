#ifndef PAGE_MAP_H
#define PAGE_MAP_H

#include <cstddef>
#include <cstdint>

struct AddrRange {
    uint64_t base_, bound_;
};

const static uint16_t PRESENT				=		(1);
const static uint16_t READ_WRITABLE			=	    (1 << 1);
const static uint16_t USER_ACCESSIBLE		=		(1 << 2);
const static uint16_t WRITE_THROUGH			=	    (1 << 3);
const static uint16_t CACHE_DISABLED		=		(1 << 4);
const static uint16_t ACCESSED				=	    (1 << 5);
const static uint16_t DIRTY					=	    (1 << 6);
const static uint16_t PAGE_ATTRIBUTE_TABLE	=	    (1 << 7);
const static uint16_t GLOBAL				=		(1 << 8);
const static uint64_t EXECUTABLE			=	    (~(1UL << 62));

const static uint16_t KERNEL_PAGE           =       (PRESENT | READ_WRITABLE);
const static uint16_t USER_PAGE             =       (PRESENT | READ_WRITABLE |
                                                     USER_ACCESSIBLE);

const static uint64_t KERNEL_DATA_BASE      =       0xFFFF800000000000;
const static uint64_t KERN_DIRECT_MAP_SIZE  =       0x100000000;

uint64_t ToHighMem(uint64_t paddr);
void *ToHighMem(void *mem);
uint64_t FromHighMem(uint64_t vaddr);
void *FromHighMem(void *mem);

class PageMap {
public:
    /**
     * Default constructor - allocate space for the PML4 table root.
     */
    PageMap();

    /**
     * Constructor for kernel pagemap. This reconstructs the stivale2 kernel
     * mappings as given by the memmap and protected memory range structures.
     * It maps paddrs 0x1000-4GiB to 0xFFFF800000000000. It additionally maps
     * @param memmap A memory map giving ranges of memory and their uses as
     *               established by the bootloader.
     * @param kern_base_addr Gives the base physical and virtual addresses at
     *                       which kernel code/data are mapped.
     * @param pmrs Protected memory ranges - ELF segments where we need to be
     *             extra careful about the flags we set.
     */
    PageMap(struct stivale2_struct_tag_memmap *memmap,
            struct stivale2_struct_tag_kernel_base_address *kern_base_addr,
            struct stivale2_struct_tag_pmrs *pmrs);

    /**
     * Traverse the entire table, free the whole hierarchy.
     */
    ~PageMap();

    /**
     * Deep-copy the entire table from rhs.
     * @param rhs PageMap to copy from.
     */
    PageMap(const PageMap &rhs);

    PageMap &operator=(const PageMap &rhs);

    bool Map(uint64_t paddr, uint64_t vaddr, uint16_t flags=KERNEL_PAGE);

    bool MapRange(const AddrRange &phys_range, size_t virt_offset,
                  uint16_t flags=KERNEL_PAGE);

    bool Remap(uint64_t vaddr, uint64_t new_vaddr, uint16_t flags=KERNEL_PAGE);

    bool RemapRange(const AddrRange &vaddr_range, uint64_t new_vaddr_base,
                    uint16_t flags=KERNEL_PAGE);

    bool Unmap(uint64_t vaddr);

    bool UnmapRange(const AddrRange &vaddr_range);

    uint64_t VAddrToPAddr(uint64_t vaddr);

    void Load();

private:
    // 512 entries per table, of 4KiB pages each.
    const static size_t LOG2_ENTRIES_PER_TABLE	=	9;
    const static size_t LOG2_FRAME_SIZE			=	12;
    const static size_t MAX_PAGE_IND			=	0x1FF;
    const static size_t FRAME_SIZE              =   0x1000;
    const static size_t TAB_ADDR_MASK           =   ~(0x1FF);

    uint64_t *root_;
    uint64_t vmem_direct_mapping_base_;

    /* Find the index of the page table which is the ancestor of the given vaddr
     * at the given level (beginning at 1) of the page table hierarchy.
     * If we want to map virtual address n, we need to figure out where in the
     * table tree n resides. If each table holds 512 entries and leaves hold
     * 4KiB pages, then the entry pointing to n will be
     * (n / (4KiB * 512^(m - 1))) % 512 at each level m. This is because each
     * leaf entry holds 4KiB of physical memory, and each table contains 512
     * pointers to lower levels. Thus, a level 2 table will contain 512^(2-1) =
     * 512 pointers to 4KiB pages; a level 3 table will contain 512 pointers to
     * 512 level 2 tables, each of which contains 512 4KiB pages, equivalent to
     * 512^(3-1) = 512*512 4KiB pages in total; and so on. This macro expression
     * is equivalent; we simply replace division with a right shift (since we'll
     * inevitably be dividing by some power of 2) and modulo with bitwise and
     * (since x % y = x % (y-1)).
     *
     * @input vaddr The virtual address whose index will be returned.
     * @input level The level of the page table for which we return the index.
     * @output The index of the ancestor of the page table of the given vaddr
     *         at the given level.
    */
    static inline size_t VAddrIndex(uint64_t vaddr, uint8_t level);

    /**
     * Given a page table entry, retrieve whether or not a given flag is set.
     * @input page An entry from a bottom-level page table.
     * @input flag The value of the flag when set. E.g. (1 << 1) for present
     * 			   (1 << 2) for read/writable. For simplicity's sake, just use
     * 			   flag macros from top of file. E.g. GetPageFlag(page, PRESENT)
     * @output True if flag is set, false otherwise.
     */
    static bool GetPageFlag(uint64_t page, uint64_t flag);

    /**
     * Given a page table and an index, return the page table pointed to by the
     * index-th entry if it exists, otherwise return NULL.
     * @input parent The parent of the page table to retrieve.
     * @input index The index of the page table to retrieve.
     * @output A pointer to the page table if it exists, otherwise NULL.
     */
    static inline uint64_t *GetPageTable(uint64_t *parent, uint64_t index);

    /**
     * Look up the index-th entry of nth-level page table parent. If this entry has
     * been set, return the pointer to the corresponding (n+1)th level page table.
     * If not, allocate the (n+1)th level page table, create an entry for it, and
     * return a ptr to it.
     * @input parent The parent of the page table to create/retrieve.
     * @input index The index of the page table to lookup.
     * @input flags The flags to be set for this page table if it must be created.
     * @output A pointer to the page table if PMM allocation succeeded, NULL
     * 		   otherwise.
     */
    static inline uint64_t *GetOrCreatePageTable(uint64_t* parent, uint64_t index,
                                                 uint16_t flags);

    /**
     * Given a PML4 table and a virtual address, return a pointer to the page table
     * entry corresponding to this address.
     * @input page_table_root Ptr to PML4 table.
     * @input vaddr The virtual address to lookup.
     * @input create Should the entry be created if it does not exist (t/f)?
     * @output A pointer to the virtual address' entry in the bottom-level page
     * 		   table.
     */
    static uint64_t *GetPage(uint64_t *page_table_root, uint64_t vaddr);

    /**
     * Given a PML4 and a virtual address, create a page table entry for said vaddr
     * and retur a ptr to it.
     * @input page_table_root The PML4 to query/edit.
     * @output Ptr to created page table entry if PMM alloc succeeded, NULL
     * 		   otherwise.
     */
    static uint64_t *CreatePage(uint64_t *page_table_root, uint64_t vaddr,
                                uint16_t flags);

    /**
     * Retrieve the physical address corresponding to some virtual address from a
     * page table.
     * @input table The PML4 from which we will retrieve the physical address.
     * @input vaddr The virtual address whose corresponding physical addr will be
     * 				returned.
     * @output The physical address corresponding to vaddr.
     */
    uint64_t VAddrToPAddr(uint64_t *table, uint64_t vaddr);

    static void DeepCopy(uint64_t *table, uint64_t *copy, size_t level);

    static void DeepFree(uint64_t *table, size_t level);
};


#endif

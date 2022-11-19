#include "buddy_allocator.h"
#include "sys/log.h"
#include "libc/string.h"

namespace BuddyAllocator {

// A struct tracking the status of a page. Each page will be assigned such
// an entry; forward_ and backward_ pointers allow this to struct be placed
// in a freelist. The order_ field gives log2([NUM PAGES IN ENTRY]). The
// entry also maintains info about whether or not this page is currently
// free.
struct FreeListEntry {
    FreeListEntry *forward_, *backward_;
    uint8_t order_;
    bool free_;
};

// A range of memory that has been entrusted to this allocator. Some segment
// of memory at the start of each range will be reserved for metadata
// regarding the set of pages within this range; this range will have size
// mdata_size_ and begin at mdata_base_, which will be the starting address
// of the array of free list entries (page_entries_). The nth page of this
// range should be present at page_entries_[n], allowing us to determine the
// status of a given page in O(1) time.
struct MemRange {
    uintptr_t mdata_base_, base_;
    size_t mdata_size_, size_;
    FreeListEntry *page_entries_;
};

// Max order is 40, allows 1 TiB total. Min order is log2(MIN_ALLOCATION).
const static uint8_t MAX_ORDER = 40;
const static uint8_t MIN_ORDER = 12;
const static size_t MIN_ALLOCATION = 0x1000;

// mem_ranges_ is an array of contiguous ranges of usable memory.
static MemRange *mem_ranges_;
static size_t num_ranges_, num_blocks_;
// free_lists_[n] gives the head of a linked list of free list entries referring
// to blocks of size 2^n bytes.
static FreeListEntry *free_lists_[MAX_ORDER-MIN_ORDER] = {nullptr};

static size_t mem_in_use_, total_mem_;

// Tracks whether the buddy allocator has been initialized.
static bool initialized_;

/**
 * Since the placement of free list entries is deterministic based on their
 * addresses, we can calculate the address corresponding to a free list
 * entry without needing to store that information within the free list
 * entry itself (and thereby saving space).
 * @param entry A pointer to the free list entry. Since this method and
 *              several others perform pointer arithmetic on the free list
 *              entry, it is imperative that the free list entry not be
 *              copied and must be a pointer to an entry in one of the
 *              memory regions'  page_entries_ field.
 * @return The starting address of the page to which this free list entry
 *         refers.
 */
static uintptr_t EntryToAddr(FreeListEntry *entry);

/**
 * This method performs to opposite translation to the above method. Given
 * an address, determine which memory range it resides in and subsequently
 * return range->page_entries_[(addr - range->base_) / PAGE_SIZE].
 * @param addr The address of an
 * @return A pointer to the freelist entry corresponding to this page; this
 *         pointer points to an entry in one of the memory ranges'
 *         page_entries_ fields. To reiterate, it is imperative that the
 *         program deal only with these specific entries and not with
 *         copies, since their location in memory allows us to deduce the
 *         addresses corresponding to entries without maintaining that as
 *         a field in the freelist entries.
 */
static FreeListEntry *AddrToEntry(uintptr_t addr);

/**
 * Given a stivale MMAP range of usable memory, add this to the list of
 * memory ranges, construct metadata documenting the pages of this range
 * (and potentially regarding ranges of memory).
 * @param range A stivale2 memmap entry identifying the base and length of
 *              some usable memory region.
 * @param init_mdata_size One block of memory will be used to store metadata
 *                        about the ranges themselves in addition to the
 *                        metadata about the range's pages. If the given
 *                        range has been selected for this purpose, then the
 *                        required space to store metadata about the memory
 *                        ranges should be given as init_mdata_size;
 *                        otherwise, this parameter should be zero.
 */
static void CatalogRange(const stivale2_mmap_entry &range,
                         size_t init_mdata_size);

/**
 * Find the order-n buddy of a given address. Essentially, flip the
 * (order)th bit of the address' offset_ from the start of its range. I.e.
 * the address of the buddy should be (addr - range->base_ ^ (1 << order)).
 * If this resulting address is valid (i.e. within the bounds of the range),
 * return the freelist entry corresponding to it. If it is not (which
 * indicates that no buddy exists for the given entry), return nullptr.
 * @param addr The address for which you wish to find the buddy.
 * @param order The order of the buddy we wish to find.
 * @return The freelist entry corresponding to the given address' buddy,
 *         if one exists (else nullptr).
 */
static FreeListEntry *BuddyOf(uintptr_t addr, uint8_t order);

/**
 * Given a freelist entry of size 2^order, split into 2 freelist entries of
 * size 2^(order-1), mark the lower one as used, and return the lower one.
 * @param entry The entry to split.
 * @param order The current order of the entry.
 * @return An order order-1 entry formed by splitting the given entry.
 */
static FreeListEntry *Split(FreeListEntry *entry, uint8_t order);

/**
 * Merge two freelist entries of order n into a single entry of order n+1.
 * @param buddy1 The first of the two buddies to merge.
 * @param buddy2 The second of the two buddies to merge.
 * @return The merged entry; this will be the freelist entry corresponding
 *         to whichever of the buddies is lower in memory, but with its
 *         order incremented.
 */
static FreeListEntry *Merge(FreeListEntry *buddy1, FreeListEntry *buddy2);

/**
 * Give the index of the memory range (in member array mem_ranges_) to which
 * a given address belongs. If none exists, return -1.
 * @param addr An address in memory.
 * @return The index of the memory range to which this address belongs, else
 *         -1.
 */
static int EntryRange(uintptr_t addr);

/**
 * Same as above function, but with a freelist entry instead of an address.
 * @param entry Freelist entry.
 * @return The memory range to which this entry corresponds.
 */
static int EntryRange(FreeListEntry *entry);

/**
 * Can two blocks be merged? I.e. Do they have the same order, are both
 * free, and would the result of merging the two entries produce an entry
 * larger than the memory range to which these buddies belong?
 * @param entry A freelist entry.
 * @param buddy The freelist entry of the above entry's buddy.
 * @return Whether or not these two regions of memory can be merged.
 */
static bool Mergeable(FreeListEntry *entry, FreeListEntry *buddy);

/**
 * Push to an entry to the front of the freelist of a given order.
 * @param order The order of the freelist to which this entry should be
 *              pushed.
 * @param entry The entry to push to the freelist.
 */
static void PushFront(uint8_t order, FreeListEntry *entry);

/**
 * Pop the entry at the head of the freelist of the given order.
 * @param order The order of the freelist from which to pop.
 * @return The (former) head of that freelist.
 */
static FreeListEntry *PopFront(uint8_t order);

/**
 * Remove the given entry from the freelist which it currently resides in.
 * @param entry The entry to remove.
 */
static void Remove(FreeListEntry *entry);

/**
 * Compute ceil(log2(size)).
 * @param size Some positive number.
 * @return ceil(log2(size)).
 */
static inline uint8_t CeilLog2(size_t size);

/**
 * Round to the nearest multiple of MIN_ALLOCATION.
 * @param size Some positive number, representing a value in bytes.
 * @return The given size rounded to the nearest multiple of the size of a
 *         page.
 */
static inline size_t RoundUp(size_t size);

void InitBuddyAllocator(const struct stivale2_struct_tag_memmap &memmap)
{
    // Set all static variables to appropriate values.
    initialized_ = false;
    num_blocks_ = 0;
    num_ranges_ = 0;
    mem_ranges_ = nullptr;
    mem_in_use_ = 0;
    total_mem_ = 0;

    // Count number of usable memory ranges in the computer's memory map.
    size_t num_usable = 0;
    for (size_t i = 0; i < memmap.entries; ++i) {
        if (memmap.memmap[i].type == STIVALE2_MMAP_USABLE) {
            ++num_usable;
        }
    }

    // One of these usable ranges should be used to store the list of ranges.
    // Additionally, every range must store metadata about the pages it contains
    // at the start of its range. Find the first range which is sufficiently
    // large to store both of these items, and set selected_range equal to the
    // index of that range in the stivale memmap.
    size_t memrange_arr_size = sizeof(MemRange) * num_usable;
    int selected_range = -1;
    for (size_t i = 0; i < memmap.entries; ++i) {
        size_t mdata_size =
                memrange_arr_size + CeilLog2(memmap.memmap[i].length);

        if (memmap.memmap[i].type == STIVALE2_MMAP_USABLE &&
            memmap.memmap[i].length > mdata_size)
        {
            mem_ranges_ = (MemRange *) memmap.memmap[i].base;
            selected_range = i;
            break;
        }
    }

    // If no such range exists which is sufficiently sized to allocate a
    if (selected_range == -1) {
        return;
    }

    // Now that we have found a location for the array of memory ranges,
    // populate that array with entries for each usable memory range.
    for (size_t i = 0; i < memmap.entries; ++i) {
        if (memmap.memmap[i].type == STIVALE2_MMAP_USABLE) {
            Log("USABLE MEM RANGE FROM 0x%x-0x%x\n", memmap.memmap[i].base,
                memmap.memmap[i].base + memmap.memmap[i].length);
            size_t init_mdata_size =
                    (selected_range == (int) i) ? memrange_arr_size : 0;
            CatalogRange(memmap.memmap[i], init_mdata_size);
            total_mem_ += memmap.memmap[i].length;
        }
    }

    initialized_ = true;
}

void *Allocate(size_t size)
{
    if (! initialized_) {
        return nullptr;
    }

    uint8_t order = CeilLog2(size);
    uint8_t block_order = order > MIN_ORDER ? order : MIN_ORDER;
    FreeListEntry *entry = nullptr;
    num_blocks_ -= (1 << (block_order - MIN_ORDER));

    // Find first block with size greater than requested, pop from free list.
    for (order = block_order; order < MAX_ORDER; ++order) {
        if (free_lists_[order - MIN_ORDER]) {
            entry = PopFront(order);
            break;
        }
    }

    if (!entry) {
        return nullptr;
    }

    // Split block until its size is equivalent to request rounded to nearest
    // 0x1000.
    for (; order > block_order; --order) {
        entry = Split(entry, order);
        entry->free_ = false;
    }

    entry->free_ = false;
    entry->order_ = block_order;

    mem_in_use_ += (1 << entry->order_);
    return (void *) EntryToAddr(entry);
}

void *Realloc(void *allocation, size_t size)
{
    if (!initialized_ || !allocation) {
        return nullptr;
    }

    uintptr_t alloc_addr = (uintptr_t) allocation;
    FreeListEntry *alloc_entry = AddrToEntry(alloc_addr);

    // Buddy allocators have a lot of internal fragmentation; there's
    // some space at the end of each allocation to ensure that the
    // allocation consists of 2^n blocks; if the requested new size can
    // fit in the already-allocation block, then there's no need to copy
    // data to a new allocation.
    size_t alloc_size = (1 << alloc_entry->order_);
    if (size < alloc_size) {
        mem_in_use_ += alloc_size;
        return allocation;
    }

    void *new_alloc = Allocate(size);
    // THis memset somehow fucks everything up.
    memset(new_alloc, 0, size);
    memcpy(new_alloc, allocation, alloc_size);
    Free(allocation);
    return new_alloc;
}


void Free(void *allocation)
{
    if (!initialized_ || !allocation) {
        return;
    }

    uintptr_t alloc_addr = (uintptr_t) allocation;
    FreeListEntry *alloc_entry = AddrToEntry(alloc_addr);
    num_blocks_ += (1 << alloc_entry->order_);

    FreeListEntry *buddy_entry = BuddyOf(alloc_addr, alloc_entry->order_);

    // If the allocation's buddy isn't free (or the allocation doesn't have a
    // buddy), then simply add the allocation to the front of its corresponding
    // freelist and mark it as free.
    if (!Mergeable(alloc_entry, buddy_entry)) {
        alloc_entry->free_ = true;
        PushFront(alloc_entry->order_, alloc_entry);
    }

    // If the allocation's buddy is free, then merge with its buddy; then see
    // if the resulting merged block can be merged with its buddy; and so forth
    // until no further merging is possible.
    while (Mergeable(alloc_entry, buddy_entry)) {
        alloc_entry->free_ = true;
        alloc_entry = Merge(alloc_entry, buddy_entry);
        alloc_addr = EntryToAddr(alloc_entry);
        buddy_entry = BuddyOf(alloc_addr, alloc_entry->order_);
    }

    mem_in_use_ -= (1 << alloc_entry->order_);
}


size_t MemInUse()
{
    return mem_in_use_;
}

size_t MemFree()
{
    return total_mem_ - mem_in_use_;
}

size_t TotalMem()
{
    return total_mem_;
}

bool MemCritical()
{
    return mem_in_use_ >= total_mem_ / 2;
}

void Print()
{
    size_t mem = 0;
    for (uint8_t order = MIN_ORDER; order < MAX_ORDER; ++order) {
        if (free_lists_[order - MIN_ORDER]) {
            size_t num_entries = 0;
            FreeListEntry *curr = free_lists_[order - MIN_ORDER];
            do {
                ++num_entries;
            } while ((curr = curr->forward_));
            mem += num_entries * (1 << order);
            Log("\t%d blocks of size %d\n", num_entries, (1 << order));
        }
    }
    Log("TOTAL MEM: %d blocks\n", mem / MIN_ALLOCATION);
}

size_t NumBlocks()
{
    return num_blocks_;
}

static uintptr_t EntryToAddr(FreeListEntry *entry)
{
    // The free list entry for a given address in range r is stored at
    // r->mdata_base_ + ((entry - r->base_) / (MIN_ALLOCATION)) *
    // sizeof(FreeListEntry). I.e. The nth page of r has an entry at
    // r->page_entries_[n].
    int mem_range_ind = EntryRange(entry);
    if (mem_range_ind == -1) {
        return 0;
    }

    size_t ind =
            ((uintptr_t) entry - mem_ranges_[mem_range_ind].mdata_base_) /
            sizeof(FreeListEntry);

    return ind * MIN_ALLOCATION + mem_ranges_[mem_range_ind].base_;
}

static FreeListEntry *AddrToEntry(uintptr_t addr)
{
    int mem_range_ind = EntryRange(addr);
    if (mem_range_ind == -1) {
        return nullptr;
    }

    MemRange *mem_range = &mem_ranges_[mem_range_ind];
    size_t ind =
            (addr / MIN_ALLOCATION) - (mem_range->base_ / MIN_ALLOCATION);
    return &(mem_range->page_entries_[ind]);
}

static void CatalogRange(const stivale2_mmap_entry &range,
                         size_t init_mdata_size)
{
    size_t num_page_structs = range.length / MIN_ALLOCATION;
    MemRange *range_entry = &mem_ranges_[num_ranges_];
    ++num_ranges_;

    // Each page in the memory range has to have an entry in the range's
    // metadata section. Determine the size of that metadata section.
    range_entry->mdata_size_ = RoundUp(init_mdata_size);
    range_entry->mdata_size_ +=
            RoundUp(num_page_structs * sizeof(FreeListEntry));
    range_entry->size_ = range.length - range_entry->mdata_size_;

    range_entry->mdata_base_ = range.base + init_mdata_size;
    range_entry->base_ = range.base + range_entry->mdata_size_;

    // The page_entries_ array should begin at mdata_base_.
    range_entry->page_entries_ = (FreeListEntry *) range_entry->mdata_base_;

    size_t region_size = range_entry->size_;
    size_t region_base = range_entry->base_;
    size_t region_bound = region_base + region_size;

    // For each page in the memory range, make an entry in page_entries_.
    for (size_t j = region_base; j < region_bound; j += MIN_ALLOCATION) {
        size_t entry_ind = (j - range.base) / MIN_ALLOCATION;
        range_entry->page_entries_[entry_ind] =
                (FreeListEntry){nullptr, nullptr, MIN_ORDER, true};
    }

    // Divide the region into entries consisting of powers of 2 that are greater
    // than the size of a page (MIN_ALLOCATION).
    for (size_t j = CeilLog2(region_size) - 1; region_size > MIN_ALLOCATION;
         --j)
    {
        if (region_size >= (1ULL << j)) {
            num_blocks_ += (1 << (j - MIN_ORDER));
            PushFront(j, AddrToEntry(region_base));
            region_size -= (1 << j);
            region_base += (1 << j);
        }
    }
}

static FreeListEntry *BuddyOf(uintptr_t addr, uint8_t order)
{
    int range_index = EntryRange(addr);
    if (range_index == -1) {
        return nullptr;
    }

    uintptr_t base = mem_ranges_[range_index].base_;
    uintptr_t buddy_addr = ((addr - base) ^ (1 << (order))) + base;
    return AddrToEntry(buddy_addr);
}

static FreeListEntry *Split(FreeListEntry *entry, uint8_t order)
{
    uintptr_t entry_addr = EntryToAddr(entry);
    FreeListEntry *buddy_entry = BuddyOf(entry_addr, order - 1);

    Remove(buddy_entry);
    Remove(entry);

    entry->order_ = order - 1;
    buddy_entry->order_ = order - 1;
    entry->free_ = true;
    PushFront(order - 1, entry);

    return buddy_entry;
}

static FreeListEntry *Merge(FreeListEntry *buddy1, FreeListEntry *buddy2)
{
    FreeListEntry *base_buddy =
            (uintptr_t) buddy1 < (uintptr_t) buddy2 ? buddy1 : buddy2;
    FreeListEntry *upper_buddy =
            (uintptr_t) buddy1 < (uintptr_t) buddy2 ? buddy2 : buddy1;

    Remove(base_buddy);
    Remove(upper_buddy);
    PushFront(++base_buddy->order_, base_buddy);

    return base_buddy;
}

static int EntryRange(uintptr_t addr)
{
    for (size_t i = 0; i < num_ranges_; ++i) {
        size_t base = mem_ranges_[i].base_;
        size_t bound = mem_ranges_[i].base_ + mem_ranges_[i].size_;

        if (base <= addr && addr <= bound) {
            return i;
        }
    }

    return -1;
}

static int EntryRange(FreeListEntry *entry)
{
    uintptr_t addr = (uintptr_t) entry;
    for (size_t i = 0; i < num_ranges_; ++i) {
        size_t base = mem_ranges_[i].mdata_base_;
        size_t bound = mem_ranges_[i].mdata_base_ + mem_ranges_[i].mdata_size_;

        if (base <= addr && addr <= bound) {
            return i;
        }
    }

    return -1;
}

static bool Mergeable(FreeListEntry *entry, FreeListEntry *buddy)
{
    if (EntryRange(EntryToAddr(buddy)) == -1) {
        return false;
    }

    return buddy->free_ && buddy->order_ == entry->order_;
}

static void PushFront(uint8_t order, FreeListEntry *entry)
{
    // Linked list insertion.
    entry->order_ = order;
    entry->forward_ = free_lists_[order - MIN_ORDER];
    if (free_lists_[order - MIN_ORDER]) {
        free_lists_[order - MIN_ORDER]->backward_ = entry;
    }
    free_lists_[order - MIN_ORDER] = entry;
}

static FreeListEntry *PopFront(uint8_t order)
{
    // Linked list pop.
    FreeListEntry *head = free_lists_[order - MIN_ORDER];
    free_lists_[order - MIN_ORDER] = head->forward_;
    if (free_lists_[order - MIN_ORDER]) {
        free_lists_[order - MIN_ORDER]->backward_ = nullptr;
    }
    return head;
}

static void Remove(FreeListEntry *entry)
{
    // Standard linked list removal.
    if (entry->forward_) {
        entry->forward_->backward_ = entry->backward_;
    }

    if (entry->backward_) {
        entry->backward_->forward_ = entry->forward_;
    }

    if (entry->order_ >= MIN_ORDER &&
        free_lists_[entry->order_ - MIN_ORDER] == entry)
    {
        free_lists_[entry->order_ - MIN_ORDER] = entry->forward_;
    }

    entry->forward_ = nullptr;
    entry->backward_ = nullptr;
}

static uint8_t CeilLog2(size_t size)
{
    uint8_t i;
    for (i = 0; (1ULL << i) < size; ++i);
    return i;
}

static size_t RoundUp(size_t size)
{
    return ((size + MIN_ALLOCATION - 1) / MIN_ALLOCATION) * MIN_ALLOCATION;
}

}
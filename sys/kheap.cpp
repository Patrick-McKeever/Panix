#include "kheap.h"
#include "sys/log.h"
#include <ds/cache.h>
#include "libc/string.h"

namespace KHeap {
namespace {
struct FreeListEntry {
    FreeListEntry *forward_, *backward_;
    size_t size_, prev_size_;
};

// Final bit of size in FreeListEntry denotes whether or not chunk is free
// (0) or in use (1). Since we align to 32 bytes, we can actually use first
// five bits; just adjust SIZE_MASK accordingly.
const uint8_t IN_USE = 1;
const uint64_t SIZE_MASK = ~(0b11111);

// First 32 bins (under 1024 bytes) are spaced 32 bytes apart. Past that, we
// have 4 bins between every power of 2. Going up to 64 bins, that allows us
// to store chunks of at most 256 KiB. Beyond this, we use the PMM to request
// more memory.
const size_t NUM_BINS = 256;
const size_t NUM_BMAP_ENTRIES = NUM_BINS / 64;
const size_t PMM_THRESHOLD = 0x4000;
const uint64_t HEAP_BASE = KERNEL_DATA_BASE + KERN_DIRECT_MAP_SIZE;

FreeListEntry *free_lists_[NUM_BINS] = {nullptr};
uint64_t free_lists_bitmap_;

PageMap *page_map_;
size_t heap_size_;
size_t max_heap_size_;
void *heap_;
uintptr_t heap_paddr_;
FreeListEntry *top_;


uint8_t BinIndex(size_t size)
{
    if (size < 1024) {
        return size / 32;
    }

    // Recall that we have 2 bins for each power of 2 past 1024 bytes; so,
    // find the most significant set bit, multiply by two, add the
    // subsequent (smaller) bit.
    static const uint8_t log2_min_size = 11;
    uint8_t msb_index = __builtin_clzl(size);
    uint8_t floor_log2_size = sizeof(size_t) * 8 - msb_index;
    uint8_t index = (floor_log2_size - log2_min_size) * 2;
    index += ((size >> (floor_log2_size - 2)) & 1);
    //index += ((size >> (floor_log2_size - 3)) & 1);
    return 32 + index;
}

FreeListEntry *PopFront(uint8_t ind)
{
    FreeListEntry *entry = free_lists_[ind];
    if (entry) {
        free_lists_[ind] = entry->forward_;
        if (free_lists_[ind]) {
            free_lists_[ind]->backward_ = nullptr;
        } else {
            free_lists_bitmap_ &= ~(1ULL << ind);
        }

        entry->forward_ = entry->backward_ = nullptr;
        return entry;
    }

    return nullptr;
}

void Remove(FreeListEntry *entry)
{
    if (entry->forward_) {
        entry->forward_->backward_ = entry->backward_;
    }

    if (entry->backward_) {
        entry->backward_->forward_ = entry->forward_;
    }

    uint8_t entry_bin = BinIndex(entry->size_ & SIZE_MASK);
    if (free_lists_[entry_bin] == entry) {
        PopFront(entry_bin);
    }

    entry->forward_ = entry->backward_ = nullptr;
}

FreeListEntry *FindEntry(size_t size)
{
    uint8_t bin_ind = BinIndex(size);
    size_t free_list_mask = free_lists_bitmap_ & -(1ULL << bin_ind);
    while(free_list_mask) {
        uint8_t smallest_sufficient_bin = 63 - __builtin_clzl(free_list_mask);
        FreeListEntry *entry = free_lists_[smallest_sufficient_bin];
        while(entry) {
            if(entry->size_ > size) {
                Remove(entry);
                return entry;
            }
            entry = entry->forward_;
        }
        ++bin_ind;
        free_list_mask &= ~(1ULL << bin_ind);
    }

    return nullptr;
}

void PushFront(uint8_t ind, FreeListEntry *entry)
{
    entry->forward_ = free_lists_[ind];
    if((uintptr_t) free_lists_[ind] == 1) {
        Log("Issue in push front (ind %d)\n", ind);
    }
    if (entry->forward_) {
        entry->forward_->backward_ = entry;
    }
    free_lists_[ind] = entry;
    free_lists_bitmap_ |= (1ULL << ind);
}

FreeListEntry *NextChunk(FreeListEntry *chunk)
{
    return (FreeListEntry *) ((char *) chunk + (chunk->size_ & SIZE_MASK));
}

FreeListEntry *PrevChunk(FreeListEntry *chunk)
{
    return (FreeListEntry *) ((char *) chunk - (chunk->prev_size_ & SIZE_MASK));
}


FreeListEntry *Split(FreeListEntry *entry, size_t size)
{
    size_t old_size = entry->size_ & SIZE_MASK;
    char *raw_mem = (char *) entry;
    auto *next = (FreeListEntry *) (raw_mem + size);
    next->forward_ = next->backward_ = nullptr;
    entry->size_ = size | IN_USE;
    next->size_ = (old_size - size) & ~(IN_USE);
    next->prev_size_ = entry->size_;
    if (entry == top_) {
        top_ = next;
    } else {
        FreeListEntry *after_next = NextChunk(next);
        after_next->prev_size_ = next->size_;
    }
    return next;
}

void SplitAndPush(FreeListEntry *entry, size_t size)
{
    FreeListEntry *next = Split(entry, size);
    if(next != top_) {
        uint8_t next_bin = BinIndex(next->size_);
        PushFront(next_bin, next);
    }
}

FreeListEntry *MergeWithNeighbors(FreeListEntry *entry)
{
    // size_t old_size = entry->size_;
    if (!(entry->prev_size_ & IN_USE) && entry->prev_size_ > 0) {
        size_t current_size = entry->size_;
        FreeListEntry *prev = PrevChunk(entry);
        Remove(prev);
        entry = prev;
        entry->size_ += current_size;
    }

    FreeListEntry *next = NextChunk(entry);
    if (!(next->size_ & IN_USE)) {
        Remove(next);
        entry->size_ += next->size_ & SIZE_MASK;
        if (next == top_) {
            top_ = entry;
        }
    }

    if(entry != top_) {
        next             = NextChunk(entry);
        next->prev_size_ = entry->size_;
    }

    return entry;
}

void GrowHeap()
{
    page_map_->UnmapRange({(uintptr_t) heap_, (uintptr_t) heap_ + heap_size_});

    if(BuddyAllocator::MemInUse() < max_heap_size_) {
        heap_size_ = heap_size_ * 2 > max_heap_size_ ?
                          max_heap_size_ : heap_size_ * 2;
        heap_paddr_ = (uintptr_t) BuddyAllocator::Realloc((void*) heap_paddr_,
                                                          heap_size_);

        page_map_->MapRange({ heap_paddr_, heap_paddr_ + heap_size_ },
                            HEAP_BASE - heap_paddr_);
        page_map_->Load();
        top_->size_ += heap_size_ / 2;
    }
}

}
}

void KHeap::Init(size_t initial_size, PageMap *page_map, size_t max_heap_size)
{
    // Smallest possible allocation is 32 bits, so heap size should be a
    // multiple of 32; this also lets us use the final 5 bits of addresses
    // to store metadata. (Currently, only the final bit is used, in order to
    // denote whether a block is in use or free.)
    initial_size = ((initial_size + 32 - 1) / 32) * 32;
    max_heap_size_ = max_heap_size;

    free_lists_bitmap_ = 0;
    heap_size_ = initial_size;
    heap_ = (void*) HEAP_BASE;
    top_ = (FreeListEntry*) heap_;
    page_map_ = page_map;
    heap_paddr_ = (uintptr_t) BuddyAllocator::Allocate(heap_size_);

    page_map->MapRange({ heap_paddr_, heap_paddr_ + heap_size_ },
                        HEAP_BASE - heap_paddr_);
    page_map->Load();
    top_->size_ = heap_size_;
    top_->forward_ = top_->backward_ = nullptr;
    top_->prev_size_ = 0;
}

void *KHeap::Allocate(size_t size)
{
    if(! heap_ || ! size) {
        return nullptr;
    }

    Verify();

    FreeListEntry *entry = nullptr;
    // Account for necessary metadata, round up to nearest multiple of 32 if
    // less than 1024 bytes.
    size += sizeof(FreeListEntry);
    size = ((size + 32 - 1) / 32) * 32;

    if(size >= PMM_THRESHOLD) {
        entry = (FreeListEntry *) BuddyAllocator::Allocate(size);
        entry->size_ = size | IN_USE;
        return ToHighMem(entry + 1);
    }

    // Now, find the first free list containing blocks larger than or
    // equal to the required size. This bit hack sets all bits less than
    // the bin index to 0 and and-s all indices greater than this. Then
    // trim down to
    if((entry = FindEntry(size))) {
        size_t entry_size = entry->size_ & SIZE_MASK;
        if(size >= 1024 && entry_size > size) {
            SplitAndPush(entry, size);
        }
    }

    // If no free list entries are sufficiently-sized, shave some memory
    // from the top chunk. If top chunk isn't large enough, grow the
    // heap.
    else {
        while(size >= top_->size_) {
            GrowHeap();
        }
        entry = top_;
        top_ = Split(top_, size);
    }

    FreeListEntry *next = NextChunk(entry);
    entry->size_ = entry->size_ | IN_USE;
    next->prev_size_ = entry->size_;

    Verify();
    return (void*) (entry + 1);
}

void *KHeap::Reallocate(void *allocation, size_t size)
{
    if (!heap_ || !size) {
        return nullptr;
    }

    Verify();

    size += sizeof(FreeListEntry);
    size = ((size + 32 - 1) / 32) * 32;

    auto *entry = (FreeListEntry * )
            ((char *) allocation - sizeof(FreeListEntry));
    size_t original_size = entry->size_;

    if(! (original_size & IN_USE)) {
        Log("[WARNING] Attempting to reallocate non-allocated memory.\n");
        return nullptr;
    }

    if (original_size >= PMM_THRESHOLD) {
        auto *new_alloc = (FreeListEntry*)
            BuddyAllocator::Realloc(ToPAddr(entry), size);
        new_alloc->size_ = size & IN_USE;
        return (void*) (new_alloc + 1);
    } else if (size >= PMM_THRESHOLD) {
        auto *new_allocation = (FreeListEntry *) BuddyAllocator::Allocate(size);
        memcpy(new_allocation, entry, size);
        new_allocation->size_ = size;
        Free(allocation);
        return (void *) (new_allocation + 1);
    }


    // Merge with neighbors. Note that, even if the resulting chunk is not
    // large enough, we'd just end up calling free, which would merge the
    // chunk anyway, so this is useful in either case.
    entry = MergeWithNeighbors(entry);
    // if ((entry->size_ & SIZE_MASK) > size) {
    //     SplitAndPush(entry, size);
    // }
    // if ((entry->size_ & SIZE_MASK) >= size) {
    //     memcpy((entry + 1), allocation, original_size & SIZE_MASK);
    //     Verify();
    //     return (void *) (entry + 1);
    // } else {
    //     FreeListEntry *next = NextChunk(entry);
    //     next->prev_size_ = entry->size_;
    // }

    if ((entry->size_ & SIZE_MASK) > size) {
        SplitAndPush(entry, size);
        memcpy((entry + 1), allocation, original_size & SIZE_MASK);
        Verify();
        return (void *) (entry + 1);
    } else if((entry->size_ & SIZE_MASK) == size) {
        memcpy((entry + 1), allocation, original_size & SIZE_MASK);
        Verify();
        return (void *) (entry + 1);
    } else {
        FreeListEntry *next = NextChunk(entry);
        next->prev_size_ = entry->size_;
    }

    void *result = Allocate(size);
    memcpy(result, allocation, original_size);
    Free((void *) (entry + 1));
    Verify();
    return result;
}

void KHeap::Free(void *allocation)
{
    if (!heap_ || !allocation) {
        return;
    }

    auto *entry = (FreeListEntry * )((char *) allocation - sizeof(FreeListEntry));
    if(! (entry->size_ & IN_USE)) {
        Log("[WARNING] Attempting to free non-allocated memory.\n");
    }
    if (entry->size_ >= PMM_THRESHOLD) {
        BuddyAllocator::Free(ToPAddr(entry));
        Verify();
        return;
    }

    entry->size_ &= ~(IN_USE);
    entry = MergeWithNeighbors(entry);
    if (entry != top_) {
        FreeListEntry *next = NextChunk(entry);
        next->prev_size_ = entry->size_;
        uint8_t bin_ind = BinIndex(entry->size_);
        PushFront(bin_ind, entry);
    }

    Verify();
}

void KHeap::Print()
{
    FreeListEntry *chunk = (FreeListEntry *) heap_;
    FreeListEntry *prev = nullptr;
    size_t i = 0;
    do {
        if ((uintptr_t) chunk > (uintptr_t) top_) {
            Log("WARNING: SIZES INCORRECT.\n");
        }
        if(prev && chunk->prev_size_ != prev->size_) {
            Log("\t Warning: Chunk's prev size value is incorrect.\n");
        }
        Log("\t--------------------------------------------------\n");
        Log("\t[%d]\t\tFREE %d\t\tSIZE %d (0x%x)\n", i, !(chunk->size_ & IN_USE),
            chunk->size_ & SIZE_MASK, chunk->size_ & SIZE_MASK);
        if((chunk->size_ & SIZE_MASK) == 0) {
            break;
        }
        prev = chunk;
        chunk = (FreeListEntry * )
                ((uintptr_t) chunk + (chunk->size_ & SIZE_MASK));
        ++i;
    } while (chunk != top_);
    Log("\t--------------------------------------------------\n");
    Log("\t[%d]\t\t[TOP]\t\tSIZE %d\n", i, top_->size_);
    Log("\t--------------------------------------------------\n");
    while(1);
}

void *KHeap::ToPAddr(void *vaddr)
{
    return (void *) ToPAddr((uintptr_t) vaddr);
}

uintptr_t KHeap::ToPAddr(uintptr_t vaddr)
{
    // There's no reason a vaddr would be mapped below KERNEL_DATA_BASE, so
    // this is almost certainly a paddr.
    if(vaddr < KERNEL_DATA_BASE) {
        return vaddr;
    } else if(vaddr <= HEAP_BASE) {
        return vaddr - KERNEL_DATA_BASE;
    }
    return vaddr + heap_paddr_ - HEAP_BASE;
}

void *KHeap::ToVAddr(void *paddr)
{
    return (void *) ToVAddr((uintptr_t) paddr);
}

uintptr_t KHeap::ToVAddr(uintptr_t paddr)
{
    return paddr - heap_paddr_ + HEAP_BASE;
}

void KHeap::SetSizeLimit(size_t max_heap_size)
{
    max_heap_size_ = max_heap_size;
}

void KHeap::Verify()
{
    if (heap_ == top_) {
        return;
    }
    FreeListEntry *chunk = (FreeListEntry *) heap_;
    FreeListEntry *prev = nullptr;
    size_t i = 0;
    size_t sum = 0;
    do {
        sum += (chunk->size_ & SIZE_MASK);
        if ((uintptr_t) chunk > (uintptr_t) top_) {
            Log("WARNING: SIZE GOES PAST TOP.\n");
            Print();
            return;
        }
        if (prev) {
            if (chunk->prev_size_ != prev->size_) {
                Log("CHUNK %d PREV SIZE IS 0x%x, SHOULD BE 0x%x.\n",
                    i, chunk->prev_size_, prev->size_);
                Print();
                return;
            }
        }

        if (chunk->backward_ && chunk->backward_->forward_ &&
            chunk->backward_->forward_ == chunk->backward_->forward_->forward_) {
            Log("LOOP IN CHUNK %d\n", i);
            Print();
            return;
        }
        if (chunk->backward_ && chunk->forward_ &&
            chunk->backward_->forward_ == chunk->backward_) {
            Log("LOOP IN CHUNK %d\n", i);
            Print();
            return;
        }

        prev = chunk;
        chunk = (FreeListEntry *)
                ((uintptr_t) chunk + (chunk->size_ & SIZE_MASK));
        ++i;
    } while (chunk != top_);

    if(top_->forward_ || top_->backward_) {
        Log("TOP ON FREE LIST\n");
        while(1);
    }

    sum += top_->size_;
    if(sum != heap_size_) {
        Log("Sum of chunks (%d) differs from heap size (%d)\n",
            sum, heap_size_);
        Print();
    }
}

void *KernelAllocator::Allocate(size_t size)
{
    return KHeap::Allocate(size);
}

void *KernelAllocator::Reallocate(void *allocation, size_t size)
{
    return KHeap::Reallocate(allocation, size);
}

void KernelAllocator::Free(void *allocation)
{
    KHeap::Free(allocation);
}

void *operator new(size_t size)
{
    void *alloc = KHeap::Allocate(size);
    return alloc;
}

void *operator new[](size_t size)
{
    return KHeap::Allocate(size);
}

void *operator new(size_t, void *p) noexcept {
    return p;
}

void *operator new[](size_t, void *p) noexcept {
    return p;
}

void operator delete(void *p)
{
    KHeap::Free(p);
}

void operator delete[](void *p)
{
    KHeap::Free(p);
}

void  operator delete  (void *, void *) noexcept { }
void  operator delete[](void *, void *) noexcept { }

void operator delete(void *ptr, size_t) noexcept
{
    KHeap::Free(ptr);
}

void operator delete[](void *ptr, size_t) noexcept
{
    KHeap::Free(ptr);
}



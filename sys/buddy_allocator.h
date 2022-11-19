#ifndef BUDDY_ALLOCATOR_H
#define BUDDY_ALLOCATOR_H

#include "stivale2.h"
#include <stddef.h>

// We make this a namespace as opposed to a class, because there should be only
// a single physical memory allocator in the entire system; as such, we use
// static variables/functions in place of private members/methods. Alternative
// would be an ugly singleton pattern.
namespace BuddyAllocator
{
/**
 * Given a stivale2 memmap, catalog all usable ranges of memory and add
 * them to freelists.
 * @param memmap A stivale2 memmap enumerating usable ranges of memory
 *               (as well as non-usable ones, which are not of interest
 *                to us).
 */
void InitBuddyAllocator(const struct stivale2_struct_tag_memmap &memmap);

/**
 * Allocate a region of pages greater than or equal to requested one.
 * Given the allocator's array of freelists in which entry n denotes 2^n
 * pages, the allocation code will find the first freelist containing an
 * entry whose size is greater than or equal to the requested one. It will
 * then pop that entry from the freelist, and continually split it until
 * the entry is equal in size to 2^(ceil(log2(size)). The unused buddies
 * will be added to freelists.
 *
 * @param size Requested allocation size, in bytes.
 * @return 2^n pages, such that n is the smallest value where
 *         2^n * PAGE_SIZE exceeds requested allocation size.
 */
void *Allocate(size_t size);

/**
 * If existing allocation is already larger than size (due to internal frag-
 * mentation from allocating powers of 2), return that allocation. Otherwise,
 * free existing allocation, retrieve a new one, and copy the data.
 *
 * @param allocation An existing allocation of pages.
 * @param size The size of the requested new allocation, in bytes.
 * @return A new allocation of 2^n pages.
 */
void *Realloc(void *allocation, size_t size);

/**
 * Given an existing allocation, mark it as free and continually merge the
 * order with its buddy so long as the buddy is free; append merged entries
 * to the freelist of the allocation's order plus one. Then do the same for
 * the parent entry.
 *
 * @param allocation An allocation of pages from a call to
 *                   BuddyAllocator::Allocate.
 */
void Free(void *allocation);

size_t MemInUse();

size_t MemFree();

size_t TotalMem();

bool MemCritical();

/**
 * Print some information about the blocks stored in this allocator's free
 * lists.
 */
void Print();

/**
 * @return Return the number of blocks (pages) currently available in this
 *         allocator's freelists.
 */
size_t NumBlocks();

}

#endif
#ifndef KHEAP_H
#define KHEAP_H

#include "buddy_allocator.h"
#include "page_map.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stddef.h>

namespace ds { class MemCache; }

namespace KHeap {
    void Init(size_t initial_size, PageMap *page_map,
              size_t max_heap_size=0x40000000);

    void *Allocate(size_t size);

    void *Reallocate(void *allocation, size_t size);

    void Free(void *allocation);

    void Print();

    void SetSizeLimit(size_t max_heap_size);

    void Verify();

    void *ToPAddr(void *vaddr);
    uintptr_t ToPAddr(uintptr_t vaddr);
    void *ToVAddr(void *paddr);
    uintptr_t ToVAddr(uintptr_t paddr);
}

struct KernelAllocator {
    static void *Allocate(size_t size);
    static void *Reallocate(void *allocation, size_t size);
    static void Free(void *allocation);
};

void *operator new(size_t size);

void *operator new[](size_t size);

void operator delete(void *p);

void operator delete[](void *p);

void *operator new(size_t, void *p) noexcept;

void *operator new[](size_t, void *p) noexcept;

void operator delete  (void *, void *) noexcept;

void operator delete[](void *, void *) noexcept;

void operator delete (void *ptr, size_t) noexcept;

template<typename derived_t>
class KernelAllocated {
public:
    void *operator new(size_t size) noexcept
    {
        return KHeap::Allocate(size);
    }

    void *operator new[](size_t count) noexcept
    {
        return KHeap::Allocate(count * sizeof(derived_t));
    }

    void operator delete(void *allocation) noexcept
    {
        if(allocation) {
            return KHeap::Free(allocation);
        }
    }

    void operator delete[](void *allocation) noexcept
    {
        if(allocation) {
            return KHeap::Free(allocation);
        }
    }

    void *operator new(size_t size, void *p) noexcept
    {
        return ::operator new(size, p);
    }

    void *operator new[](size_t size, void *p) noexcept
    {
        return ::operator new[](size, p);
    }

    void  operator delete (void *p1, void *p2) noexcept
    {
        return ::operator delete(p1, p2);
    }

    void  operator delete[](void *p1, void *p2) noexcept
    {
        return ::operator delete[](p1, p2);
    }

};

#endif
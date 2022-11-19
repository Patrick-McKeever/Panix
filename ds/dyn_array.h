#ifndef DYN_ARRAY_H
#define DYN_ARRAY_H

#include <sys/kheap.h>
#include <libc/string.h>
#include <sys/log.h>
#include <ds/optional.h>
#include <initializer_list>

namespace ds {
template <typename T, typename allocator_t = KernelAllocator>
class DynArray
{
public:
    DynArray()
        : slots_used_(0)
        , capacity_(0)
        , arr_(nullptr)
    {}

    DynArray(size_t init_size)
        : slots_used_(0)
        , capacity_(init_size)
        , arr_((T*) allocator_t::Allocate(capacity_ * sizeof(T)))
    {
        if(arr_) {
            memset(arr_, 0, capacity_ * sizeof(T));
        }
    }

    DynArray(const std::initializer_list<T>& els)
        : slots_used_(els.size())
        , capacity_(els.size() > 1 ?
                        1 << (8 * sizeof(typeof(slots_used_)) -
                          __builtin_clz(els.size() - 1)) :
                        1)
        , arr_((T*) allocator_t::Allocate(capacity_ * sizeof(T)))
    {
        if(arr_) {
            memset(arr_, 0, capacity_ * sizeof(T));
        }
        for (size_t i = 0; i < els.size(); ++i) {
            arr_[i] = *(els.begin() + i);
        }
    }

    DynArray(const DynArray& rhs)
        : slots_used_(rhs.slots_used_)
        , capacity_(rhs.capacity_)
        , arr_((T*) allocator_t::Allocate(capacity_ * sizeof(T)))
    {
        if(arr_) {
            memset((void*) arr_, 0, capacity_ * sizeof(T));
        }
        for (size_t i = 0; i < rhs.Size(); ++i) {
            arr_[i] = rhs[i];
        }
    }

    DynArray& operator=(const DynArray& rhs)
    {
        if (&rhs == this) {
            return *this;
        }
        if (arr_) {
            allocator_t::Free(arr_);
        }

        slots_used_ = rhs.slots_used_;
        capacity_   = rhs.capacity_;
        if(capacity_) {
            arr_ = (T*) allocator_t::Allocate(capacity_ * sizeof(T));
            memset(arr_, 0, capacity_ * sizeof(T));

            for (size_t i = 0; i < rhs.Size(); ++i) {
                arr_[i] = rhs.arr_[i];
            }
        }
        return *this;
    }

    ~DynArray()
    {
        if(arr_) {
            allocator_t::Free(arr_);
        }
    }

    void Append(const T& el)
    {
        if (slots_used_ == capacity_) {
            capacity_ = capacity_ ? capacity_ * 2 : 1;
            if(arr_) {
                arr_ = (T*) allocator_t::Reallocate(arr_, capacity_ * sizeof(T));
                memset(&arr_[slots_used_], 0,
                       (capacity_ - slots_used_) * sizeof(T));

            } else {
                arr_ = (T*) allocator_t::Allocate(capacity_ * sizeof(T));
                memset(arr_, 0, capacity_ * sizeof(T));
            }
        }
        arr_[slots_used_++] = el;
    }

    T Lookup(int ind) const
    {
        // Bounds checking; if negative, abs(ind) should be less than no. slots
        // used.
        if (ind >= 0) {
            return arr_[ind];
        }
        return arr_[slots_used_ - ind];

        // I think this should work as a branchless version...
        // return arr_[(slots_used_ + ind) % slots_used_];
    }

    void Remove(size_t ind)
    {
        memmove(&arr_[ind], arr_[ind + 1], --slots_used_ - ind);
    }

    size_t Size() const
    {
        return slots_used_;
    }

    T Back() const
    {
        return arr_[slots_used_ - 1];
    }

    T operator[](int ind) const
    {
        if (ind >= 0) {
            return arr_[ind];
        } else {
            return arr_[slots_used_ + ind];
        }
    }

    T& operator[](int ind)
    {
        if (ind >= 0) {
            return (T&) arr_[ind];
        } else {
            return (T&) arr_[slots_used_ + ind];
        }
    }

protected:
    size_t slots_used_, capacity_;
    T*     arr_;
};
}

#endif
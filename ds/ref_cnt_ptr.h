#ifndef REF_CNT_PTR_H
#define REF_CNT_PTR_H

#include <sys/kheap.h>
#include <initializer_list>

namespace ds {

template<typename T, typename allocator_t=KernelAllocator>
class RefCntPtr
{
public:
    RefCntPtr()
            : raw_ptr_(nullptr)
            , refs_(nullptr)
    {}

    RefCntPtr(void *ptr)
            : raw_ptr_((T*) ptr)
            , refs_((size_t *) allocator_t::Allocate(sizeof(size_t)))
    {
        *refs_ = 1;
    }

    RefCntPtr(T *ptr)
            : raw_ptr_(ptr)
            , refs_((size_t *) allocator_t::Allocate(sizeof(size_t)))
    {
        *refs_ = 1;
    }

    RefCntPtr(const RefCntPtr &rhs)
            : raw_ptr_(rhs.raw_ptr_)
            , refs_(rhs.refs_)
    {
        ++(*refs_);
        if((uintptr_t) refs_ == 0x659007988001a710) {
            Log("123");
        }
    }

    RefCntPtr &operator=(const RefCntPtr &rhs)
    {
        if (&rhs == this) {
            return *this;
        }

        if(refs_) {
            --(*refs_);
            if (!*refs_) {
                if (refs_) {
                    allocator_t::Free(refs_);
                }
                if (raw_ptr_) {
                    allocator_t::Free(raw_ptr_);
                }
            }
        }

        raw_ptr_ = rhs.raw_ptr_;
        refs_    = rhs.refs_;
        ++(*refs_);

        return *this;
    }

    RefCntPtr(RefCntPtr &&rhs)
            : raw_ptr_(rhs.raw_ptr_)
            , refs_(rhs.refs_)
    {
        rhs.refs_ = nullptr;
        rhs.raw_ptr_ = nullptr;
    }

    RefCntPtr &operator=(RefCntPtr &&rhs)
    {
        if(refs_) {
            --(*refs_);
            if (!*refs_) {
                if (refs_) {
                    allocator_t::Free(refs_);
                }
                if (raw_ptr_) {
                    allocator_t::Free(raw_ptr_);
                }
            }
        }

        raw_ptr_ = rhs.raw_ptr_;
        refs_    = rhs.refs_;
        rhs.refs_ = nullptr;
        rhs.raw_ptr_ = nullptr;
        return *this;
    }

    ~RefCntPtr()
    {
        if(refs_) {
            --(*refs_);
            if (!*refs_) {
                if (refs_) {
                    allocator_t::Free(refs_);
                }
                if (raw_ptr_) {
                    allocator_t::Free(raw_ptr_);
                }
            }
        }
    }

    T &operator*() const noexcept
    {
        return *raw_ptr_;
    }

    T *operator->() const noexcept
    {
        return raw_ptr_;
    }

    T operator[](uint64_t i) const noexcept
    {
        return raw_ptr_[i];
    }

    T &operator[](uint64_t i) noexcept
    {
        return (T &) raw_ptr_[i];
    }

    operator bool() const
    {
        return (raw_ptr_ != nullptr);
    }

    friend bool operator==(RefCntPtr &lhs, RefCntPtr &rhs)
    {
        return lhs.raw_ptr_ == rhs.raw_ptr_;
    }

protected:
    T      *raw_ptr_;
    size_t *refs_;
};

template<typename T, typename allocator_t=KernelAllocator>
class RefCntPtr<T> MakeRefCntPtr(T *ptr)
{
    return RefCntPtr<T, allocator_t>(ptr);
}

}

#endif

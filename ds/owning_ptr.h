#ifndef RAII_PTR_H
#define RAII_PTR_H

namespace ds {
template <typename T>
class OwningPtr
{
public:
    OwningPtr(void *raw_ptr)
        : raw_ptr_((T*) raw_ptr)
    {}

    OwningPtr(T *raw_ptr)
        : raw_ptr_(raw_ptr)
    {}

    OwningPtr(nullptr_t)
        : raw_ptr_(nullptr)
    {}

    OwningPtr(const OwningPtr&) = delete;
    OwningPtr&operator=(const OwningPtr&) = delete;

    OwningPtr(OwningPtr &&rhs) noexcept
        : raw_ptr_(rhs.raw_ptr_)
    {
        rhs.raw_ptr_ = nullptr;
    }

    OwningPtr &operator=(OwningPtr &&rhs) noexcept
    {
        if(&rhs == this) {
            return *this;
        }

        if(raw_ptr_) {
            delete raw_ptr_;
        }

        raw_ptr_ = rhs.raw_ptr_;
        rhs.raw_ptr_ = nullptr;
        return *this;
    }


    ~OwningPtr()
    {
        if(raw_ptr_) {
            raw_ptr_->~T();
            KHeap::Free(raw_ptr_);
        }
    }

    T *ToRawPtr() const noexcept
    {
        return raw_ptr_;
    }

    T &operator *() const noexcept
    {
        return *raw_ptr_;
    }

    T *operator ->() const noexcept
    {
        return raw_ptr_;
    }

    T operator [](uint64_t i) const noexcept
    {
        return raw_ptr_[i];
    }

    T &operator [](uint64_t i) noexcept
    {
        return (T&) raw_ptr_[i];
    }

    operator bool() const
    {
        return raw_ptr_ != nullptr;
    }

private:
    T *raw_ptr_;
};
}
#endif

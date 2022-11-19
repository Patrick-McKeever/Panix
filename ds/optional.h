#ifndef OPTIONAL_H
#define OPTIONAL_H

#include <sys/kheap.h>

namespace ds {
struct nullopt_t {};
constexpr nullopt_t NullOpt;

template <typename T, typename allocator_t = KernelAllocator>
class Optional
{
public:
    Optional()
        : has_val_(false)
        , dummy_(0)
    {
        memset(&val_, 0, sizeof(T));
    }

    Optional(nullopt_t)
        : has_val_(false)
        , dummy_(0)
    {
        memset(&val_, 0, sizeof(T));
    }

    Optional(const T &obj)
        : has_val_(true)
        , val_(obj)
    {}

    Optional(const Optional &rhs)
        : has_val_(rhs.has_val_)
    {
        memset(&val_, 0, sizeof(T));
        if(has_val_) {
            val_ = rhs.val_;
        }
    }

    Optional &operator =(const T &val)
    {
        has_val_ = true;
        if(has_val_) {
            val_.~T();
        }
        val_ = val;
        return *this;
    }

    Optional &operator =(nullopt_t)
    {
        has_val_ = false;
        if(has_val_) {
            val_.~T();
        }
        dummy_ = 0;
        return *this;
    }

    Optional &operator =(const Optional &rhs)
    {
        if(&rhs == this) {
            return *this;
        }

        if(has_val_) {
            val_.~T();
        }

        has_val_ = rhs.has_val_;
        if(rhs.has_val_) {
            val_ = rhs.val_;
        }
        return *this;
    }

    ~Optional()
    {
        if(has_val_) {
            val_.~T();
        }
    }

    T Value() const
    {
        return val_;
    }

    bool HasValue() const noexcept
    {
        return has_val_;
    }

    T operator *() const noexcept
    {
        return val_;
    }

    T* operator ->() const noexcept
    {
        if(! has_val_) {
            return (T*) (nullptr);
        }
        return (T*) (&val_);
    }

    operator bool() const noexcept
    {
        return has_val_;
    }

protected:
    bool has_val_;
    union { T val_; char dummy_; };
};
}

#endif

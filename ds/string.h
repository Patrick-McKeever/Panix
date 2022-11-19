#ifndef VARDAROS_STRING_H
#define VARDAROS_STRING_H

#include <libc/string.h>
#include <sys/kheap.h>
#include <sys/log.h>

namespace ds {
template<typename allocator_t=KernelAllocator>
class BaseString
{
public:
    BaseString() = default;

    BaseString(size_t len)
            : len_(len + 1)
            , chars_((char*) allocator_t::Allocate(len_))
    {
        memset(chars_, 0, len_);
    }

    BaseString(const char *str)
        : len_(strlen(str) + 1)
        , chars_((char*) allocator_t::Allocate(len_))
    {
        for(size_t i = 0; i < len_; ++i) {
            chars_[i] = str[i];
        }
        chars_[len_ - 1] = '\0';
    }

    BaseString(const char *str, size_t len)
        : len_(len ? str[len-1] == '\0' ? len : len + 1 : 1)
        , chars_((char*) allocator_t::Allocate(len_))
    {
        size_t i;
        for(i = 0; i < len; ++i) {
            chars_[i] = str[i];
        }
        chars_[len_ - 1] = '\0';
    }

    BaseString(const BaseString &rhs)
        : len_(rhs.len_)
        , chars_((char*) allocator_t::Allocate(len_))
    {
        for(size_t i = 0; i < len_; ++i) {
            chars_[i] = rhs.chars_[i];
        }
    }

    BaseString &operator =(const BaseString &rhs)
    {
        if(&rhs == this) {
            return *this;
        }

        allocator_t::Free(chars_);

        len_ = rhs.len_;
        chars_ = (char*) allocator_t::Allocate(len_);
        for(size_t i = 0; i < len_; ++i) {
            chars_[i] = rhs.chars_[i];
        }

        return *this;
    }

    ~BaseString()
    {
        if(chars_) {
            allocator_t::Free(chars_);
        }
    }

    BaseString Substr(int start, int end) const
    {
        size_t len = len_ - 1;
        size_t end_ind = (end >= 0) * (end) + (end < 0) * (len + end);
        size_t start_ind = (start >= 0) * (start) + (start < 0) * (len + start);

        if(end_ind == start_ind) {
            return "";
        }
        BaseString str(&chars_[start_ind], end_ind - start_ind);
        return str;
    }

    size_t Len() const
    {
        return len_;
    }

    const char *ToChars() const
    {
        return chars_;
    }

    friend BaseString operator +(const BaseString &lhs, const BaseString &rhs)
    {
        const char *concatenation = strcat(lhs, rhs);
        return BaseString(concatenation);
    }

    BaseString &operator +=(const BaseString &rhs)
    {
        size_t lhs_len = len_;
        len_ += rhs.len_;
        chars_ = allocator_t::Reallocate(chars_, len_);

        for(size_t i = 0; i < rhs.len_; ++i)
        {
            chars_[lhs_len + i] = rhs[i];
        }
        chars_[lhs_len + rhs.len_] = '\0';
        return this;
    }

    friend bool operator ==(const BaseString &lhs, const BaseString &rhs)
    {
        if(lhs.len_ != rhs.len_) {
            return false;
        }
        if(! lhs.chars_ && ! rhs.chars_) {
            return true;
        }
        return (strncmp(lhs.chars_, rhs.chars_, lhs.len_) == 0);
    }

    friend bool operator !=(const BaseString &lhs, const BaseString &rhs)
    {
        return ! (lhs == rhs);
    }

    char &operator [](int i)
    {
        if(i >= 0) {
            return (char&) chars_[i];
        } else if(i < 0) {
            return (char&) chars_[len_ - i];
        }
    }

    char operator [](int i) const
    {
        if(i >= 0 && i < (int) len_) {
            return chars_[i];
        } else if(i < 0 && (i + (int) len_) < 0) {
            return chars_[len_ - i];
        }
        return '\0';
    }

private:
    size_t len_;
    char *chars_;
};

using String = BaseString<KernelAllocator>;
}

#endif

#ifndef DISK_MEM_H
#define DISK_MEM_H

#include <ds/ref_cnt_ptr.h>

template<typename T>
class DiskMem : public ds::RefCntPtr<T>
{
public:
    DiskMem()
            : ds::RefCntPtr<T>(nullptr)
            , disk_(nullptr)
            , base_sector_(0)
            , num_sectors_(0)
    {}

    DiskMem(nullptr_t, uint64_t base_sector, size_t num_sectors)
            : ds::RefCntPtr<T>(nullptr)
            , disk_(nullptr)
            , base_sector_(0)
            , num_sectors_(0)
    {}

    DiskMem(SATAPort *disk, uint64_t base_sector, size_t num_sectors)
            : ds::RefCntPtr<T>((T*) disk->Read(base_sector, num_sectors))
            , disk_(disk)
            , base_sector_(base_sector)
            , num_sectors_(num_sectors)
    {}

    ~DiskMem()
    {
        WriteBack();
    }

    void WriteBack()
    {
        if(raw_ptr_) {
            disk_->Write(base_sector_, num_sectors_, raw_ptr_);
        }
    }

    const char *ToBytes() const
    {
        return (char*) raw_ptr_;
    }

    friend bool operator ==(const DiskMem &lhs, const DiskMem &rhs)
    {
        return lhs.raw_ptr_ = rhs.raw_ptr_;
    }

    using ds::RefCntPtr<T>::operator bool;
    using ds::RefCntPtr<T>::operator*;
    using ds::RefCntPtr<T>::operator->;
    using ds::RefCntPtr<T>::operator[];

private:
    constexpr static uint16_t SECTOR_SIZE = 512;

    using ds::RefCntPtr<T>::raw_ptr_;
    SATAPort *disk_;
    uint64_t base_sector_;
    size_t num_sectors_;
};
#endif

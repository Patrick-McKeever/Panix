#ifndef VNODE_H
#define VNODE_H

#include <ds/string.h>
#include <ds/ref_cnt_ptr.h>
#include <ds/hash_map.h>
#include <ds/optional.h>

enum class FileType {
    DIRECTORY,
    REGULAR,
    SYMLINK,
    BLOCK_DEV,
    CHAR_DEV,
    SOCKET,
    NAMED_PIPE,
    OTHER
};

class VNode : public KernelAllocated<VNode>
{
public:
    virtual uint32_t GetIno() const = 0;
    virtual FileType GetFileType() const = 0;
    virtual uint64_t GetLength() const = 0;
    virtual bool WriteBack() const = 0;
    virtual ds::Optional<ds::RefCntPtr<VNode>> Lookup(const ds::String &name) = 0;
    virtual ds::Optional<ds::RefCntPtr<VNode>>
    LookupAndPin(const ds::String &name) = 0;
    virtual void Unpin() = 0;
    virtual ds::Optional<ds::RefCntPtr<VNode>> Touch(const ds::String &name) = 0;
    virtual ds::Optional<ds::RefCntPtr<VNode>> MkDir(const ds::String &name) = 0;
    virtual ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
    ReadDir() = 0;
    virtual bool Remove(const ds::String &child_name) = 0;
    virtual bool Chmod(uint16_t perms) = 0;
    virtual bool Read(void *buffer, size_t offset, size_t len) = 0;
    virtual bool Write(void *buffer, size_t offset, size_t len) = 0;

    // virtual bool RmDir() = 0;
    //virtual bool Rename(const ds::String &new_name) = 0;
    virtual ds::Optional<ds::RefCntPtr<VNode>> Link(const ds::String &name,
                                                    uint32_t ino) = 0;
    virtual bool Unlink(const ds::String &name) = 0;
    virtual bool SymLink(const ds::String &name, const ds::String &target) = 0;
    //virtual void Open() = 0;
    virtual bool Mount(const ds::String &mntpt,
                       const ds::RefCntPtr<VNode> &root) = 0;
    virtual bool Unmount(const ds::String &mntpt) = 0;
    virtual size_t GetNumMounts() const = 0;
    //virtual void Access() = 0;
    //virtual void GetAttr() = 0;
    //virtual void SetAttr() = 0;
    //virtual void ReadLink() = 0;
    //virtual void MMap() = 0;
    //virtual void Close() = 0;
    //virtual void AdvLock() = 0;
    //virtual void Ioctl() = 0;
    //virtual void Select() = 0;
    //virtual void Lock() = 0;
    //virtual void Unlock() = 0;
    //virtual void Inactive() = 0;
    //virtual void Reclaim() = 0;
    //virtual void Abortop() = 0;
protected:
    ds::HashMap<ds::String, ds::RefCntPtr<VNode>> mnts_;
};


#endif

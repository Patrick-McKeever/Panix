#include "vfs.h"

FileHandle::FileHandle(ds::RefCntPtr<VNode> vnode, const ds::String &path,
                       bool readable, bool writeable)
        : vnode_(std::move(vnode))
        , path_(path)
        , offset_(0)
        , readable_(readable)
        , writeable_(writeable)
        , valid_(true)
{}

void FileHandle::Seek(size_t offset)
{
    offset_ = offset;
}

ds::Optional<ds::String> FileHandle::Read(size_t len)
{
    if(valid_ && readable_) {
        auto buff = (char *) KHeap::Allocate(len);
        if (vnode_->Read(buff, offset_, len)) {
            offset_ += len;
            return ds::String(buff, len);
        }
    }
    return ds::NullOpt;
}

bool FileHandle::Write(const ds::String &buff_str, size_t len)
{
    if(valid_ && writeable_) {
        if(vnode_->Write((void*) buff_str.ToChars(), offset_, len)) {
            offset_ += len;
            return true;
        }
    }
    return false;
}

ds::String FileHandle::GetPath() const
{
    return path_;
}

size_t FileHandle::GetLength() const
{
    return vnode_->GetLength();
}

void FileHandle::SuspendIO()
{
    valid_ = false;
    vnode_->Unpin();
}

VFS::VFS(SATAPort *port, size_t part_num)
{
    if(ds::Optional<GPTEntry> part_opt = port->GetNthPartition(part_num)) {
        ds::Optional<ds::RefCntPtr<VNode>> root_opt;
        if((root_opt = GetRootVNode(port, *part_opt))) {
            root_ = *root_opt;
        }
    }
}

VFS::VFS(SATAPort *port, const GPTEntry &entry)
{
    ds::Optional<ds::RefCntPtr<VNode>> root_opt;
    if((root_opt = GetRootVNode(port, entry))) {
        root_ = *root_opt;
    }
}

ds::Optional<FileHandle> VFS::Open(const ds::String &filename, bool read,
                                   bool write, bool create)
{
    ds::Optional<ds::RefCntPtr<VNode>> vnode_opt;
    KHeap::Verify();
    if((vnode_opt = FindVNode(filename, create, true))) {
        if(open_handles_.Contains(filename)) {
            ++open_handles_[filename];
        } else {
            open_handles_.Insert(filename, 1);
        }
        return FileHandle(*vnode_opt, filename, read, write);
    }

    return ds::NullOpt;
}

bool VFS::Close(FileHandle &handle)
{
    handle.SuspendIO();
    return true;
}

bool VFS::Mount(const ds::String &mnt_path_str, SATAPort *port, size_t part_num)
{
    Path mnt_path = DecomposePath(mnt_path_str);

    ds::Optional<ds::RefCntPtr<VNode>> mnt_parent_opt;
    if(! (mnt_parent_opt = root_->LookupAndPin(mnt_path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> mnt_parent = *mnt_parent_opt;

    ds::Optional<GPTEntry> part_opt;
    if(! (part_opt = port->GetNthPartition(part_num))) {
        return false;
    }
    GPTEntry part = *part_opt;

    ds::Optional<ds::RefCntPtr<VNode>> mnt_root = GetRootVNode(port, part);
    if(! mnt_root) {
        return false;
    }

    mnt_parent->Mount(mnt_path.file, *mnt_root);
    return true;
}

bool VFS::Unmount(const ds::String &mnt_path_str)
{
    Path mnt_path = DecomposePath(mnt_path_str);

    ds::Optional<ds::RefCntPtr<VNode>> mnt_parent_opt;
    if(! (mnt_parent_opt = root_->Lookup(mnt_path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> mnt_parent = *mnt_parent_opt;

    if(! mnt_parent->Unmount(mnt_path.file)) {
        return false;
    }

    if(mnt_parent->GetNumMounts() == 0) {
        mnt_parent->Unpin();
    }

    return true;
}

bool VFS::Touch(const ds::String &name)
{
    Path path = DecomposePath(name);
    ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
    if (!(parent_dir_opt = root_->Lookup(path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;
    return parent_dir->Touch(path.file).HasValue();
}

bool VFS::MkDir(const ds::String &name)
{
    Path path = DecomposePath(name);
    ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
    if (!(parent_dir_opt = root_->Lookup(path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;
    return parent_dir->MkDir(path.file);
}

bool VFS::Remove(const ds::String &name)
{
    Path path = DecomposePath(name);
    ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
    if (!(parent_dir_opt = root_->Lookup(path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;
    return parent_dir->Remove(path.file);
}

bool VFS::Link(const ds::String &name, const ds::String &target)
{
    ds::Optional<ds::RefCntPtr<VNode>> vnode_opt;
    if ((vnode_opt = FindVNode(name, false))) {
        Path target_path = DecomposePath(target);

        ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
        if (!(parent_dir_opt = root_->Lookup(target_path.dir))) {
            return false;
        }
        ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;

        return parent_dir->Link(target_path.file, (*vnode_opt)->GetIno());
    }
    return false;
}

bool VFS::SymLink(const ds::String &name, const ds::String &target)
{
    Path target_path = DecomposePath(target);

    ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
    if (!(parent_dir_opt = root_->Lookup(target_path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;
    return parent_dir->SymLink(target_path.file, name);
}

bool VFS::Unlink(const ds::String &name, const ds::String &target)
{
    Path target_path = DecomposePath(target);
    ds::Optional<ds::RefCntPtr<VNode>> parent_dir_opt;
    if (!(parent_dir_opt = root_->Lookup(target_path.dir))) {
        return false;
    }
    ds::RefCntPtr<VNode> parent_dir = *parent_dir_opt;
    return parent_dir->Unlink(target_path.file);
}

VFS::Path VFS::DecomposePath(const ds::String &path_str)
{
    int i;
    for(i = path_str.Len(); i >= 0 && path_str[i] != '/'; --i);
    Path path {};
    path.dir = path_str.Substr(0, i+1);
    path.file = path_str.Substr(i+1, path_str.Len());
    return path;
}

ds::Optional<ds::RefCntPtr<VNode>> VFS::FindVNode(const ds::String &filename,
                                                  bool create, bool pin)
{
    ds::Optional<ds::RefCntPtr<VNode>> vnode_opt;
    if(create) {
        Path path = DecomposePath(filename);
        ds::RefCntPtr<VNode> parent_dir;
        if(path.dir == "/") {
            parent_dir = root_;
        } else if(ds::Optional<ds::RefCntPtr<VNode>> pd = root_->Lookup(path.dir)) {
            parent_dir = *pd;
        }

        vnode_opt = parent_dir ? parent_dir->Touch(path.file) : ds::NullOpt;
    } else {
        vnode_opt = pin ? root_->LookupAndPin(filename) : root_->Lookup(filename);
    }

    return vnode_opt;
}

ds::Optional<ds::RefCntPtr<VNode>> VFS::GetRootVNode(SATAPort *port,
                                                     GPTEntry part)
{

    if(part.part_type_guid_lo_ == ESP_GUID_LO &&
       part.part_type_guid_hi_ == ESP_GUID_HI)
    {
        auto mnt = new Ext2Mount(port, part.start_lba_);
        return mnt->ReadRootDir();
    }

    return ds::NullOpt;
}

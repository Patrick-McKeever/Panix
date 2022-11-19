#ifndef VFS_H
#define VFS_H

#include <ds/string.h>
#include <sys/fs/vmount.h>
#include <sys/gpt.h>
#include <utility>
#include <sys/fs/ext2_vnode.h>

class FileHandle {
public:
    FileHandle(ds::RefCntPtr<VNode> vnode, const ds::String &path,
               bool readable, bool writeable);

    void Seek(size_t offset);

    ds::Optional<ds::String> Read(size_t len);

    bool Write(const ds::String &buff_str, size_t len);

    size_t GetLength() const;

    ds::String GetPath() const;

    void SuspendIO();

private:
    ds::RefCntPtr<VNode> vnode_;
    ds::String path_;
    size_t offset_;
    bool readable_, writeable_, valid_;
};

class VFS
{
public:
    VFS(SATAPort *port, size_t part_num=1);
    VFS(SATAPort *port, const GPTEntry &entry);

    ds::Optional<FileHandle> Open(const ds::String &filename, bool read=true,
                                  bool write=true, bool create=false);

    bool Close(FileHandle &handle);

    bool Mount(const ds::String &mnt_path_str, SATAPort *port,
               size_t part_num=1);

    bool Unmount(const ds::String &mnt_path_str);

    bool Touch(const ds::String &name);

    bool MkDir(const ds::String &name);

    bool Remove(const ds::String &name);

    bool Link(const ds::String &name, const ds::String &target);

    bool SymLink(const ds::String &name, const ds::String &target);

    bool Unlink(const ds::String &name, const ds::String &target);

private:
    ds::HashMap<ds::String, size_t> open_handles_;
    ds::RefCntPtr<VNode> root_;

    struct Path {
        ds::String dir;
        ds::String file;
    };


    Path DecomposePath(const ds::String &path_str);

    ds::Optional<ds::RefCntPtr<VNode>> FindVNode(const ds::String &filename,
                                                 bool create, bool pin=false);

    ds::Optional<ds::RefCntPtr<VNode>> GetRootVNode(SATAPort *port,
                                                    GPTEntry part);
};


#endif
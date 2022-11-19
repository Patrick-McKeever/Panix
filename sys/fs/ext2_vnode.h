#ifndef EXT2_VNODE_H
#define EXT2_VNODE_H

#include <sys/fs/vnode.h>
#include <sys/fs/ext2_mount.h>

class Ext2VNode : public VNode
{
public:
    Ext2VNode(uint32_t ino, class Ext2Mount &mount);
    Ext2VNode(Ext2INode inode, uint32_t ino, class Ext2Mount &mount);
    ~Ext2VNode();


    virtual uint32_t GetIno() const override;
    virtual FileType GetFileType() const override;
    virtual uint64_t GetLength() const override;
    virtual bool WriteBack() const override;
    uint16_t GetPerms() const;

    virtual bool Read(void *buffer, size_t offset, size_t len) override;

    virtual bool Write(void *buffer, size_t offset, size_t len) override;

    virtual ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
    ReadDir() override;


    virtual ds::Optional<ds::RefCntPtr<VNode>> Link(const ds::String &name,
                                                    uint32_t ino) override;

    virtual bool Unlink(const ds::String &name) override;

    virtual bool SymLink(const ds::String &name, const ds::String &target)
    override;

    virtual ds::Optional<ds::RefCntPtr<VNode>>
    MkDir(const ds::String &name) override;

    virtual ds::Optional<ds::RefCntPtr<VNode>>
    Touch(const ds::String &name) override;

    // virtual bool RmDir() override;

    virtual ds::Optional<ds::RefCntPtr<VNode>>
    Lookup(const ds::String &name) override;

    virtual ds::Optional<ds::RefCntPtr<VNode>>
    LookupAndPin(const ds::String &name) override;

    virtual void Unpin() override;

    virtual bool Remove(const ds::String &child_name) override;

    virtual bool Chmod(uint16_t perms) override;

    virtual bool Mount(const ds::String &mntpt, const ds::RefCntPtr<VNode> &root) override;

    virtual bool Unmount(const ds::String &mntpt) override;

    virtual size_t GetNumMounts() const override;

private:
    Ext2INode inode_;
    uint32_t ino_;
    class Ext2Mount &mount_;

    struct RawDirEntry {
        uint32_t block_no;
        size_t offset;
        void *mem;
    };

    ds::Optional<RawDirEntry> GetDirEntry(const ds::String &name);

    void Prealloc();
    ds::Optional<ds::RefCntPtr<VNode>>
    MakeDirEntry(const ds::String &name, Ext2FileMode fmode,
                 Ext2VNode *vnode=nullptr);
    bool MakeDirEntry(const ds::String &name, const ds::RefCntPtr<VNode> &vnode);

    /**
     *
     * @param block_no The index of the block within this file. I.e. If we want
     *                 block 12 of the file, pass 12 as block_no.
     * @param create Should we create this block if it does not already exist?
     * @return The index of this block within the filesystem.
     */
    ds::Optional<uint32_t> GetOrCreateBlock(uint32_t block_no, bool create);

    ds::Optional<ds::DynArray<Extent>>
    GetOrCreateExtents(uint32_t start_block, size_t len, bool create,
                       bool mdata=false);
    int64_t GetOrCreateExtents(uint32_t *parent, uint32_t block_no, size_t len,
                               ds::DynArray<Extent> &extents, uint8_t level,
                               bool create, bool mdata=false);

    size_t PtrsPerIndirectBlock(uint8_t level);

    void AddToExtentList(ds::DynArray<Extent> &extents, uint32_t block);
    bool ReadWrite(void *buffer, size_t offset, size_t len, bool write);
    uint16_t GetExt2FileType() const;
    bool InitializeDir(uint32_t parent_ino);
};

#endif

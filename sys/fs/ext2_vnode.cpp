#include "ext2_vnode.h"

#include <utility>

static Ext2FileType FileTypeToExt2FileType(FileType type)
{
    switch(type) {
        case FileType::REGULAR: {
            return Ext2FileType::EXT2_FT_REG_FILE;
        }
        case FileType::DIRECTORY: {
            return Ext2FileType::EXT2_FT_DIR;
        }
        case FileType::SYMLINK: {
            return Ext2FileType::EXT2_FT_SYMLINK;
        }
        case FileType::BLOCK_DEV: {
            return Ext2FileType::EXT2_FT_BLKDEV;
        }
        case FileType::CHAR_DEV: {
            return Ext2FileType::EXT2_FT_CHRDEV;
        }
        case FileType::NAMED_PIPE: {
            return Ext2FileType::EXT2_FT_FIFO;
        }
        case FileType::SOCKET: {
            return Ext2FileType::EXT2_FT_SOCK;
        }
        case FileType::OTHER: {
            return Ext2FileType::EXT2_FT_UNKNOWN;
        }
    }
}

static Ext2FileType FileModeToFileType(Ext2FileMode mode)
{
    switch(mode) {
        case EXT2_S_IFSOCK: {
            return EXT2_FT_SOCK;
        }
        case EXT2_S_IFLNK: {
            return EXT2_FT_SYMLINK;
        }
        case EXT2_S_IFREG: {
            return EXT2_FT_REG_FILE;
        }
        case EXT2_S_IFBLK: {
            return EXT2_FT_BLKDEV;
        }
        case EXT2_S_IFDIR: {
            return EXT2_FT_DIR;
        }
        case EXT2_S_IFCHR: {
            return EXT2_FT_CHRDEV;
        }
        case EXT2_S_IFIFO: {
            return EXT2_FT_FIFO;
        }
        default: {
            return EXT2_FT_UNKNOWN;
        }
    }
}

static Ext2FileMode FileTypeToFileMode(Ext2FileType type)
{
    switch(type) {
        case EXT2_FT_SOCK: {
            return EXT2_S_IFSOCK;
        }
        case EXT2_FT_SYMLINK: {
            return EXT2_S_IFLNK;
        }
        case EXT2_FT_REG_FILE: {
            return EXT2_S_IFREG;
        }
        case EXT2_FT_BLKDEV: {
            return EXT2_S_IFBLK;
        }
        case EXT2_FT_DIR: {
            return EXT2_S_IFDIR;
        }
        case EXT2_FT_CHRDEV: {
            return EXT2_S_IFCHR;
        }
        case EXT2_FT_FIFO: {
            return EXT2_S_IFIFO;
        }
            // This is actually outside the spec, but it stops GCC from complaining
            // about lack of default case and produces desired behavior regardless.
        default: {
            return EXT2_S_NONE;
        }
    }
}

Ext2VNode::Ext2VNode(Ext2INode inode, uint32_t ino, class Ext2Mount &mount)
        : inode_(inode)
        , ino_(ino)
        , mount_(mount)
{}

Ext2VNode::Ext2VNode(uint32_t ino, class Ext2Mount &mount)
        : inode_{}
        , ino_(ino)
        , mount_(mount)
{
    if(ds::Optional<Ext2INode> inode_opt = mount.ReadINode(ino)) {
        inode_ = *inode_opt;
    }
}

Ext2VNode::~Ext2VNode()
{
    WriteBack();
    mount_.Unpin(ino_);
}

bool Ext2VNode::WriteBack() const
{
    if(! mount_.WriteINode(ino_, inode_)) {
        return false;
    }
    return true;
}

uint32_t Ext2VNode::GetIno() const
{
    return ino_;
}

FileType Ext2VNode::GetFileType() const
{
    switch(GetExt2FileType()) {
        case EXT2_S_IFREG: {
            return FileType::REGULAR;
        }
        case EXT2_S_IFDIR: {
            return FileType::DIRECTORY;
        }
        case EXT2_S_IFLNK: {
            return FileType::SYMLINK;
        }
        case EXT2_S_IFBLK: {
            return FileType::BLOCK_DEV;
        }
        case EXT2_S_IFCHR: {
            return FileType::CHAR_DEV;
        }
        case EXT2_S_IFIFO: {
            return FileType::NAMED_PIPE;
        }
        case EXT2_S_IFSOCK: {
            return FileType::SOCKET;
        }
    }
    return FileType::OTHER;
}

uint64_t Ext2VNode::GetLength() const
{
    return inode_.i_size;
}

uint16_t Ext2VNode::GetPerms() const
{
    return inode_.i_mode & 0x1FF;
}

uint16_t Ext2VNode::GetExt2FileType() const {
    return inode_.i_mode & 0xF000;
}

bool Ext2VNode::Read(void *buffer, size_t offset, size_t len)
{
    return ReadWrite(buffer, offset, len, false);
}

bool Ext2VNode::Write(void *buffer, size_t offset, size_t len)
{
    if(ReadWrite(buffer, offset, len, true)) {
        inode_.i_size += (offset - inode_.i_size) * (offset > inode_.i_size);
        WriteBack();
        return true;
    }
    return false;
}

ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::MkDir(const ds::String &name)
{
    if (Ext2VNode *vnode = mount_.AllocVNode(EXT2_S_IFDIR)) {
        vnode->Prealloc();
        vnode->InitializeDir(ino_);
        vnode->Chmod(0x1FF);
        vnode->WriteBack();
        if(MakeDirEntry(name, vnode)) {
            ++inode_.i_links_count;
            WriteBack();
            return ds::MakeRefCntPtr((VNode*) vnode);
        }
    }

    return ds::NullOpt;
}

ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Touch(const ds::String &name)
{
    if (Ext2VNode *vnode = mount_.AllocVNode(EXT2_S_IFREG)) {
        vnode->Prealloc();
        vnode->Chmod(0x1FF);
        vnode->WriteBack();
        if(MakeDirEntry(name, vnode)) {
            return ds::MakeRefCntPtr((VNode*) vnode);
        }
    }

    return ds::NullOpt;
}

ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>> Ext2VNode::ReadDir()
{
    if(GetExt2FileType() != EXT2_S_IFDIR) {
        return ds::NullOpt;
    }

    size_t block_size = mount_.GetBlockSize();
    ds::HashMap<ds::String, ds::RefCntPtr<VNode>> vnodes;
    void *entries = KHeap::Allocate(mount_.GetBlockSize());

    size_t rem = inode_.i_size;
    for(size_t blk = 0; rem > 0; ++blk) {
        ds::Optional<uint32_t> fs_blk_no;
        if(! (fs_blk_no = GetOrCreateBlock(blk, false))) {
            KHeap::Free(entries);
            return ds::NullOpt;
        }

        if(! mount_.ReadBlock(entries, *fs_blk_no)) {
            KHeap::Free(entries);
            return ds::NullOpt;
        }

        auto entry = (Ext2DirEntry*) entries;
        size_t block_off = 0;
        Log("Dir size %d\n", inode_.i_size);
        while(block_off < block_size && rem) {
            if(entry->inode) {
                ds::String name(entry->name, entry->name_len);
                ds::Optional<ds::RefCntPtr<VNode>> vnode_opt;
                if((vnode_opt = mount_.GetVNode(entry->inode))) {
                    vnodes.Insert(name, *vnode_opt);
                } else {
                    return ds::NullOpt;
                }

                Log("%s (ino %d), len %d\n", name, entry->inode, entry->rec_len);
            }
            block_off += entry->rec_len;
            rem -= entry->rec_len;
            entry = (Ext2DirEntry*) ((uint8_t*) entry + entry->rec_len);
        }

        if(block_off < block_size && !entry->inode) {
            break;
        }
    }

    KHeap::Free(entries);
    return vnodes;
}

ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Lookup(const ds::String &name)
{
    int len;
    int start = name[0] == '/' ? 1 : 0;
    for(len = start; len < name.Len() && name[len] != '/'; ++len);
    ds::String child = name.Substr(start, len+1);

    ds::RefCntPtr<VNode> child_vnode;
    if(ds::Optional<RawDirEntry> raw_entry_opt = GetDirEntry(child)) {
        RawDirEntry raw_entry = *raw_entry_opt;
        auto entry = (Ext2DirEntry*) ((char*) raw_entry.mem + raw_entry.offset);
        ds::Optional<ds::RefCntPtr<VNode>> child_opt;
        if(! (child_opt = mount_.GetVNode(entry->inode))) {
            KHeap::Free(raw_entry.mem);
            return ds::NullOpt;
        }

        child_vnode = *child_opt;
        KHeap::Free(raw_entry.mem);
    } else if(ds::Optional<ds::RefCntPtr<VNode>> mnt_opt = mnts_.Lookup(child)) {
        child_vnode = *mnt_opt;
    } else {
        return ds::NullOpt;
    }

    ds::String remaining = name.Substr(len, name.Len());
    if(remaining == "" || remaining == "/") {
        return child_vnode;
    }
    return child_vnode->Lookup(remaining);
}

ds::Optional<ds::RefCntPtr<VNode>>
Ext2VNode::LookupAndPin(const ds::String &name)
{
    ds::Optional<ds::RefCntPtr<VNode>> vnode_opt = Lookup(name);
    if(vnode_opt) {
        mount_.Pin(*vnode_opt);
    }

    return vnode_opt;
}

void Ext2VNode::Unpin()
{
    mount_.Unpin(ino_);
}

bool Ext2VNode::Remove(const ds::String &child_name)
{
    ds::Optional<RawDirEntry> raw_entry_opt;
    if(! (raw_entry_opt = GetDirEntry(child_name))) {
        return false;
    }

    RawDirEntry raw_entry = *raw_entry_opt;
    auto entry = (Ext2DirEntry*) ((char*) raw_entry.mem + raw_entry.offset);
    uint32_t child_ino = entry->inode;
    entry->inode = 0;
    memset(entry->name, 0, entry->name_len);

    if(! mount_.WriteBlock(raw_entry.mem, raw_entry.block_no)) {
        return false;
    }

    Ext2VNode child(child_ino, mount_);
    // Only delete actual inode/contents if no further links exist.
    if(--child.inode_.i_links_count > 0) {
        child.WriteBack();
        return true;
    }

    ds::Optional<ds::DynArray<Extent>> exts_opt;
    size_t no_data_blks = child.GetLength() / mount_.GetBlockSize();
    if(! (exts_opt = child.GetOrCreateExtents(0, no_data_blks, false, true))) {
        return false;
    }

    ds::DynArray<Extent> exts = *exts_opt;
    for(int ext = 0; ext < exts.Size(); ++ext) {
        uint32_t final_blk = exts[ext].start_ + exts[ext].len_;
        for(uint32_t blk = exts[ext].start_; blk < final_blk; ++blk) {
            if(! mount_.FreeBlock(blk)) {
                return false;
            }
        }
    }

    if(! mount_.WriteINode(child_ino, {})) {
        return false;
    }
    return true;
}

ds::Optional<Ext2VNode::RawDirEntry>
Ext2VNode::GetDirEntry(const ds::String &name)
{
    if(GetExt2FileType() != EXT2_S_IFDIR) {
        return ds::NullOpt;
    }

    ds::Optional<ds::DynArray<Extent>> exts_opt;
    ds::DynArray<Extent> exts;
    if((exts_opt = GetOrCreateExtents(0, inode_.i_blocks, false))) {
        exts = *exts_opt;
    } else {
        return ds::NullOpt;
    }

    size_t rem = inode_.i_size;
    size_t blk_size = mount_.GetBlockSize();
    void *entries = KHeap::Allocate(mount_.GetBlockSize());
    const char *name_cstr = name.ToChars();

    for(int ext = 0; ext < exts.Size(); ++ext) {
        size_t final_blk = exts[ext].start_ + exts[ext].len_;
        for(uint32_t blk = exts[ext].start_; blk < final_blk; ++blk) {
            if(! mount_.ReadBlock(entries, blk)) {
                KHeap::Free(entries);
                return ds::NullOpt;
            }

            size_t entries_size = rem < blk_size ? rem : blk_size;
            auto entry = (Ext2DirEntry*) entries;
            while(entries_size > 0) {
                if(entry->rec_len == 0) {
                    return ds::NullOpt;
                }

                if(strncmp(name_cstr, entry->name, entry->name_len) == 0) {
                    return (RawDirEntry) {
                            .block_no = blk,
                            .offset = (uintptr_t) entry - (uintptr_t) entries,
                            .mem = entries
                    };
                }
                entry = (Ext2DirEntry*) ((char*) entry + entry->rec_len);
                entries_size -= entry->rec_len;
            }
        }
    }

    KHeap::Free(entries);
    return ds::NullOpt;

}

bool Ext2VNode::Chmod(uint16_t perms)
{
    // Only the lower 9 bits of i_mode are dedicated to permissions.
    if(perms > 0x1FF) {
        return false;
    }
    inode_.i_mode |= perms;
    return true;
}

bool Ext2VNode::ReadWrite(void *buff_param, size_t offset, size_t len,
                          bool write)
{
    char *buff_ptr    = (char*) buff_param;
    size_t block_size = mount_.GetBlockSize();
    size_t start_off = offset;

    // If the seek head is not positioned at the start of a block, read the
    // block and write to its last (seek head % block size) bytes.
    if(offset % block_size != 0) {
        size_t first_block = offset / block_size;
        ds::Optional<uint32_t> block_ind = GetOrCreateBlock(first_block, write);
        if(! block_ind) { return false; }
        char *buff = (char*) KHeap::Allocate(block_size);
        if(! buff) { return false; }
        size_t op_len = block_size - offset % block_size;

        if(write) {
            memcpy(buff + offset % block_size, buff_ptr, op_len);
            if (!mount_.ReadBlock(buff, *block_ind)) {
                KHeap::Free(buff);
                return false;
            }
        } else {
            if(! mount_.ReadBlock(buff, *block_ind)) {
                KHeap::Free(buff);
                return false;
            }
            memcpy(buff_ptr, buff + offset % block_size, op_len);
        }

        KHeap::Free(buff);
        offset += op_len;
        buff_ptr += op_len;
    }

    // (Over)write all full blocks in range [offset_, offset_ + len) with
    // contents from buff_ptr. We don't need to read.
    size_t remaining_len = len;
    while(remaining_len >= block_size) {
        size_t block_ind = offset / block_size;
        ds::Optional<ds::DynArray<Extent>> extents_opt = GetOrCreateExtents(
                block_ind, remaining_len / block_size, write);

        if(! extents_opt) {
            return false;
        }

        ds::DynArray<Extent> extents = *extents_opt;
        if(write) {
            if(! mount_.WriteBlocks(extents, buff_ptr)) {
                return false;
            }
        } else if(! mount_.ReadBlocks(extents, buff_ptr)) {
            return false;
        }

        for(int i = 0; i < extents.Size(); ++i) {
            remaining_len -= extents[i].len_ * block_size;
            buff_ptr += extents[i].len_ * block_size;
            offset += extents[i].len_ * block_size;
        }
    }

    // Write the final partial block, if one exists..
    if(remaining_len != 0) {
        size_t last_block = len / block_size;
        ds::Optional<uint32_t> block_ind = GetOrCreateBlock(last_block, write);
        char *buff = (char*) KHeap::Allocate(block_size);
        memset(buff, 0, block_size);
        if(! buff) { return false; }

        if(write) {
            memcpy(buff, buff_ptr + start_off - offset, remaining_len);
            if(! mount_.WriteBlock(buff, *block_ind)) {
                KHeap::Free(buff);
                return false;
            }
        } else {
            if (!mount_.ReadBlock(buff, *block_ind)) {
                KHeap::Free(buff);
                return false;
            }
            memcpy(buff_ptr, buff, remaining_len);
        }
        KHeap::Free(buff);
        offset += remaining_len;
    }

    return true;
}

bool Ext2VNode::MakeDirEntry(const ds::String &name,
                             const ds::RefCntPtr<VNode> &vnode)
{
    // Note: inode.i_size will always be at least block_size, because an
    // emtpy directory contains entries for "." and "..", and i_size of a
    // directory is rounded up to the size of a block.
    size_t last_blk = inode_.i_size / mount_.GetBlockSize() - 1;
    ds::Optional<uint32_t> fs_blk_no;
    if(! (fs_blk_no = GetOrCreateBlock(last_blk, true))) {
        return false;
    }

    size_t blk_size = mount_.GetBlockSize();
    void *entries = KHeap::Allocate(blk_size);
    if(! mount_.ReadBlock(entries, *fs_blk_no)) {
        KHeap::Free(entries);
        return false;
    }

    size_t blk_off = 0;
    size_t new_record_size = name.Len() + sizeof(Ext2DirEntry);
    auto entry = (Ext2DirEntry*) entries;

    // The location of a new record entry and the number of the block containing
    // it.
    Ext2DirEntry *record = nullptr;
    ds::Optional<uint32_t> records_blk = ds::NullOpt;
    while(blk_off < blk_size && ! record) {
        // The last entry of a given block points to the end of the block,
        // even if this is greater than the size of the record itself.
        size_t expected_size = sizeof(Ext2DirEntry) + entry->name_len;

        // Dir entries are aligned to 4 bytes.
        expected_size = ((expected_size + 4 - 1) / 4) * 4;
        size_t empty_space = entry->rec_len - expected_size *
                                              (entry->rec_len >= expected_size);

        // If the space between the block's last entry and the end of the block
        // is large enough to fit a new entry, place one right after the entry.
        if(empty_space > 0 && empty_space > new_record_size) {
            records_blk = fs_blk_no;
            record = (Ext2DirEntry*) ((char*) entry + expected_size);
            record->rec_len = empty_space;
            entry->rec_len = expected_size;
        }
        blk_off += entry->rec_len;
        entry = (Ext2DirEntry*) ((uint8_t*) entry + entry->rec_len);
    }

    // If the last block doesn't contain enough space for a new directory entry,
    // add a new one.
    if(! record) {
        records_blk = GetOrCreateBlock(last_blk+1, true);
        record = entry;
    }

    if(record && records_blk) {
        record->inode = vnode->GetIno();
        memcpy(record->name, name.ToChars(), name.Len());
        record->name_len  = name.Len() - 1;
        record->file_type = FileTypeToExt2FileType(vnode->GetFileType());
        record->rec_len   = blk_size;
        if (mount_.WriteBlock(entries, *records_blk)) {
            KHeap::Free(entries);
            inode_.i_size += blk_size;
            WriteBack();
            return true;
        }
    }
    return false;
}

ds::Optional<uint32_t> Ext2VNode::GetOrCreateBlock(uint32_t block_no,
                                                   bool create)
{
    ds::Optional<ds::DynArray<Extent>> extents;
    if((extents = GetOrCreateExtents(block_no, 1, create))) {
        return (*extents)[0].start_;
    }
    return ds::NullOpt;
}

ds::Optional<ds::DynArray<Extent>>
Ext2VNode::GetOrCreateExtents(uint32_t block_no, size_t len, bool create,
                              bool mdata)
{
    ds::DynArray<Extent> extents;
    int64_t parent_ind = -1;
    size_t rem_len = len;
    auto parent = (uint32_t *) KHeap::Allocate(mount_.GetBlockSize());

    while(rem_len > 0) {
        size_t ptrs_per_block = mount_.GetBlockSize() / sizeof(uint32_t);
        size_t ptrs_per_dblock = ptrs_per_block * ptrs_per_block;
        size_t ptrs_per_tblock = ptrs_per_block * ptrs_per_dblock;
        ds::Optional<uint32_t> ret = ds::NullOpt;
        if(block_no < 12) {
            if(inode_.i_block[block_no] == 0 && create) {
                if(ds::Optional<uint32_t> block_opt = mount_.AllocBlock()) {
                    inode_.i_block[block_no] = *block_opt;
                    ++inode_.i_blocks;
                } else {
                    KHeap::Free(parent);
                    return ds::NullOpt;
                }
            }

            AddToExtentList(extents, inode_.i_block[block_no]);
            --rem_len;
            ++block_no;
        } else {
            int8_t block_ind;
            uint32_t block_child_ind = block_no;
            if (block_no < 12 + ptrs_per_block) {
                block_ind = 12;
                block_child_ind -= 12;
            } else if (block_no < 12 + ptrs_per_block + ptrs_per_dblock) {
                block_ind = 13;
                block_child_ind -= 12 + ptrs_per_block;
            } else if (block_no < 12 + ptrs_per_block + ptrs_per_dblock +
                                  ptrs_per_tblock)
            {
                block_ind = 14;
                block_child_ind -= 12 + ptrs_per_block + ptrs_per_dblock;
            } else {
                KHeap::Free(parent);
                return ds::NullOpt;
            }

            if(create && inode_.i_block[block_ind] == 0) {
                if(ds::Optional<uint32_t> new_block = mount_.AllocBlock()) {
                    inode_.i_block[block_ind] = *new_block;
                } else {
                    return ds::NullOpt;
                }
            }

            if(parent_ind != inode_.i_block[block_ind]) {
                parent_ind = inode_.i_block[block_ind];
                if(! mount_.ReadBlock(parent, parent_ind)) {
                    KHeap::Free(parent);
                    return ds::NullOpt;
                }

                if(mdata) {
                    AddToExtentList(extents, parent_ind);
                }
            }

            uint8_t level = block_ind - 12;
            size_t ptrs_in_child = PtrsPerIndirectBlock(level);
            size_t read_size = (ptrs_in_child < rem_len ? ptrs_in_child : rem_len);
            int64_t blks_read = GetOrCreateExtents(parent, block_child_ind,
                                                   read_size, extents,
                                                   block_ind - 12, create,
                                                   mdata);
            block_no += blks_read;
            rem_len -= blks_read;

            if(create) {
                mount_.WriteBlock(parent, inode_.i_block[block_ind]);
            }

            if(blks_read < 0) {
                KHeap::Free(parent);
                return ds::NullOpt;
            }
        }
    }

    if(parent) {
        KHeap::Free(parent);
    }
    return extents;

}

int64_t Ext2VNode::GetOrCreateExtents(uint32_t *parent, uint32_t block_no,
                                      size_t len, ds::DynArray<Extent> &extents,
                                      uint8_t level, bool create, bool mdata)
{
    size_t ptrs_in_child = PtrsPerIndirectBlock(level - 1);
    uint32_t blocks_read = 0;

    if(level == 0) {
        for(uint32_t i = 0; i < len; ++i, ++blocks_read) {
            uint32_t ind = block_no + i;
            if(create && parent[ind] == 0) {
                if(ds::Optional<uint32_t> free_block = mount_.AllocBlock()) {
                    parent[ind] = *free_block;
                    ++inode_.i_blocks;
                } else {
                    return -1;
                }
            }

            if(! parent[ind]) {
                Log("\tFailed to read %d, showing full block:\n", i);
                for(uint32_t j = 0; j < len; ++j) {
                    Log("%d,", parent[ind]);
                }
                return -1;
            }

            AddToExtentList(extents, parent[ind]);
        }
    } else {
        while(blocks_read < len) {
            uint32_t parent_ind = block_no / ptrs_in_child;
            if (create && block_no % ptrs_in_child == 0 &&
                parent[parent_ind] == 0)
            {
                if(ds::Optional<uint32_t> new_parent = mount_.AllocBlock()) {
                    parent[parent_ind] = *new_parent;
                    //++inode_.i_blocks;
                } else {
                    return -1;
                }
            }

            if (auto buff = (uint32_t *) KHeap::Allocate(mount_.GetBlockSize()))
            {
                // Read child into RAM and alter it.
                if (mount_.ReadBlock(buff, parent[parent_ind])) {
                    if(mdata) {
                        AddToExtentList(extents, parent[parent_ind]);
                    }

                    uint32_t child_ind = block_no % ptrs_in_child;

                    size_t new_len = (ptrs_in_child < len ? ptrs_in_child : len);
                    int64_t sub_blocks_read = GetOrCreateExtents(
                            buff, child_ind, new_len, extents, level - 1,
                            create, mdata);
                    if(sub_blocks_read < 0) {
                        return sub_blocks_read;
                    }

                    len -= sub_blocks_read;
                    blocks_read += sub_blocks_read;
                    block_no += sub_blocks_read;

                    // Write back child block if get or create block call was a
                    // success.
                    if (create) {
                        mount_.WriteBlock(buff, parent[parent_ind]);
                    }
                }
                KHeap::Free(buff);
            } else {
                return -1;
            }
        }
    }

    return blocks_read;
}

void Ext2VNode::AddToExtentList(ds::DynArray<Extent> &extents, uint32_t block)
{
    if(extents.Size()) {
        Extent last_extent = extents.Back();
        if(block == last_extent.start_ + last_extent.len_) {
            ++extents[-1].len_;
            return;
        }
    }

    extents.Append({ block, 1 });
}

void Ext2VNode::Prealloc()
{
    if(GetExt2FileType() == EXT2_S_IFREG) {
        uint8_t prealloc_blocks = mount_.GetPreallocBlocks();
        GetOrCreateExtents(0, prealloc_blocks, true);
    } else if(GetExt2FileType() == EXT2_S_IFDIR) {
        uint8_t prealloc_dir_blocks = mount_.GetPreallocDirBlocks();
        GetOrCreateExtents(0, prealloc_dir_blocks, true);
    }
}

size_t Ext2VNode::PtrsPerIndirectBlock(uint8_t level)
{
    size_t ptrs_per_direct_block = mount_.GetBlockSize() / sizeof(uint32_t);
    size_t ptrs_per_inddirect_block = ptrs_per_direct_block;
    for(uint8_t i = 0; i < level; ++i) {
        ptrs_per_inddirect_block *= ptrs_per_direct_block;
    }
    return ptrs_per_inddirect_block;
}

bool Ext2VNode::InitializeDir(uint32_t parent_ino)
{
    bool ret = false;
    if(ds::Optional<uint32_t> blk_opt = GetOrCreateBlock(0, true)) {
        size_t blk_size = mount_.GetBlockSize();
        if(auto entries = (char*) KHeap::Allocate(blk_size)) {
            auto dir_entry = (Ext2DirEntry*) entries;
            uint16_t dir_entry_len = sizeof(Ext2DirEntry) + 1;
            dir_entry_len = ((dir_entry_len + 4 - 1) / 4) * 4;

            *dir_entry = (Ext2DirEntry) {
                    .inode = ino_,
                    .rec_len = dir_entry_len,
                    .name_len = 1,
                    .file_type = EXT2_FT_DIR
            };
            dir_entry->name[0] = '.';


            auto parent_entry = (Ext2DirEntry*) ((char*) entries + dir_entry_len);
            uint16_t parent_entry_len = blk_size - dir_entry_len;
            *parent_entry = (Ext2DirEntry) {
                    .inode = parent_ino,
                    .rec_len = parent_entry_len,
                    .name_len = 2,
                    .file_type = EXT2_FT_DIR
            };
            parent_entry->name[0] = parent_entry->name[1] = '.';
            inode_.i_size = blk_size;
            ++inode_.i_links_count;

            if(mount_.WriteBlock(entries, *blk_opt) && WriteBack()) {
                ret = true;
            }

            KHeap::Free(entries);
        }
    }
    return ret;
}

ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Link(const ds::String &name,
                                                   uint32_t ino)
{
    if(ds::Optional<ds::RefCntPtr<VNode>> src_file_opt = mount_.GetVNode(ino)) {
        auto src_file = *src_file_opt;
        if(MakeDirEntry(name, src_file)) {
            src_file->WriteBack();
            return src_file;
        }
    }

    return ds::NullOpt;
}

bool Ext2VNode::Unlink(const ds::String &name)
{
    // For ext2 in particular, a hard link is just a distinct directory entry
    // pointing to an inode, so we remove the dir entry. In other FSs, it can
    // be more complicated, so distinguishing between removal/unlinking in base
    // class is worthwhile.
    return Remove(name);
}

bool Ext2VNode::SymLink(const ds::String &name, const ds::String &target)
{
    // Make vnode.
    Ext2VNode *vnode;
    if(! (vnode = mount_.AllocVNode(EXT2_S_IFLNK))) {
        return false;
    }

    const char *target_buff = target.ToChars();
    if(target.Len() < 60) {
        memcpy(&vnode->inode_.i_block, target_buff, target.Len());
    } else {
        vnode->Write((void*) target_buff, 0, target.Len());
    }

    ds::RefCntPtr<VNode> vnode_ptr = ds::MakeRefCntPtr((VNode*) vnode);
    return MakeDirEntry(name, vnode_ptr);
}

bool Ext2VNode::Mount(const ds::String &mntpt, const ds::RefCntPtr<VNode> &root)
{
    return mnts_.Insert(mntpt, root);
}

bool Ext2VNode::Unmount(const ds::String &mntpt)
{
    return mnts_.Delete(mntpt);
}

size_t Ext2VNode::GetNumMounts() const
{
    return mnts_.Len();
}
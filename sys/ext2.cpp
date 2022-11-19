//#include "ext2.h"
//#include <libc/string.h>
//
//static Ext2FileType FileModeToFileType(Ext2FileMode mode)
//{
//    switch(mode) {
//        case EXT2_S_IFSOCK: {
//            return EXT2_FT_SOCK;
//        }
//        case EXT2_S_IFLNK: {
//            return EXT2_FT_SYMLINK;
//        }
//        case EXT2_S_IFREG: {
//            return EXT2_FT_REG_FILE;
//        }
//        case EXT2_S_IFBLK: {
//            return EXT2_FT_BLKDEV;
//        }
//        case EXT2_S_IFDIR: {
//            return EXT2_FT_DIR;
//        }
//        case EXT2_S_IFCHR: {
//            return EXT2_FT_CHRDEV;
//        }
//        case EXT2_S_IFIFO: {
//            return EXT2_FT_FIFO;
//        }
//        default: {
//            return EXT2_FT_UNKNOWN;
//        }
//    }
//}
//
//static Ext2FileMode FileTypeToFileMode(Ext2FileType type)
//{
//    switch(type) {
//        case EXT2_FT_SOCK: {
//            return EXT2_S_IFSOCK;
//        }
//        case EXT2_FT_SYMLINK: {
//            return EXT2_S_IFLNK;
//        }
//        case EXT2_FT_REG_FILE: {
//            return EXT2_S_IFREG;
//        }
//        case EXT2_FT_BLKDEV: {
//            return EXT2_S_IFBLK;
//        }
//        case EXT2_FT_DIR: {
//            return EXT2_S_IFDIR;
//        }
//        case EXT2_FT_CHRDEV: {
//            return EXT2_S_IFCHR;
//        }
//        case EXT2_FT_FIFO: {
//            return EXT2_S_IFIFO;
//        }
//        // This is actually outside the spec, but it stops GCC from complaining
//        // about lack of default case and produces desired behavior regardless.
//        default: {
//            return EXT2_S_NONE;
//        }
//    }
//}
//
//Ext2Mount::Ext2Mount(SATAPort *disk, uint64_t partition_base_sector)
//        : partition_base_sector_(partition_base_sector)
//        , disk_(disk)
//        , super_((Ext2SuperBlock*) (disk_->Read(partition_base_sector_+2, 1)))
//        , block_size_(1024 << super_->s_log_block_size)
//        , sectors_per_block_(block_size_ / 512)
//        , bgdt_(disk, partition_base_sector + 2 + (block_size_ >= 2048 ? 1 : 2),
//                sectors_per_block_)
//        , inode_tab_cache_(&Ext2Mount::WriteBackDiskMem<Ext2INode>)
//        , inode_bmap_cache_(&Ext2Mount::WriteBackDiskMem<uint64_t>)
//        , block_bmap_cache_(&Ext2Mount::WriteBackDiskMem<uint64_t>)
//{}
//
//uint32_t Ext2Mount::GetBlockSize() const
//{
//    return block_size_;
//}
//
//uint32_t Ext2Mount::GetSectorsPerBlock() const
//{
//    return sectors_per_block_;
//}
//
//uint8_t Ext2Mount::GetPreallocBlocks() const
//{
//    return super_->s_prealloc_blocks;
//}
//
//uint8_t Ext2Mount::GetPreallocDirBlocks() const
//{
//    return super_->s_prealloc_dir_blocks * (super_->s_feature_compat & 1);
//}
//
//ds::Optional<Ext2INode> Ext2Mount::ReadINode(uint32_t ino)
//{
//    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
//    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;
//
//    uint32_t tab_block_off = (local_ind * super_->s_inode_size) / block_size_;
//    uint32_t block_no = bgdt_[block_group].bg_inode_table + tab_block_off;
//    ds::OwningPtr<uint8_t> inode_tab = ReadBlock(block_no);
//
//    if(inode_tab) {
//        uint32_t inode_off = (local_ind * super_->s_inode_size) % block_size_;
//        const uint8_t *inode_tab_bytes = inode_tab.ToRawPtr();
//        auto inode = (Ext2INode*) (inode_tab_bytes + inode_off);
//
//        return *inode;
//    }
//
//    return ds::NullOpt;
//}
//
//bool Ext2Mount::WriteINode(uint32_t ino, const Ext2INode &inode)
//{
//    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
//    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;
//
//    uint32_t tab_block_off = (local_ind * super_->s_inode_size) / block_size_;
//    uint32_t block_no = bgdt_[block_group].bg_inode_table + tab_block_off;
//    ds::OwningPtr<uint8_t> inode_tab = ReadBlock(block_no);
//
//    if(inode_tab) {
//        uint32_t inode_off = (local_ind * super_->s_inode_size) % block_size_;
//        const uint8_t *inode_tab_bytes = inode_tab.ToRawPtr();
//        auto inode_ptr = (Ext2INode*) (inode_tab_bytes + inode_off);
//        *inode_ptr = inode;
//
//        if(WriteBlock((void*) inode_tab_bytes, block_no)) {
//            return true;
//        }
//    }
//
//    return false;
//}
//
//VNode *Ext2Mount::ReadVNode(uint32_t ino)
//{
//    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
//    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;
//
//    ds::Optional<DiskMem<Ext2INode>> inode_tab_opt;
//    if((inode_tab_opt = ReadINodeTable(block_group))) {
//        size_t inode_offset = local_ind * super_->s_inode_size;
//        DiskMem<Ext2INode> inode_tab = *inode_tab_opt;
//        const char *inode_tab_bytes = inode_tab.ToBytes();
//        auto inode = (Ext2INode*) (inode_tab_bytes + inode_offset);
//        return new Ext2VNode(*inode, ino, *this);
//    }
//    return nullptr;
//}
//
//
//VNode *Ext2Mount::ReadRootDir()
//{
//    return ReadVNode(2);
//}
//
//Ext2VNode *Ext2Mount::AllocVNode(Ext2FileMode type, uint16_t perms,
//                                 uint16_t uid, uint16_t gid)
//{
//    // Write back superblock at some point.
//    size_t no_bgroups = super_->s_blocks_count / super_->s_blocks_per_group;
//    int64_t free_inode = -1;
//    for(uint32_t i = 0 ; i < no_bgroups && free_inode == -1; ++i) {
//        if(bgdt_[i].bg_free_inodes_count) {
//            if(auto inode_bmap_opt = ReadINodeBitmap(i)) {
//                DiskMem<uint64_t> inode_bmap = *inode_bmap_opt;
//                for(uint32_t j = 0; j < block_size_ / 8; ++j) {
//                    if(inode_bmap[j] != 0xFFFFFFFFFFFFFFFF) {
//                        // Ino 1 is not valid.
//                        uint64_t free_inos = (i == 0 && j == 0) ?
//                                      ~(inode_bmap[j] | 1) : ~(inode_bmap[j]);
//                        // Branchless bmap lookup; if all inodes are free,
//                        // just ignore (undefined) clzl result and return
//                        // first inode in group. Otherwise, return first
//                        // index of first unset bit in bitmap.
//                        uint64_t unset = __builtin_ffsll(free_inos);
//                        uint32_t bg_inode_base = i * super_->s_inodes_per_group;
//                        free_inode = bg_inode_base + (j * 64) + unset;
//                        inode_bmap[j] |= (1ULL << (unset - 1));
//                        --bgdt_[i].bg_free_inodes_count;
//                        --super_->s_free_inodes_count;
//                        bgdt_.WriteBack();
//                        inode_bmap.WriteBack();
//                        break;
//                    }
//                }
//            }
//        }
//    }
//
//    if(free_inode == -1) { return nullptr; }
//    Ext2INode new_inode = {};
//    new_inode.i_mode = type | (perms > 0x1FF ? 0 : perms);
//    new_inode.i_links_count = 1;
//    new_inode.i_uid = uid;
//    new_inode.i_gid = gid;
//    Log("\n\nALLOCATED INO IS %d\n\n", free_inode);
//    if(free_inode == 82) {
//        Log("BP\n");
//    }
//    return new Ext2VNode(new_inode, free_inode, *this);
//}
//
//ds::Optional<uint32_t> Ext2Mount::AllocBlock()
//{
//    // Write back superblock at some point.
//    size_t no_bgroups = super_->s_blocks_count / super_->s_blocks_per_group;
//    int64_t free_block = -1;
//    for(uint32_t i = 0 ; i < no_bgroups && free_block == -1; ++i) {
//        if(bgdt_[i].bg_free_blocks_count) {
//            if(auto block_bmap_opt = ReadBlockBitmap(i)) {
//                DiskMem<uint64_t> block_bmap = *block_bmap_opt;
//                for(uint32_t j = 0; j < block_size_ / 8; ++j) {
//                    if(block_bmap[j] != 0xFFFFFFFFFFFFFFFF) {
//                        uint64_t free_blocks = i == 0 && j == 0 ?
//                                ~(block_bmap[j] | 1) : ~(block_bmap[j]);
//                        uint64_t unset = __builtin_ffsll(free_blocks);
//                        uint32_t bg_inode_base = i * super_->s_blocks_per_group;
//                        free_block = bg_inode_base + (j * 64) + unset;
//                        //Log("\t unset %d, before 0x%x, ", unset, block_bmap[j]);
//                        block_bmap[j] |= (1ULL << (unset - 1));
//                        //Log("after 0x%x\n", block_bmap[j]);
//                        block_bmap.WriteBack();
//                        --bgdt_[i].bg_free_blocks_count;
//                        break;
//                    }
//                }
//            }
//        }
//    }
//
//    if(free_block == -1) { return ds::NullOpt; }
//    //Log("Free block %d\n", free_block);
//    return free_block + 1;
//}
//
//
//bool Ext2Mount::DeleteINode(uint32_t ino)
//{
//    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
//    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;
//
//    if(auto inode_bmap_opt = ReadINodeBitmap(block_group)) {
//        DiskMem<uint64_t> inode_bmap = *inode_bmap_opt;
//        inode_bmap[local_ind / 64] &= ~(1ULL << (local_ind % 64));
//        return true;
//    }
//
//    return false;
//}
//
//bool Ext2Mount::FreeBlock(uint32_t block_no)
//{
//    uint32_t block_group = (block_no - 1) / super_->s_blocks_per_group;
//    uint32_t local_ind = (block_no - 1) % super_->s_blocks_per_group;
//    if(auto block_bmap_opt = ReadBlockBitmap(block_group)) {
//        DiskMem<uint64_t> block_bmap = *block_bmap_opt;
//        block_bmap[local_ind / 64] &= ~(1ULL << (local_ind % 64));
//        return true;
//    }
//    return false;
//}
//
//bool Ext2Mount::ReadBlock(void *buff, uint32_t block, size_t num_blocks) const
//{
//    if(block == 0) {
//        return false;
//    }
//
//    uint32_t sector_no = (block - 1) * sectors_per_block_;
//    uint32_t num_sectors = (num_blocks * sectors_per_block_);
//    return disk_->Read(partition_base_sector_ + 2 + sector_no, num_sectors,
//                       buff);
//}
//
//
//bool Ext2Mount::WriteBlock(void *buff, uint32_t block, size_t num_blocks)
//{
//    if(block == 0) {
//        return false;
//    }
//
//    uint32_t sector_no = (block - 1) * sectors_per_block_;
//    uint32_t num_sectors = (num_blocks * sectors_per_block_);
//    return disk_->Write(partition_base_sector_ + 2 + sector_no, num_sectors,
//                        buff);
//}
//
//ds::OwningPtr<uint8_t>
//Ext2Mount::ReadBlock(uint32_t block, size_t num_blocks) const
//{
//    auto buff = (uint8_t*) KHeap::Allocate(block_size_ * num_blocks);
//    uint32_t sector_no = (block - 1) * sectors_per_block_;
//    uint32_t num_sectors = (num_blocks * sectors_per_block_);
//    if(disk_->Read(partition_base_sector_ + 2 + sector_no, num_sectors, buff)) {
//        return ds::OwningPtr<uint8_t>(buff);
//    }
//    return nullptr;
//}
//
//bool Ext2Mount::ReadBlocks(const ds::DynArray<Extent> &extents, void *buff)
//{
//    ds::DynArray<DiskRange> ranges;
//    uint64_t first_block = partition_base_sector_ + 2;
//    for(int i = 0; i < extents.Size(); ++i) {
//        uint32_t sector_no = (extents[i].start_ - 1) * sectors_per_block_;
//        uint64_t start_lba =  first_block + sector_no;
//        ranges.Append({ start_lba, extents[i].len_ * sectors_per_block_ });
//    }
//
//    return disk_->Read(ranges, buff);
//}
//
//bool Ext2Mount::WriteBlocks(const ds::DynArray<Extent> &extents, void *buff)
//{
//    ds::DynArray<DiskRange> ranges;
//    uint64_t first = partition_base_sector_ + 2;
//    for(int i = 0; i < extents.Size(); ++i) {
//        Log("Extent %d-%d\n", extents[i].start_,
//            extents[i].start_ + extents[i].len_);
//        uint64_t sector_no = (extents[i].start_ - 1) * sectors_per_block_;
//        uint64_t start_lba =  first + sector_no;
//        ranges.Append({ start_lba, extents[i].len_ * sectors_per_block_ });
//    }
//
//    return disk_->Write(ranges, buff);
//}
//
//bool Ext2Mount::AppendToArr(ds::DynArray<uint32_t> &block_arr,
//                            uint32_t block_no)
//{
//    block_arr.Append(block_no);
//    return true;
//}
//
//ds::Optional<DiskMem<Ext2INode>>
//Ext2Mount::ReadINodeTable(uint32_t block_group_no)
//{
//    ds::Optional<DiskMem<Ext2INode>> tab;
//    if((tab = inode_tab_cache_.Lookup(block_group_no))) {
//        return tab;
//    }
//
//    Ext2BlockGroupDescriptor bgdt_entry = bgdt_[block_group_no];
//    uint32_t table_block_no = bgdt_entry.bg_inode_table;
//    uint64_t fs_block_no = sectors_per_block_ * (table_block_no - 1);
//    uint64_t start_lba = partition_base_sector_ + 2 + fs_block_no;
//    uint64_t free_inodes = bgdt_entry.bg_free_inodes_count;
//    uint64_t used_inodes = (super_->s_inodes_per_group - free_inodes);
//    uint64_t tab_size = (used_inodes * super_->s_inode_size);
//    uint64_t tab_sectors = (tab_size / block_size_) * sectors_per_block_;
//
//    DiskMem<Ext2INode> table(disk_, start_lba, tab_sectors);
//    if(table) {
//        inode_tab_cache_.Insert(block_group_no, (table));
//        return table;
//    }
//
//    return ds::NullOpt;
//}
//
//ds::Optional<DiskMem<uint64_t>>
//Ext2Mount::ReadINodeBitmap(uint32_t block_group_no)
//{
//    ds::Optional<DiskMem<uint64_t>> cache_bmap;
//    if((cache_bmap = inode_bmap_cache_.Lookup(block_group_no))) {
//        return cache_bmap;
//    }
//
//    uint32_t bmap_block_no = bgdt_[block_group_no].bg_inode_bitmap;
//    uint64_t bmap_sector = partition_base_sector_ + 2 + (bmap_block_no - 1) *
//                                                        sectors_per_block_;
//    DiskMem<uint64_t> bmap(disk_, bmap_sector, sectors_per_block_);
//    Log("BMAP SECTOR: %d\n", bmap_sector);
//    if(bmap) {
//        inode_bmap_cache_.Insert(block_group_no, bmap);
//        return bmap;
//    }
//
//    return ds::NullOpt;
//}
//
//ds::Optional<DiskMem<uint64_t>>
//Ext2Mount::ReadBlockBitmap(uint32_t block_group_no)
//{
//    ds::Optional<DiskMem<uint64_t>> cache_bmap;
//    if((cache_bmap = block_bmap_cache_.Lookup(block_group_no))) {
//        return cache_bmap;
//    }
//
//    uint32_t bmap_block_no = bgdt_[block_group_no].bg_block_bitmap;
//    uint64_t bmap_sector = partition_base_sector_ + 2 + (bmap_block_no - 1) *
//            sectors_per_block_;
//    DiskMem<uint64_t> bmap(disk_, bmap_sector, sectors_per_block_);
//    if(bmap) {
//        block_bmap_cache_.Insert(block_group_no, bmap);
//        return bmap;
//    }
//
//    return ds::NullOpt;
//}
//
//
//Ext2VNode::Ext2VNode(Ext2INode inode, uint32_t ino, class Ext2Mount &mount)
//        : inode_(inode)
//        , ino_(ino)
//        , offset_(0)
//        , mount_(mount)
//{}
//
//Ext2VNode::Ext2VNode(uint32_t ino, class Ext2Mount &mount)
//        : inode_{}
//        , ino_(ino)
//        , offset_(0)
//        , mount_(mount)
//{
//    if(ds::Optional<Ext2INode> inode_opt = mount.ReadINode(ino)) {
//        inode_ = *inode_opt;
//    }
//}
//
//Ext2VNode::~Ext2VNode()
//{
//    WriteBack();
//}
//
//bool Ext2VNode::WriteBack() const
//{
//    Log("Writing back\n");
//    if(! mount_.WriteINode(ino_, inode_)) {
//        return false;
//    }
//    return true;
//}
//
//uint32_t Ext2VNode::GetIno() const
//{
//    return ino_;
//}
//
//FileType Ext2VNode::GetFileType() const
//{
//    switch(GetExt2FileType()) {
//        case EXT2_S_IFREG: {
//            return FileType::REGULAR;
//        }
//        case EXT2_S_IFDIR: {
//            return FileType::DIRECTORY;
//        }
//        case EXT2_S_IFLNK: {
//            return FileType::SYMLINK;
//        }
//        case EXT2_S_IFBLK: {
//            return FileType::BLOCK_DEV;
//        }
//        case EXT2_S_IFCHR: {
//            return FileType::CHAR_DEV;
//        }
//        case EXT2_S_IFIFO: {
//            return FileType::NAMED_PIPE;
//        }
//        case EXT2_S_IFSOCK: {
//            return FileType::SOCKET;
//        }
//    }
//    return FileType::OTHER;
//}
//
//uint64_t Ext2VNode::GetLength() const
//{
//    return inode_.i_size;
//}
//
//uint16_t Ext2VNode::GetPerms() const
//{
//    return inode_.i_mode & 0x1FF;
//}
//
//uint16_t Ext2VNode::GetExt2FileType() const {
//    return inode_.i_mode & 0xF000;
//}
//
//bool Ext2VNode::Seek(size_t offset)
//{
//    if((offset) >= (inode_.i_blocks * mount_.GetBlockSize())) {
//        return false;
//    }
//    offset_ = offset;
//    return true;
//}
//
//bool Ext2VNode::Read(void *buffer, size_t len)
//{
//    return ReadWrite(buffer, len, false);
//}
//
//bool Ext2VNode::Write(void *buffer, size_t len)
//{
//    if(ReadWrite(buffer, len, true)) {
//        inode_.i_size += (offset_ - inode_.i_size) * (offset_ > inode_.i_size);
//        WriteBack();
//        return true;
//    }
//    return false;
//}
//
//ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::MkDir(const ds::String &name)
//{
//    return MakeDirEntry(name, EXT2_S_IFDIR);
//}
//
//ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Touch(const ds::String &name)
//{
//    return MakeDirEntry(name, EXT2_S_IFREG);
//}
//
//ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>> Ext2VNode::ReadDir()
//{
//    if(GetExt2FileType() != EXT2_S_IFDIR) {
//        return ds::NullOpt;
//    }
//
//    size_t block_size = mount_.GetBlockSize();
//    ds::HashMap<ds::String, ds::RefCntPtr<VNode>> vnodes;
//    void *entries = KHeap::Allocate(mount_.GetBlockSize());
//
//    size_t rem = inode_.i_size;
//    for(size_t blk = 0; rem > 0; ++blk) {
//        ds::Optional<uint32_t> fs_blk_no;
//        if(! (fs_blk_no = GetOrCreateBlock(blk, false))) {
//            KHeap::Free(entries);
//            return ds::NullOpt;
//        }
//
//        if(! mount_.ReadBlock(entries, *fs_blk_no)) {
//            KHeap::Free(entries);
//            return ds::NullOpt;
//        }
//
//        auto entry = (Ext2DirEntry*) entries;
//        size_t block_off = 0;
//        Log("Dir size %d\n", inode_.i_size);
//        while(block_off < block_size && rem) {
//            if(entry->inode) {
//                ds::String name(entry->name, entry->name_len);
//                Log("%s (ino %d), len %d\n", name, entry->inode, entry->rec_len);
//                VNode *vnode = new Ext2VNode(entry->inode, mount_);
//                vnodes.Insert(name, ds::MakeRefCntPtr(vnode));
//            }
//            block_off += entry->rec_len;
//            rem -= entry->rec_len;
//            entry = (Ext2DirEntry*) ((uint8_t*) entry + entry->rec_len);
//        }
//
//        if(block_off < block_size && !entry->inode) {
//            break;
//        }
//    }
//
//    KHeap::Free(entries);
//    return vnodes;
//}
//
//ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Lookup(const ds::String &name)
//{
//    if(ds::Optional<RawDirEntry> raw_entry_opt = GetDirEntry(name)) {
//        RawDirEntry raw_entry = *raw_entry_opt;
//        auto entry = (Ext2DirEntry*) ((char*) raw_entry.mem + raw_entry.offset);
//        VNode *vnode = new Ext2VNode(entry->inode, mount_);
//        KHeap::Free(raw_entry.mem);
//        return ds::MakeRefCntPtr(vnode);
//    }
//
//    return ds::NullOpt;
//}
//
//bool Ext2VNode::Remove(const ds::String &child_name)
//{
//    ds::Optional<RawDirEntry> raw_entry_opt;
//    if(! (raw_entry_opt = GetDirEntry(child_name))) {
//        return false;
//    }
//
//    RawDirEntry raw_entry = *raw_entry_opt;
//    auto entry = (Ext2DirEntry*) ((char*) raw_entry.mem + raw_entry.offset);
//    uint32_t child_ino = entry->inode;
//    entry->inode = 0;
//    memset(entry->name, 0, entry->name_len);
//
//    if(! mount_.WriteBlock(raw_entry.mem, raw_entry.block_no)) {
//        return false;
//    }
//
//    Ext2VNode child(child_ino, mount_);
//    // Only delete actual inode/contents if no further links exist.
//    if(--child.inode_.i_links_count > 0) {
//        child.WriteBack();
//        return true;
//    }
//
//    ds::Optional<ds::DynArray<Extent>> exts_opt;
//    size_t no_data_blks = child.GetLength() / mount_.GetBlockSize();
//    if(! (exts_opt = child.GetOrCreateExtents(0, no_data_blks, false, true))) {
//        return false;
//    }
//
//    ds::DynArray<Extent> exts = *exts_opt;
//    for(int ext = 0; ext < exts.Size(); ++ext) {
//        uint32_t final_blk = exts[ext].start_ + exts[ext].len_;
//        for(uint32_t blk = exts[ext].start_; blk < final_blk; ++blk) {
//            if(! mount_.FreeBlock(blk)) {
//                return false;
//            }
//        }
//    }
//
//    if(! mount_.WriteINode(child_ino, {})) {
//        return false;
//    }
//    return true;
//}
//
//ds::Optional<Ext2VNode::RawDirEntry>
//Ext2VNode::GetDirEntry(const ds::String &name)
//{
//    if(GetExt2FileType() != EXT2_S_IFDIR) {
//        return ds::NullOpt;
//    }
//
//    ds::Optional<ds::DynArray<Extent>> exts_opt;
//    ds::DynArray<Extent> exts;
//    if((exts_opt = GetOrCreateExtents(0, inode_.i_blocks, false))) {
//        exts = *exts_opt;
//    } else {
//        return ds::NullOpt;
//    }
//
//    size_t rem = inode_.i_size;
//    size_t blk_size = mount_.GetBlockSize();
//    void *entries = KHeap::Allocate(mount_.GetBlockSize());
//    const char *name_cstr = name.ToChars();
//
//    for(int ext = 0; ext < exts.Size(); ++ext) {
//        size_t final_blk = exts[ext].start_ + exts[ext].len_;
//        for(uint32_t blk = exts[ext].start_; blk < final_blk; ++blk) {
//            if(! mount_.ReadBlock(entries, blk)) {
//                KHeap::Free(entries);
//                return ds::NullOpt;
//            }
//
//            size_t entries_size = rem < blk_size ? rem : blk_size;
//            auto entry = (Ext2DirEntry*) entries;
//            while(entries_size > 0) {
//                if(entry->rec_len == 0) {
//                    return ds::NullOpt;
//                }
//
//                if(strncmp(name_cstr, entry->name, entry->name_len) == 0) {
//                    return (RawDirEntry) {
//                        .block_no = blk,
//                        .offset = (uintptr_t) entry - (uintptr_t) entries,
//                        .mem = entries
//                    };
//                }
//                entry = (Ext2DirEntry*) ((char*) entry + entry->rec_len);
//                entries_size -= entry->rec_len;
//            }
//        }
//    }
//
//    KHeap::Free(entries);
//    return ds::NullOpt;
//
//}
//
//bool Ext2VNode::Chmod(uint16_t perms)
//{
//    // Only the lower 9 bits of i_mode are dedicated to permissions.
//    if(perms > 0x1FF) {
//        return false;
//    }
//    inode_.i_mode |= perms;
//    return true;
//}
//
//bool Ext2VNode::ReadWrite(void *buff_param, size_t len, bool write)
//{
//    char *buff_ptr    = (char*) buff_param;
//    size_t block_size = mount_.GetBlockSize();
//    size_t start_off = offset_;
//
//    // If the seek head is not positioned at the start of a block, read the
//    // block and write to its last (seek head % block size) bytes.
//    if(offset_ % block_size != 0) {
//        size_t first_block = offset_ / block_size;
//        ds::Optional<uint32_t> block_ind = GetOrCreateBlock(first_block, write);
//        if(! block_ind) { return false; }
//        char *buff = (char*) KHeap::Allocate(block_size);
//        if(! buff) { return false; }
//        size_t op_len = block_size - offset_ % block_size;
//
//        if(write) {
//            memcpy(buff + offset_ % block_size, buff_ptr, op_len);
//            if (!mount_.ReadBlock(buff, *block_ind)) {
//                KHeap::Free(buff);
//                return false;
//            }
//        } else {
//            if(! mount_.ReadBlock(buff, *block_ind)) {
//                KHeap::Free(buff);
//                return false;
//            }
//            memcpy(buff_ptr, buff + offset_ % block_size, op_len);
//        }
//
//        KHeap::Free(buff);
//        offset_ += op_len;
//        buff_ptr += op_len;
//    }
//
//    // (Over)write all full blocks in range [offset_, offset_ + len) with
//    // contents from buff_ptr. We don't need to read.
//    size_t remaining_len = len;
//    while(remaining_len >= block_size) {
//        size_t block_ind = offset_ / block_size;
//        ds::Optional<ds::DynArray<Extent>> extents_opt = GetOrCreateExtents(
//                block_ind, remaining_len / block_size, write);
//
//        if(! extents_opt) {
//            return false;
//        }
//
//        ds::DynArray<Extent> extents = *extents_opt;
//        if(write) {
//            if(! mount_.WriteBlocks(extents, buff_ptr)) {
//                return false;
//            }
//        } else if(! mount_.ReadBlocks(extents, buff_ptr)) {
//            return false;
//        }
//
//        for(int i = 0; i < extents.Size(); ++i) {
//            remaining_len -= extents[i].len_ * block_size;
//            buff_ptr += extents[i].len_ * block_size;
//            offset_ += extents[i].len_ * block_size;
//        }
//    }
//    Log("Out of loop, rem 0x%x\n", remaining_len);
//
//    // Write the final partial block, if one exists..
//    if(remaining_len != 0) {
//        size_t last_block = offset_ / block_size;
//        ds::Optional<uint32_t> block_ind = GetOrCreateBlock(last_block, write);
//        char *buff = (char*) KHeap::Allocate(block_size);
//        memset(buff, 0, block_size);
//        if(! buff) { return false; }
//
//        Log("Final block is %d\n", *block_ind);
//        if(write) {
//            memcpy(buff, buff_ptr + start_off - offset_, remaining_len);
//            if(! mount_.WriteBlock(buff, *block_ind)) {
//                KHeap::Free(buff);
//                return false;
//            }
//        } else {
//            if (!mount_.ReadBlock(buff, *block_ind)) {
//                KHeap::Free(buff);
//                return false;
//            }
//            memcpy(buff_ptr, buff, remaining_len);
//        }
//        KHeap::Free(buff);
//        offset_ += remaining_len;
//    }
//
//    return true;
//}
//
//ds::Optional<ds::RefCntPtr<VNode>>
//Ext2VNode::MakeDirEntry(const ds::String &name, Ext2FileMode fmode,
//                        Ext2VNode *vnode)
//{
//    if(GetExt2FileType() != EXT2_S_IFDIR) {
//        return ds::NullOpt;
//    }
//
//    if (! vnode) {
//        if (! (vnode = mount_.AllocVNode(fmode))) {
//            return ds::NullOpt;
//        }
//    }
//
//    // Note: inode.i_size will always be at least block_size, because an
//    // emtpy directory contains entries for "." and "..", and i_size of a
//    // directory is rounded up to the size of a block.
//    size_t last_blk = inode_.i_size / mount_.GetBlockSize() - 1;
//    ds::Optional<uint32_t> fs_blk_no;
//    if(! (fs_blk_no = GetOrCreateBlock(last_blk, true))) {
//        return ds::NullOpt;
//    }
//
//    size_t blk_size = mount_.GetBlockSize();
//    void *entries = KHeap::Allocate(blk_size);
//    if(! mount_.ReadBlock(entries, *fs_blk_no)) {
//        KHeap::Free(entries);
//        return ds::NullOpt;
//    }
//
//    size_t blk_off = 0;
//    size_t new_record_size = name.Len() + sizeof(Ext2DirEntry);
//    auto entry = (Ext2DirEntry*) entries;
//    while(blk_off < blk_size) {
//        // The last entry of a given block points to the end of the block,
//        // even if this is greater than the size of the record itself.
//        size_t expected_size = sizeof(Ext2DirEntry) + entry->name_len;
//
//        // Dir entries are aligned to 4 bytes.
//        expected_size = ((expected_size + 4 - 1) / 4) * 4;
//        size_t empty_space = entry->rec_len - expected_size *
//                              (entry->rec_len >= expected_size);
//
//        if(empty_space > blk_size) {
//            Log("BP2");
//        }
//
//        // If the space between the block's last entry and the end of the block
//        // is large enough to fit a new entry, place one right after the entry.
//        if(empty_space > 0 && empty_space > new_record_size) {
//            auto new_record = (Ext2DirEntry*) ((char*) entry + expected_size);
//            new_record->inode = vnode->GetIno();
//            memcpy(new_record->name, name.ToChars(), name.Len());
//            new_record->name_len = name.Len() - 1;
//            new_record->file_type = FileModeToFileType(fmode);
//
//            // Make sure former last record points to new record rather than
//            // end of block, and make sure new record points to end of block.
//            new_record->rec_len = empty_space;
//            entry->rec_len = expected_size;
//
//            Log("Before\n");
//            KHeap::Verify();
//            if(mount_.WriteBlock(entries, *fs_blk_no)) {
//                Log("After\n");
//                KHeap::Verify();
//                KHeap::Free(entries);
//                if(fmode == EXT2_S_IFREG) {
//                    vnode->Prealloc();
//                } else if(fmode == EXT2_S_IFDIR) {
//                    vnode->InitializeDir(ino_);
//                    ++inode_.i_links_count;
//                }
//                vnode->Chmod(0x1FF);
//                vnode->WriteBack();
//                return ds::MakeRefCntPtr((VNode*) vnode);
//            }
//
//            KHeap::Free(entries);
//            return ds::NullOpt;
//        }
//        blk_off += entry->rec_len;
//        entry = (Ext2DirEntry*) ((uint8_t*) entry + entry->rec_len);
//    }
//
//    // If the last block doesn't contain enough space for a new directory entry,
//    // add a new one.
//    if(ds::Optional<uint32_t> new_blk= GetOrCreateBlock(last_blk+1, true)) {
//        auto first_entry = (Ext2DirEntry*) entries;
//        first_entry->inode = vnode->GetIno();
//        memcpy(first_entry->name, name.ToChars(), name.Len());
//        first_entry->name_len = name.Len() - 1;
//        first_entry->file_type = FileModeToFileType(fmode);
//        first_entry->rec_len = blk_size;
//        if(mount_.WriteBlock(entries, *new_blk)) {
//            KHeap::Free(entries);
//            inode_.i_size += blk_size;
//            if(fmode == EXT2_S_IFREG) {
//                vnode->Prealloc();
//            } else if(fmode == EXT2_S_IFDIR) {
//                vnode->InitializeDir(ino_);
//                ++inode_.i_links_count;
//            }
//            vnode->Chmod(0x1FF);
//            vnode->WriteBack();
//            WriteBack();
//            return ds::MakeRefCntPtr((VNode*) vnode);
//        }
//    }
//    return ds::NullOpt;
//}
//
//ds::Optional<uint32_t> Ext2VNode::GetOrCreateBlock(uint32_t block_no,
//                                                   bool create)
//{
//    ds::Optional<ds::DynArray<Extent>> extents;
//    if((extents = GetOrCreateExtents(block_no, 1, create))) {
//        return (*extents)[0].start_;
//    }
//    return ds::NullOpt;
//}
//
//ds::Optional<ds::DynArray<Extent>>
//Ext2VNode::GetOrCreateExtents(uint32_t block_no, size_t len, bool create,
//                              bool mdata)
//{
//    ds::DynArray<Extent> extents;
//    int64_t parent_ind = -1;
//    size_t rem_len = len;
//    auto parent = (uint32_t *) KHeap::Allocate(mount_.GetBlockSize());
//
//    while(rem_len > 0) {
//        size_t ptrs_per_block = mount_.GetBlockSize() / sizeof(uint32_t);
//        size_t ptrs_per_dblock = ptrs_per_block * ptrs_per_block;
//        size_t ptrs_per_tblock = ptrs_per_block * ptrs_per_dblock;
//        ds::Optional<uint32_t> ret = ds::NullOpt;
//        if(block_no < 12) {
//            if(inode_.i_block[block_no] == 0 && create) {
//                if(ds::Optional<uint32_t> block_opt = mount_.AllocBlock()) {
//                    inode_.i_block[block_no] = *block_opt;
//                    ++inode_.i_blocks;
//                } else {
//                    KHeap::Free(parent);
//                    return ds::NullOpt;
//                }
//            }
//
//            AddToExtentList(extents, inode_.i_block[block_no]);
//            --rem_len;
//            ++block_no;
//        } else {
//            int8_t block_ind;
//            uint32_t block_child_ind = block_no;
//            if (block_no < 12 + ptrs_per_block) {
//                block_ind = 12;
//                block_child_ind -= 12;
//            } else if (block_no < 12 + ptrs_per_block + ptrs_per_dblock) {
//                block_ind = 13;
//                block_child_ind -= 12 + ptrs_per_block;
//            } else if (block_no < 12 + ptrs_per_block + ptrs_per_dblock +
//                                  ptrs_per_tblock)
//            {
//                block_ind = 14;
//                block_child_ind -= 12 + ptrs_per_block + ptrs_per_dblock;
//            } else {
//                KHeap::Free(parent);
//                return ds::NullOpt;
//            }
//
//            if(create && inode_.i_block[block_ind] == 0) {
//                if(ds::Optional<uint32_t> new_block = mount_.AllocBlock()) {
//                    inode_.i_block[block_ind] = *new_block;
//                } else {
//                    return ds::NullOpt;
//                }
//            }
//
//            if(parent_ind != inode_.i_block[block_ind]) {
//                parent_ind = inode_.i_block[block_ind];
//                if(! mount_.ReadBlock(parent, parent_ind)) {
//                    KHeap::Free(parent);
//                    return ds::NullOpt;
//                }
//
//                if(mdata) {
//                    AddToExtentList(extents, parent_ind);
//                }
//            }
//
//            //Log("PARENT IND IS %d\n", inode_.i_block[block_ind]);
//
//            uint8_t level = block_ind - 12;
//            size_t ptrs_in_child = PtrsPerIndirectBlock(level);
//            size_t read_size = (ptrs_in_child < rem_len ? ptrs_in_child : rem_len);
//            int64_t blks_read = GetOrCreateExtents(parent, block_child_ind,
//                                                   read_size, extents,
//                                                   block_ind - 12, create,
//                                                   mdata);
//            block_no += blks_read;
//            rem_len -= blks_read;
//
//            if(create) {
//                mount_.WriteBlock(parent, inode_.i_block[block_ind]);
//            }
//
//            if(blks_read < 0) {
//                KHeap::Free(parent);
//                return ds::NullOpt;
//            }
//        }
//    }
//
//    if(parent) {
//        KHeap::Free(parent);
//    }
//    return extents;
//
//}
//
//int64_t Ext2VNode::GetOrCreateExtents(uint32_t *parent, uint32_t block_no,
//                                      size_t len, ds::DynArray<Extent> &extents,
//                                      uint8_t level, bool create, bool mdata)
//{
//    //Log("Level %d\n", level);
//    size_t ptrs_in_child = PtrsPerIndirectBlock(level - 1);
//    uint32_t blocks_read = 0;
//
//    if(level == 0) {
//        for(uint32_t i = 0; i < len; ++i, ++blocks_read) {
//            if(create && parent[i] == 0) {
//                if(ds::Optional<uint32_t> free_block = mount_.AllocBlock()) {
//                    parent[i] = *free_block;
//                    ++inode_.i_blocks;
//                } else {
//                    return -1;
//                }
//            }
//
//            if(! parent[i]) {
//                Log("\tFailed to read %d, showing full block:\n", i);
//                for(uint32_t j = 0; j < len; ++j) {
//                    Log("%d,", parent[i]);
//                }
//                return -1;
//            }
//
//            AddToExtentList(extents, parent[i]);
//        }
//    } else {
//        while(blocks_read < len) {
//            uint32_t parent_ind = block_no / ptrs_in_child;
//            if (create && block_no % ptrs_in_child == 0 &&
//                parent[parent_ind] == 0)
//            {
//                if(ds::Optional<uint32_t> new_parent = mount_.AllocBlock()) {
//                    parent[parent_ind] = *new_parent;
//                    //++inode_.i_blocks;
//                } else {
//                    return -1;
//                }
//            }
//
//            if (auto buff = (uint32_t *) KHeap::Allocate(mount_.GetBlockSize()))
//            {
//                // Read child into RAM and alter it.
//                if (mount_.ReadBlock(buff, parent[parent_ind])) {
//                    if(mdata) {
//                        AddToExtentList(extents, parent[parent_ind]);
//                    }
//
//                    uint32_t child_ind = block_no % ptrs_in_child;
//
//                    size_t new_len = (ptrs_in_child < len ? ptrs_in_child : len);
//                    int64_t sub_blocks_read = GetOrCreateExtents(
//                            buff, child_ind, new_len, extents, level - 1,
//                            create, mdata);
//                    if(sub_blocks_read < 0) {
//                        return sub_blocks_read;
//                    }
//
//                    len -= sub_blocks_read;
//                    blocks_read += sub_blocks_read;
//                    block_no += sub_blocks_read;
//
//                    // Write back child block if get or create block call was a
//                    // success.
//                    if (create) {
//                        mount_.WriteBlock(buff, parent[parent_ind]);
//                    }
//                }
//                KHeap::Free(buff);
//            } else {
//                return -1;
//            }
//        }
//    }
//
//    return blocks_read;
//}
//
//void Ext2VNode::AddToExtentList(ds::DynArray<Extent> &extents, uint32_t block)
//{
//    if(extents.Size()) {
//        Extent last_extent = extents.Back();
//        if(block == last_extent.start_ + last_extent.len_) {
//            ++extents[-1].len_;
//            return;
//        }
//    }
//
//    extents.Append({ block, 1 });
//}
//
//void Ext2VNode::Prealloc()
//{
//    Log("PREALLOCING\n");
//    if(GetExt2FileType() == EXT2_S_IFREG) {
//        uint8_t prealloc_blocks = mount_.GetPreallocBlocks();
//        GetOrCreateExtents(0, prealloc_blocks, true);
//    } else if(GetExt2FileType() == EXT2_S_IFDIR) {
//        uint8_t prealloc_dir_blocks = mount_.GetPreallocDirBlocks();
//        GetOrCreateExtents(0, prealloc_dir_blocks, true);
//    }
//}
//
//size_t Ext2VNode::PtrsPerIndirectBlock(uint8_t level)
//{
//    size_t ptrs_per_direct_block = mount_.GetBlockSize() / sizeof(uint32_t);
//    size_t ptrs_per_inddirect_block = ptrs_per_direct_block;
//    for(uint8_t i = 0; i < level; ++i) {
//        ptrs_per_inddirect_block *= ptrs_per_direct_block;
//    }
//    return ptrs_per_inddirect_block;
//}
//
//bool Ext2VNode::InitializeDir(uint32_t parent_ino)
//{
//    bool ret = false;
//    if(ds::Optional<uint32_t> blk_opt = GetOrCreateBlock(0, true)) {
//        size_t blk_size = mount_.GetBlockSize();
//        if(auto entries = (char*) KHeap::Allocate(blk_size)) {
//            auto dir_entry = (Ext2DirEntry*) entries;
//            uint16_t dir_entry_len = sizeof(Ext2DirEntry) + 1;
//            dir_entry_len = ((dir_entry_len + 4 - 1) / 4) * 4;
//
//            *dir_entry = (Ext2DirEntry) {
//                    .inode = ino_,
//                    .rec_len = dir_entry_len,
//                    .name_len = 1,
//                    .file_type = EXT2_FT_DIR
//            };
//            dir_entry->name[0] = '.';
//
//
//            auto parent_entry = (Ext2DirEntry*) ((char*) entries + dir_entry_len);
//            uint16_t parent_entry_len = blk_size - dir_entry_len;
//            *parent_entry = (Ext2DirEntry) {
//                    .inode = parent_ino,
//                    .rec_len = parent_entry_len,
//                    .name_len = 2,
//                    .file_type = EXT2_FT_DIR
//            };
//            parent_entry->name[0] = parent_entry->name[1] = '.';
//            inode_.i_size = blk_size;
//            ++inode_.i_links_count;
//
//            if(mount_.WriteBlock(entries, *blk_opt) && WriteBack()) {
//                ret = true;
//            }
//
//            KHeap::Free(entries);
//        }
//    }
//    return ret;
//}
//
//ds::Optional<ds::RefCntPtr<VNode>> Ext2VNode::Link(const ds::String &name,
//                                                   uint32_t ino)
//{
//    auto src_file = new Ext2VNode(ino, mount_);
//
//    Ext2FileType ftype = (Ext2FileType) src_file->GetExt2FileType();
//    Ext2FileMode fmode = FileTypeToFileMode(ftype);
//    ++src_file->inode_.i_links_count;
//    ds::Optional<ds::RefCntPtr<VNode>> entry;
//    if((entry = MakeDirEntry(name, fmode, src_file))) {
//        src_file->WriteBack();
//        return entry;
//    }
//
//    return ds::NullOpt;
//}
//
//bool Ext2VNode::Unlink(const ds::String &name)
//{
//    // For ext2 in particular, a hard link is just a distinct directory entry
//    // pointing to an inode, so we remove the dir entry. In other FSs, it can
//    // be more complicated, so distinguishing between removal/unlinking in base
//    // class is worthwhile.
//    return Remove(name);
//}
//
//bool Ext2VNode::SymLink(const ds::String &name, const ds::String &target)
//{
//    // Make vnode.
//    Ext2VNode *vnode;
//    if(! (vnode = mount_.AllocVNode(EXT2_S_IFLNK))) {
//        return false;
//    }
//
//    const char *target_buff = target.ToChars();
//    if(target.Len() < 60) {
//        memcpy(&vnode->inode_.i_block, target_buff, target.Len());
//    } else {
//        vnode->Write((void*) target_buff, target.Len());
//    }
//
//    ds::Optional<ds::RefCntPtr<VNode>> entry = MakeDirEntry(name, EXT2_S_IFDIR,
//                                                            vnode);
//    return entry.HasValue();
//}
//
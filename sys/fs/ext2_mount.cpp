#include <sys/fs/ext2_mount.h>
#include <sys/fs/ext2_vnode.h>

Ext2Mount::Ext2Mount(SATAPort *disk, uint64_t partition_base_sector)
        : partition_base_sector_(partition_base_sector)
        , disk_(disk)
        , super_((Ext2SuperBlock*) (disk_->Read(partition_base_sector_+2, 1)))
        , block_size_(1024 << super_->s_log_block_size)
        , sectors_per_block_(block_size_ / 512)
        , bgdt_(disk, partition_base_sector + 2 + (block_size_ >= 2048 ? 1 : 2),
                sectors_per_block_)
        , inode_tab_cache_(&Ext2Mount::WriteBackDiskMem<Ext2INode>)
        , inode_bmap_cache_(&Ext2Mount::WriteBackDiskMem<uint64_t>)
        , block_bmap_cache_(&Ext2Mount::WriteBackDiskMem<uint64_t>)
{}

uint32_t Ext2Mount::GetBlockSize() const
{
    return block_size_;
}

uint32_t Ext2Mount::GetSectorsPerBlock() const
{
    return sectors_per_block_;
}

uint8_t Ext2Mount::GetPreallocBlocks() const
{
    return super_->s_prealloc_blocks;
}

uint8_t Ext2Mount::GetPreallocDirBlocks() const
{
    return super_->s_prealloc_dir_blocks * (super_->s_feature_compat & 1);
}

ds::Optional<Ext2INode> Ext2Mount::ReadINode(uint32_t ino)
{
    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;

    uint32_t tab_block_off = (local_ind * super_->s_inode_size) / block_size_;
    uint32_t block_no = bgdt_[block_group].bg_inode_table + tab_block_off;
    ds::OwningPtr<uint8_t> inode_tab = ReadBlock(block_no);

    if(inode_tab) {
        uint32_t inode_off = (local_ind * super_->s_inode_size) % block_size_;
        const uint8_t *inode_tab_bytes = inode_tab.ToRawPtr();
        auto inode = (Ext2INode*) (inode_tab_bytes + inode_off);

        return *inode;
    }

    return ds::NullOpt;
}

bool Ext2Mount::WriteINode(uint32_t ino, const Ext2INode &inode)
{
    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;

    uint32_t tab_block_off = (local_ind * super_->s_inode_size) / block_size_;
    uint32_t block_no = bgdt_[block_group].bg_inode_table + tab_block_off;
    ds::OwningPtr<uint8_t> inode_tab = ReadBlock(block_no);

    if(inode_tab) {
        uint32_t inode_off = (local_ind * super_->s_inode_size) % block_size_;
        const uint8_t *inode_tab_bytes = inode_tab.ToRawPtr();
        auto inode_ptr = (Ext2INode*) (inode_tab_bytes + inode_off);
        *inode_ptr = inode;

        if(WriteBlock((void*) inode_tab_bytes, block_no)) {
            return true;
        }
    }

    return false;
}

VNode *Ext2Mount::ReadVNode(uint32_t ino)
{
    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;

    ds::Optional<DiskMem<Ext2INode>> inode_tab_opt;
    if((inode_tab_opt = ReadINodeTable(block_group))) {
        size_t inode_offset = local_ind * super_->s_inode_size;
        DiskMem<Ext2INode> inode_tab = *inode_tab_opt;
        const char *inode_tab_bytes = inode_tab.ToBytes();
        auto inode = (Ext2INode*) (inode_tab_bytes + inode_offset);
        return new Ext2VNode(*inode, ino, *this);
    }
    return nullptr;
}


ds::RefCntPtr<VNode> Ext2Mount::ReadRootDir()
{
    return ds::MakeRefCntPtr((VNode*) ReadVNode(2));
}

Ext2VNode *Ext2Mount::AllocVNode(Ext2FileMode type, uint16_t perms,
                                 uint16_t uid, uint16_t gid)
{
    // Write back superblock at some point.
    size_t no_bgroups = super_->s_blocks_count / super_->s_blocks_per_group;
    int64_t free_inode = -1;
    for(uint32_t i = 0 ; i < no_bgroups && free_inode == -1; ++i) {
        if(bgdt_[i].bg_free_inodes_count) {
            if(auto inode_bmap_opt = ReadINodeBitmap(i)) {
                DiskMem<uint64_t> inode_bmap = *inode_bmap_opt;
                for(uint32_t j = 0; j < block_size_ / 8; ++j) {
                    if(inode_bmap[j] != 0xFFFFFFFFFFFFFFFF) {
                        // Ino 1 is not valid.
                        uint64_t free_inos = (i == 0 && j == 0) ?
                                             ~(inode_bmap[j] | 1) : ~(inode_bmap[j]);
                        // Branchless bmap lookup; if all inodes are free,
                        // just ignore (undefined) clzl result and return
                        // first inode in group. Otherwise, return first
                        // index of first unset bit in bitmap.
                        uint64_t unset = __builtin_ffsll(free_inos);
                        uint32_t bg_inode_base = i * super_->s_inodes_per_group;
                        free_inode = bg_inode_base + (j * 64) + unset;
                        inode_bmap[j] |= (1ULL << (unset - 1));
                        --bgdt_[i].bg_free_inodes_count;
                        --super_->s_free_inodes_count;
                        bgdt_.WriteBack();
                        inode_bmap.WriteBack();
                        break;
                    }
                }
            }
        }
    }

    if(free_inode == -1) { return nullptr; }
    Ext2INode new_inode = {};
    new_inode.i_mode = type | (perms > 0x1FF ? 0 : perms);
    new_inode.i_links_count = 1;
    new_inode.i_uid = uid;
    new_inode.i_gid = gid;
    Log("\n\nALLOCATED INO IS %d\n\n", free_inode);
    if(free_inode == 82) {
        Log("BP\n");
    }
    return new Ext2VNode(new_inode, free_inode, *this);
}

ds::Optional<uint32_t> Ext2Mount::AllocBlock()
{
    // Write back superblock at some point.
    size_t no_bgroups = super_->s_blocks_count / super_->s_blocks_per_group;
    int64_t free_block = -1;
    for(uint32_t i = 0 ; i < no_bgroups && free_block == -1; ++i) {
        if(bgdt_[i].bg_free_blocks_count) {
            if(auto block_bmap_opt = ReadBlockBitmap(i)) {
                DiskMem<uint64_t> block_bmap = *block_bmap_opt;
                for(uint32_t j = 0; j < block_size_ / 8; ++j) {
                    if(block_bmap[j] != 0xFFFFFFFFFFFFFFFF) {
                        uint64_t free_blocks = i == 0 && j == 0 ?
                                               ~(block_bmap[j] | 1) : ~(block_bmap[j]);
                        uint64_t unset = __builtin_ffsll(free_blocks);
                        uint32_t bg_inode_base = i * super_->s_blocks_per_group;
                        free_block = bg_inode_base + (j * 64) + unset;
                        //Log("\t unset %d, before 0x%x, ", unset, block_bmap[j]);
                        block_bmap[j] |= (1ULL << (unset - 1));
                        //Log("after 0x%x\n", block_bmap[j]);
                        block_bmap.WriteBack();
                        --bgdt_[i].bg_free_blocks_count;
                        break;
                    }
                }
            }
        }
    }

    if(free_block == -1) { return ds::NullOpt; }
    //Log("Free block %d\n", free_block);
    return free_block + 1;
}


bool Ext2Mount::DeleteINode(uint32_t ino)
{
    uint32_t block_group = (ino - 1) / super_->s_inodes_per_group;
    uint32_t local_ind = (ino - 1) % super_->s_inodes_per_group;

    if(auto inode_bmap_opt = ReadINodeBitmap(block_group)) {
        DiskMem<uint64_t> inode_bmap = *inode_bmap_opt;
        inode_bmap[local_ind / 64] &= ~(1ULL << (local_ind % 64));
        return true;
    }

    return false;
}

bool Ext2Mount::FreeBlock(uint32_t block_no)
{
    uint32_t block_group = (block_no - 1) / super_->s_blocks_per_group;
    uint32_t local_ind = (block_no - 1) % super_->s_blocks_per_group;
    if(auto block_bmap_opt = ReadBlockBitmap(block_group)) {
        DiskMem<uint64_t> block_bmap = *block_bmap_opt;
        block_bmap[local_ind / 64] &= ~(1ULL << (local_ind % 64));
        return true;
    }
    return false;
}

bool Ext2Mount::ReadBlock(void *buff, uint32_t block, size_t num_blocks) const
{
    if(block == 0) {
        return false;
    }

    uint32_t sector_no = (block - 1) * sectors_per_block_;
    uint32_t num_sectors = (num_blocks * sectors_per_block_);
    return disk_->Read(partition_base_sector_ + 2 + sector_no, num_sectors,
                       buff);
}


bool Ext2Mount::WriteBlock(void *buff, uint32_t block, size_t num_blocks)
{
    if(block == 0) {
        return false;
    }

    uint32_t sector_no = (block - 1) * sectors_per_block_;
    uint32_t num_sectors = (num_blocks * sectors_per_block_);
    return disk_->Write(partition_base_sector_ + 2 + sector_no, num_sectors,
                        buff);
}

ds::OwningPtr<uint8_t>
Ext2Mount::ReadBlock(uint32_t block, size_t num_blocks) const
{
    auto buff = (uint8_t*) KHeap::Allocate(block_size_ * num_blocks);
    uint32_t sector_no = (block - 1) * sectors_per_block_;
    uint32_t num_sectors = (num_blocks * sectors_per_block_);
    if(disk_->Read(partition_base_sector_ + 2 + sector_no, num_sectors, buff)) {
        return ds::OwningPtr<uint8_t>(buff);
    }
    return nullptr;
}

bool Ext2Mount::ReadBlocks(const ds::DynArray<Extent> &extents, void *buff)
{
    ds::DynArray<DiskRange> ranges;
    uint64_t first_block = partition_base_sector_ + 2;
    for(int i = 0; i < extents.Size(); ++i) {
        uint32_t sector_no = (extents[i].start_ - 1) * sectors_per_block_;
        uint64_t start_lba =  first_block + sector_no;
        ranges.Append({ start_lba, extents[i].len_ * sectors_per_block_ });
    }

    return disk_->Read(ranges, buff);
}

bool Ext2Mount::WriteBlocks(const ds::DynArray<Extent> &extents, void *buff)
{
    ds::DynArray<DiskRange> ranges;
    uint64_t first = partition_base_sector_ + 2;
    for(int i = 0; i < extents.Size(); ++i) {
        Log("Extent %d-%d\n", extents[i].start_,
            extents[i].start_ + extents[i].len_);
        uint64_t sector_no = (extents[i].start_ - 1) * sectors_per_block_;
        uint64_t start_lba =  first + sector_no;
        ranges.Append({ start_lba, extents[i].len_ * sectors_per_block_ });
    }

    return disk_->Write(ranges, buff);
}

bool Ext2Mount::AppendToArr(ds::DynArray<uint32_t> &block_arr,
                            uint32_t block_no)
{
    block_arr.Append(block_no);
    return true;
}

ds::Optional<DiskMem<Ext2INode>>
Ext2Mount::ReadINodeTable(uint32_t block_group_no)
{
    ds::Optional<DiskMem<Ext2INode>> tab;
    if((tab = inode_tab_cache_.Lookup(block_group_no))) {
        return tab;
    }

    Ext2BlockGroupDescriptor bgdt_entry = bgdt_[block_group_no];
    uint32_t table_block_no = bgdt_entry.bg_inode_table;
    uint64_t fs_block_no = sectors_per_block_ * (table_block_no - 1);
    uint64_t start_lba = partition_base_sector_ + 2 + fs_block_no;
    uint64_t free_inodes = bgdt_entry.bg_free_inodes_count;
    uint64_t used_inodes = (super_->s_inodes_per_group - free_inodes);
    uint64_t tab_size = (used_inodes * super_->s_inode_size);
    uint64_t tab_blks = (tab_size / block_size_) + (tab_size % block_size_ > 0);
    uint64_t tab_sectors = tab_blks * sectors_per_block_;

    DiskMem<Ext2INode> table(disk_, start_lba, tab_sectors);
    if(table) {
        inode_tab_cache_.Insert(block_group_no, (table));
        return table;
    }

    return ds::NullOpt;
}

ds::Optional<DiskMem<uint64_t>>
Ext2Mount::ReadINodeBitmap(uint32_t block_group_no)
{
    ds::Optional<DiskMem<uint64_t>> cache_bmap;
    if((cache_bmap = inode_bmap_cache_.Lookup(block_group_no))) {
        return cache_bmap;
    }

    uint32_t bmap_block_no = bgdt_[block_group_no].bg_inode_bitmap;
    uint64_t bmap_sector = partition_base_sector_ + 2 + (bmap_block_no - 1) *
                                                        sectors_per_block_;
    DiskMem<uint64_t> bmap(disk_, bmap_sector, sectors_per_block_);
    Log("BMAP SECTOR: %d\n", bmap_sector);
    if(bmap) {
        inode_bmap_cache_.Insert(block_group_no, bmap);
        return bmap;
    }

    return ds::NullOpt;
}

ds::Optional<DiskMem<uint64_t>>
Ext2Mount::ReadBlockBitmap(uint32_t block_group_no)
{
    ds::Optional<DiskMem<uint64_t>> cache_bmap;
    if((cache_bmap = block_bmap_cache_.Lookup(block_group_no))) {
        return cache_bmap;
    }

    uint32_t bmap_block_no = bgdt_[block_group_no].bg_block_bitmap;
    uint64_t bmap_sector = partition_base_sector_ + 2 + (bmap_block_no - 1) *
                                                        sectors_per_block_;
    DiskMem<uint64_t> bmap(disk_, bmap_sector, sectors_per_block_);
    if(bmap) {
        block_bmap_cache_.Insert(block_group_no, bmap);
        return bmap;
    }

    return ds::NullOpt;
}

void Ext2Mount::Pin(const ds::RefCntPtr<VNode> &vnode)
{
    uint32_t ino = vnode->GetIno();
    if(pinned_vnodes_.Contains(ino)) {
        ++num_pins_[ino];
    } else {
        num_pins_.Insert(ino, 1);
        pinned_vnodes_.Insert(ino, vnode);
    }
}

void Ext2Mount::Unpin(uint32_t ino)
{
    if(num_pins_.Contains(ino) && --num_pins_[ino] == 0) {
        num_pins_.Delete(ino);
        pinned_vnodes_.Delete(ino);
    }
}

ds::Optional<ds::RefCntPtr<VNode>> Ext2Mount::GetVNode(uint32_t ino)
{
    if(ds::Optional<ds::RefCntPtr<VNode>> vnode_opt = pinned_vnodes_.Lookup(ino)) {
        ++num_pins_[ino];
        return *vnode_opt;
    }

    if(VNode *vnode = ReadVNode(ino)) {
        return ds::MakeRefCntPtr(vnode);
    }

    return ds::NullOpt;
}

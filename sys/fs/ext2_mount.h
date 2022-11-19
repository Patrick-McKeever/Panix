#ifndef EXT2_MOUNT_H
#define EXT2_MOUNT_H

#include <ds/hash_map.h>
#include <ds/ref_cnt_ptr.h>
#include <sys/sata_port.h>
#include <sys/fs/vnode.h>
#include <sys/disk_mem.h>
#include <sys/fs/vmount.h>

struct Ext2SuperBlock {
    uint32_t	s_inodes_count;
    uint32_t	s_blocks_count;
    uint32_t	s_r_blocks_count;
    uint32_t	s_free_blocks_count;
    uint32_t	s_free_inodes_count;
    uint32_t	s_first_data_block;
    uint32_t	s_log_block_size;
    uint32_t	s_log_frag_size;
    uint32_t	s_blocks_per_group;
    uint32_t	s_frags_per_group;
    uint32_t	s_inodes_per_group;
    uint32_t	s_mtime;
    uint32_t	s_wtime;
    uint16_t	s_mnt_count;
    uint16_t	s_max_mnt_count;
    uint16_t	s_magic;
    uint16_t	s_state;
    uint16_t	s_errors;
    uint16_t	s_minor_rev_level;
    uint32_t	s_lastcheck;
    uint32_t	s_checkinterval;
    uint32_t	s_creator_os;
    uint32_t	s_rev_level;
    uint16_t	s_def_resuid;
    uint16_t	s_def_resgid;
    uint32_t	s_first_ino;
    uint16_t	s_inode_size;
    uint16_t	s_block_group_nr;
    uint32_t	s_feature_compat;
    uint32_t	s_feature_incompat;
    uint32_t	s_feature_ro_compat;
    uint8_t 	s_uuid[16];
    uint8_t 	s_volume_name[16];
    uint32_t	s_last_mounted;
    uint32_t	s_algo_bitmap;
    uint8_t		s_prealloc_blocks;
    uint8_t		s_prealloc_dir_blocks;
    uint16_t	alignment;
    uint8_t		s_journal_uuid[16];
    uint32_t	s_journal_inum;
    uint32_t	s_journal_dev;
    uint32_t	s_last_orphan;
    uint32_t	s_hash_seed[4];
    uint8_t		s_def_hash_version;
    uint8_t 	padding[3];
    uint32_t	s_default_mount_options;
    uint32_t	s_first_meta_bg;
} __attribute__((packed));

struct Ext2BlockGroupDescriptor {
    uint32_t	bg_block_bitmap;
    uint32_t	bg_inode_bitmap;
    uint32_t	bg_inode_table;
    uint16_t	bg_free_blocks_count;
    uint16_t	bg_free_inodes_count;
    uint16_t	bg_used_dirs_count;
    uint16_t	bg_pad;
    uint8_t		bg_reserved[12];
} __attribute__((packed));

struct Ext2INode {
    uint16_t	i_mode;
    uint16_t	i_uid;
    uint32_t	i_size;
    uint32_t	i_atime;
    uint32_t	i_ctime;
    uint32_t	i_mtime;
    uint32_t	i_dtime;
    uint16_t	i_gid;
    uint16_t	i_links_count;
    uint32_t	i_blocks;
    uint32_t	i_flags;
    uint32_t	i_osd1;
    uint32_t	i_block[15];
    uint32_t	i_generation;
    uint32_t	i_file_acl;
    uint32_t	i_dir_acl;
    uint32_t	i_faddr;
    uint8_t     i_osd2[12];
} __attribute__((packed));

struct Ext2DirEntry {
    uint32_t    inode;
    uint16_t	rec_len;
    uint8_t	    name_len;
    uint8_t 	file_type;
    char        name[];
} __attribute__((packed));

enum Ext2FileType {
    EXT2_FT_UNKNOWN	    =   0,
    EXT2_FT_REG_FILE	=   1,
    EXT2_FT_DIR	        =   2,
    EXT2_FT_CHRDEV	    =   3,
    EXT2_FT_BLKDEV	    =   4,
    EXT2_FT_FIFO	    =   5,
    EXT2_FT_SOCK	    =   6,
    EXT2_FT_SYMLINK	    =   7
};

enum Ext2FileMode {
    EXT2_S_IFSOCK	=   0xC000,
    EXT2_S_IFLNK	=   0xA000,
    EXT2_S_IFREG	=   0x8000,
    EXT2_S_IFBLK	=   0x6000,
    EXT2_S_IFDIR	=   0x4000,
    EXT2_S_IFCHR	=   0x2000,
    EXT2_S_IFIFO	=   0x1000,
    EXT2_S_NONE     =   0x0000
};

struct Extent {
    uint32_t start_;
    uint32_t len_;
};

class Ext2Mount
{
public:
    Ext2Mount(SATAPort *disk, uint64_t partition_base_sector);

    uint8_t GetPreallocBlocks() const;
    uint8_t GetPreallocDirBlocks() const;
    ds::Optional<Ext2INode> ReadINode(uint32_t ino);
    bool WriteINode(uint32_t ino, const Ext2INode &inode);
    VNode *ReadVNode(uint32_t ino);

    class Ext2VNode *AllocVNode(Ext2FileMode type=EXT2_S_IFREG,
                                uint16_t perms=0x1FF, uint16_t uid=0,
                                uint16_t gid=0);

    ds::Optional<uint32_t> AllocBlock();

    bool DeleteINode(uint32_t ino);

    bool FreeBlock(uint32_t block_no);

    virtual ds::RefCntPtr<VNode> ReadRootDir();

    bool ReadBlock(void *buff, uint32_t block, size_t num_blocks=1) const;
    bool WriteBlock(void *buff, uint32_t block, size_t num_blocks = 1);
    bool ReadBlocks(const ds::DynArray<Extent> &extents, void *buff);
    bool WriteBlocks(const ds::DynArray<Extent> &extents, void *buff);

    ds::OwningPtr<uint8_t> ReadBlock(uint32_t block, size_t num_blocks=1) const;

    ds::Optional<ds::DynArray<uint32_t>> GetBlockList(const Ext2INode &inode);

    uint32_t GetBlockSize() const;
    uint32_t GetSectorsPerBlock() const;

    void Pin(const ds::RefCntPtr<VNode> &vnode);
    void Unpin(uint32_t ino);
    ds::Optional<ds::RefCntPtr<VNode>> GetVNode(uint32_t ino);


private:
    uint64_t partition_base_sector_;
    SATAPort *disk_;
    Ext2SuperBlock *super_;
    uint32_t block_size_;
    uint32_t sectors_per_block_;
    DiskMem<Ext2BlockGroupDescriptor> bgdt_;
    ds::LRUCache<uint32_t, DiskMem<Ext2INode>> inode_tab_cache_;
    ds::LRUCache<uint32_t, DiskMem<uint64_t>> inode_bmap_cache_;
    ds::LRUCache<uint32_t, DiskMem<uint64_t>> block_bmap_cache_;
    ds::HashMap<uint32_t, ds::RefCntPtr<VNode>> pinned_vnodes_;
    ds::HashMap<uint32_t, size_t> num_pins_;

    enum traversal_t { POSTORDER, PREORDER, LEAVES_ONLY };

    ds::Optional<DiskMem<Ext2INode>> ReadINodeTable(uint32_t block_group_no);

    ds::Optional<DiskMem<uint64_t>> ReadINodeBitmap(uint32_t block_group_no);

    ds::Optional<DiskMem<uint64_t>> ReadBlockBitmap(uint32_t block_group_no);

    bool AppendToArr(ds::DynArray<uint32_t> &block_arr, uint32_t block_no);


    template<typename T>
    static void WriteBackDiskMem(uint32_t no, DiskMem<T> disk_mem)
    {
        Log("Writing back disk mem\n");
        disk_mem.WriteBack();
    }

};


#endif //EXT2_MOUNT_H

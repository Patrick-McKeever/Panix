//#ifndef EXT2_H
//#define EXT2_H
//
//#include <ds/string.h>
//#include <ds/cache.h>
//#include <ds/optional.h>
//#include <ds/ref_cnt_ptr.h>
//#include <sys/kheap.h>
//#include <sys/sata_port.h>
//#include <sys/disk_mem.h>
//#include <utility>
//
//struct Ext2SuperBlock {
//    uint32_t	s_inodes_count;
//    uint32_t	s_blocks_count;
//    uint32_t	s_r_blocks_count;
//    uint32_t	s_free_blocks_count;
//    uint32_t	s_free_inodes_count;
//    uint32_t	s_first_data_block;
//    uint32_t	s_log_block_size;
//    uint32_t	s_log_frag_size;
//    uint32_t	s_blocks_per_group;
//    uint32_t	s_frags_per_group;
//    uint32_t	s_inodes_per_group;
//    uint32_t	s_mtime;
//    uint32_t	s_wtime;
//    uint16_t	s_mnt_count;
//    uint16_t	s_max_mnt_count;
//    uint16_t	s_magic;
//    uint16_t	s_state;
//    uint16_t	s_errors;
//    uint16_t	s_minor_rev_level;
//    uint32_t	s_lastcheck;
//    uint32_t	s_checkinterval;
//    uint32_t	s_creator_os;
//    uint32_t	s_rev_level;
//    uint16_t	s_def_resuid;
//    uint16_t	s_def_resgid;
//    uint32_t	s_first_ino;
//    uint16_t	s_inode_size;
//    uint16_t	s_block_group_nr;
//    uint32_t	s_feature_compat;
//    uint32_t	s_feature_incompat;
//    uint32_t	s_feature_ro_compat;
//    uint8_t 	s_uuid[16];
//    uint8_t 	s_volume_name[16];
//    uint32_t	s_last_mounted;
//    uint32_t	s_algo_bitmap;
//    uint8_t		s_prealloc_blocks;
//    uint8_t		s_prealloc_dir_blocks;
//    uint16_t	alignment;
//    uint8_t		s_journal_uuid[16];
//    uint32_t	s_journal_inum;
//    uint32_t	s_journal_dev;
//    uint32_t	s_last_orphan;
//    uint32_t	s_hash_seed[4];
//    uint8_t		s_def_hash_version;
//    uint8_t 	padding[3];
//    uint32_t	s_default_mount_options;
//    uint32_t	s_first_meta_bg;
//} __attribute__((packed));
//
//struct Ext2BlockGroupDescriptor {
//    uint32_t	bg_block_bitmap;
//    uint32_t	bg_inode_bitmap;
//    uint32_t	bg_inode_table;
//    uint16_t	bg_free_blocks_count;
//    uint16_t	bg_free_inodes_count;
//    uint16_t	bg_used_dirs_count;
//    uint16_t	bg_pad;
//    uint8_t		bg_reserved[12];
//} __attribute__((packed));
//
//struct Ext2INode {
//    uint16_t	i_mode;
//    uint16_t	i_uid;
//    uint32_t	i_size;
//    uint32_t	i_atime;
//    uint32_t	i_ctime;
//    uint32_t	i_mtime;
//    uint32_t	i_dtime;
//    uint16_t	i_gid;
//    uint16_t	i_links_count;
//    uint32_t	i_blocks;
//    uint32_t	i_flags;
//    uint32_t	i_osd1;
//    uint32_t	i_block[15];
//    uint32_t	i_generation;
//    uint32_t	i_file_acl;
//    uint32_t	i_dir_acl;
//    uint32_t	i_faddr;
//    uint8_t     i_osd2[12];
//} __attribute__((packed));
//
//struct Ext2DirEntry {
//    uint32_t    inode;
//    uint16_t	rec_len;
//	uint8_t	    name_len;
//	uint8_t 	file_type;
//	char        name[];
//} __attribute__((packed));
//
//enum class FileType {
//    DIRECTORY,
//    REGULAR,
//    SYMLINK,
//    BLOCK_DEV,
//    CHAR_DEV,
//    SOCKET,
//    NAMED_PIPE,
//    OTHER
//};
//
//enum Ext2FileType {
//    EXT2_FT_UNKNOWN	    =   0,
//    EXT2_FT_REG_FILE	=   1,
//    EXT2_FT_DIR	        =   2,
//    EXT2_FT_CHRDEV	    =   3,
//    EXT2_FT_BLKDEV	    =   4,
//    EXT2_FT_FIFO	    =   5,
//    EXT2_FT_SOCK	    =   6,
//    EXT2_FT_SYMLINK	    =   7
//};
//
//enum Ext2FileMode {
//    EXT2_S_IFSOCK	=   0xC000,
//    EXT2_S_IFLNK	=   0xA000,
//    EXT2_S_IFREG	=   0x8000,
//    EXT2_S_IFBLK	=   0x6000,
//    EXT2_S_IFDIR	=   0x4000,
//    EXT2_S_IFCHR	=   0x2000,
//    EXT2_S_IFIFO	=   0x1000,
//    EXT2_S_NONE     =   0x0000
//};
//
//
//class VNode : public KernelAllocated<VNode>
//{
//public:
//    virtual uint32_t GetIno() const = 0;
//    virtual FileType GetFileType() const = 0;
//    virtual uint64_t GetLength() const = 0;
//    virtual ds::Optional<ds::RefCntPtr<VNode>> Lookup(const ds::String &name) = 0;
//    virtual ds::Optional<ds::RefCntPtr<VNode>> Touch(const ds::String &name) = 0;
//    virtual ds::Optional<ds::RefCntPtr<VNode>> MkDir(const ds::String &name) = 0;
//    virtual ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
//        ReadDir() = 0;
//    virtual bool Remove(const ds::String &child_name) = 0;
//    virtual bool Chmod(uint16_t perms) = 0;
//    virtual bool Seek(size_t offset) = 0;
//    virtual bool Read(void *buffer, size_t len) = 0;
//    virtual bool Write(void *buffer, size_t len) = 0;
//
//    // virtual bool RmDir() = 0;
//    //virtual bool Rename(const ds::String &new_name) = 0;
//    virtual ds::Optional<ds::RefCntPtr<VNode>> Link(const ds::String &name,
//                                                    uint32_t ino) = 0;
//    virtual bool Unlink(const ds::String &name) = 0;
//    virtual bool SymLink(const ds::String &name, const ds::String &target) = 0;
//    //virtual void Open() = 0;
//    //virtual void Mount() = 0;
//    //virtual void Unmount() = 0;
//    //virtual void Access() = 0;
//    //virtual void GetAttr() = 0;
//    //virtual void SetAttr() = 0;
//    //virtual void ReadLink() = 0;
//    //virtual void MMap() = 0;
//    //virtual void Close() = 0;
//    //virtual void AdvLock() = 0;
//    //virtual void Ioctl() = 0;
//    //virtual void Select() = 0;
//    //virtual void Lock() = 0;
//    //virtual void Unlock() = 0;
//    //virtual void Inactive() = 0;
//    //virtual void Reclaim() = 0;
//    //virtual void Abortop() = 0;
//};
//
//struct Extent {
//    uint32_t start_;
//    uint32_t len_;
//};
//
//class Ext2VNode : public VNode
//{
//public:
//    Ext2VNode(uint32_t ino, class Ext2Mount &mount);
//    Ext2VNode(Ext2INode inode, uint32_t ino, class Ext2Mount &mount);
//    ~Ext2VNode();
//    bool WriteBack() const;
//
//    virtual uint32_t GetIno() const override;
//    virtual FileType GetFileType() const override;
//    virtual uint64_t GetLength() const override;
//    uint16_t GetPerms() const;
//
//    virtual bool Seek(size_t offset) override;
//
//    virtual bool Read(void *buffer, size_t len) override;
//
//    virtual bool Write(void *buffer, size_t len) override;
//
//    virtual ds::Optional<ds::HashMap<ds::String, ds::RefCntPtr<VNode>>>
//        ReadDir() override;
//
//
//    virtual ds::Optional<ds::RefCntPtr<VNode>> Link(const ds::String &name,
//                                                    uint32_t ino) override;
//
//    virtual bool Unlink(const ds::String &name) override;
//
//    virtual bool SymLink(const ds::String &name, const ds::String &target)
//    override;
//
//    virtual ds::Optional<ds::RefCntPtr<VNode>>
//    MkDir(const ds::String &name) override;
//
//    virtual ds::Optional<ds::RefCntPtr<VNode>>
//    Touch(const ds::String &name) override;
//
//    // virtual bool RmDir() override;
//
//    virtual ds::Optional<ds::RefCntPtr<VNode>>
//    Lookup(const ds::String &name) override;
//
//    virtual bool Remove(const ds::String &child_name) override;
//
//    virtual bool Chmod(uint16_t perms) override;
//
//private:
//    Ext2INode inode_;
//    uint32_t ino_;
//    size_t offset_;
//    class Ext2Mount &mount_;
//
//    struct RawDirEntry {
//        uint32_t block_no;
//        size_t offset;
//        void *mem;
//    };
//
//    ds::Optional<RawDirEntry> GetDirEntry(const ds::String &name);
//
//    void Prealloc();
//    ds::Optional<ds::RefCntPtr<VNode>>
//    MakeDirEntry(const ds::String &name, Ext2FileMode fmode,
//                 Ext2VNode *vnode=nullptr);
//
//    /**
//     *
//     * @param block_no The index of the block within this file. I.e. If we want
//     *                 block 12 of the file, pass 12 as block_no.
//     * @param create Should we create this block if it does not already exist?
//     * @return The index of this block within the filesystem.
//     */
//    ds::Optional<uint32_t> GetOrCreateBlock(uint32_t block_no, bool create);
//
//    ds::Optional<ds::DynArray<Extent>>
//    GetOrCreateExtents(uint32_t start_block, size_t len, bool create,
//                       bool mdata=false);
//    int64_t GetOrCreateExtents(uint32_t *parent, uint32_t block_no, size_t len,
//                               ds::DynArray<Extent> &extents, uint8_t level,
//                               bool create, bool mdata=false);
//
//    size_t PtrsPerIndirectBlock(uint8_t level);
//
//    void AddToExtentList(ds::DynArray<Extent> &extents, uint32_t block);
//    bool ReadWrite(void *buffer, size_t len, bool write);
//    uint16_t GetExt2FileType() const;
//    bool InitializeDir(uint32_t parent_ino);
//};
//
//class Ext2Mount
//{
//public:
//    Ext2Mount(SATAPort *disk, uint64_t partition_base_sector);
//
//
//
//    uint8_t GetPreallocBlocks() const;
//    uint8_t GetPreallocDirBlocks() const;
//    ds::Optional<Ext2INode> ReadINode(uint32_t ino);
//    bool WriteINode(uint32_t ino, const Ext2INode &inode);
//    VNode *ReadVNode(uint32_t ino);
//
//    Ext2VNode *AllocVNode(Ext2FileMode type=EXT2_S_IFREG,
//                          uint16_t perms=0x1FF, uint16_t uid=0,
//                          uint16_t gid=0);
//
//    ds::Optional<uint32_t> AllocBlock();
//
//    bool DeleteINode(uint32_t ino);
//
//    bool FreeBlock(uint32_t block_no);
//
//    VNode *ReadRootDir();
//    VNode *Find(const ds::String &name);
//
//    bool ReadBlock(void *buff, uint32_t block, size_t num_blocks=1) const;
//    bool WriteBlock(void *buff, uint32_t block, size_t num_blocks = 1);
//    bool ReadBlocks(const ds::DynArray<Extent> &extents, void *buff);
//    bool WriteBlocks(const ds::DynArray<Extent> &extents, void *buff);
//
//    ds::OwningPtr<uint8_t> ReadBlock(uint32_t block, size_t num_blocks=1) const;
//
//    ds::Optional<ds::DynArray<uint32_t>> GetBlockList(const Ext2INode &inode);
//
//
//    template<typename lambda_t, typename ... arg_ts>
//    bool BlockWalk(const Ext2INode &inode,
//                   lambda_t block_lambda, arg_ts && ... args)
//    {
//        return WalkINodeBlocks(inode, LEAVES_ONLY, block_lambda, (args)...);
//    }
//
//    template<typename lambda_t, typename ... arg_ts>
//    bool PreOrderBlockWalk(const ds::OwningPtr<Ext2INode> &inode,
//                           lambda_t block_lambda, arg_ts && ... args)
//    {
//        return WalkINodeBlocks(inode, PREORDER, block_lambda, (args)...);
//    }
//
//    template<typename lambda_t, typename ... arg_ts>
//    bool PostOrderBlockWalk(const Ext2INode &inode,
//                            lambda_t block_lambda, arg_ts && ... args)
//    {
//        return WalkINodeBlocks(inode, POSTORDER, block_lambda, (args)...);
//    }
//
//    uint32_t GetBlockSize() const;
//    uint32_t GetSectorsPerBlock() const;
//
//
//private:
//    uint64_t partition_base_sector_;
//    SATAPort *disk_;
//    Ext2SuperBlock *super_;
//    uint32_t block_size_;
//    uint32_t sectors_per_block_;
//    DiskMem<Ext2BlockGroupDescriptor> bgdt_;
//    ds::LRUCache<uint32_t, DiskMem<Ext2INode>> inode_tab_cache_;
//    ds::LRUCache<uint32_t, DiskMem<uint64_t>> inode_bmap_cache_;
//    ds::LRUCache<uint32_t, DiskMem<uint64_t>> block_bmap_cache_;
//
//    enum traversal_t { POSTORDER, PREORDER, LEAVES_ONLY };
//
//    ds::Optional<DiskMem<Ext2INode>> ReadINodeTable(uint32_t block_group_no);
//
//    ds::Optional<DiskMem<uint64_t>> ReadINodeBitmap(uint32_t block_group_no);
//
//    ds::Optional<DiskMem<uint64_t>> ReadBlockBitmap(uint32_t block_group_no);
//
//    bool AppendToArr(ds::DynArray<uint32_t> &block_arr, uint32_t block_no);
//
//    template<typename lambda_t, typename ... arg_ts>
//    bool WalkINodeBlocks(const Ext2INode &inode,
//                         traversal_t trav_type,
//                         lambda_t block_lambda, arg_ts && ... args)
//    {
//        size_t total_blocks = inode.i_blocks / sectors_per_block_;
//        ds::DynArray<uint32_t> blocks;
//        uint32_t curr_block = 0;
//        while(curr_block < 12) {
//            bool lambda_success = (this->*block_lambda)(
//                    std::forward<arg_ts>(args)..., inode.i_block[curr_block]);
//            if(! lambda_success) {
//                return false;
//            }
//        }
//
//        // Read indirect blocks. Allocate space for a block once.
//        auto ind_block = ds::OwningPtr<uint32_t>(KHeap::Allocate(block_size_));
//        for(size_t i = 13; i < 15 && curr_block < total_blocks; ++i) {
//            if(! ReadBlock(ind_block, inode.i_block[i])) {
//                return false;
//            }
//
//            if(trav_type == PREORDER) {
//                bool lambda_success = (this->*block_lambda)(
//                        std::forward<arg_ts>(args)..., inode.i_block[i]);
//                if(! lambda_success) {
//                    return false;
//                }
//            }
//
//            // 12th block is indirect, 13th doubly-indirect, 14th
//            // trebly-indirect.
//            uint8_t lev = i - 12;
//            bool ind_read_code = WalkIndirectBlocks(ind_block, curr_block,
//                                                    total_blocks,lev, trav_type,
//                                                    block_lambda, (args)...);
//            if(trav_type == POSTORDER) {
//                bool lambda_success = (this->*block_lambda)(
//                        std::forward<arg_ts>(args)..., inode.i_block[i]);
//                if(! lambda_success) {
//                    return false;
//                }
//            }
//
//            if(! ind_read_code) {
//                return false;
//            }
//        }
//
//        return true;
//    }
//
//    template<typename lambda_t, typename ... arg_ts>
//    bool WalkIndirectBlocks(const ds::OwningPtr<uint32_t> &ind_block,
//                            uint32_t &current_block, uint32_t total_blocks,
//                            uint8_t level, traversal_t trav_type,
//                            lambda_t block_lambda, arg_ts && ... args)
//    {
//        // Allocate once and just reuse it. Much better for performance.
//        auto child_block = ds::OwningPtr<uint32_t>(KHeap::Allocate(block_size_));
//        for(size_t i = 0; current_block < total_blocks; ++current_block, ++i) {
//            if(level == 1) {
//                bool lambda_success = (this->*block_lambda)(
//                        std::forward<arg_ts>(args)..., child_block[i]);
//                if(! lambda_success) {
//                    return false;
//                }
//            } else if(ReadBlock(child_block.ToRawPtr(), ind_block[i])) {
//                if(trav_type == PREORDER) {
//                    bool lambda_success = (this->*block_lambda)(
//                            std::forward<arg_ts>(args)..., child_block[i]);
//                    if(! lambda_success) {
//                        return false;
//                    }
//                }
//
//                bool child_succ = WalkIndirectBlocks(child_block, current_block,
//                                                     total_blocks, level - 1,
//                                                     trav_type, block_lambda,
//                                                     (args)...);
//                if(! child_succ) {
//                    return false;
//                }
//
//                if(trav_type == POSTORDER) {
//                    bool lambda_success = (this->*block_lambda)(
//                            std::forward<arg_ts>(args)..., child_block[i]);
//                    if(! lambda_success) {
//                        return false;
//                    }
//                }
//            } else {
//                return false;
//            }
//        }
//        return true;
//    }
//
//    template<typename T>
//    static void WriteBackDiskMem(uint32_t no, DiskMem<T> disk_mem)
//    {
//        Log("Writing back disk mem\n");
//        disk_mem.WriteBack();
//    }
//
//};
//
//#endif
//
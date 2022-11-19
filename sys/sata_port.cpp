#include "sata_port.h"
#include <libc/string.h>
#include <sys/buddy_allocator.h>
#include <sys/log.h>
#include <sys/disk_mem.h>

ds::DynArray<SATAPort*> EnumerateDevices(volatile void *hba_mmio_base)
{
    // NOTE: At some point, I'll make a base class for all types of supported
    // disks with a common interface. Then, we'll return a dynarray of the
    // base class and use polymorphism. However, for the time being, I have only
    // implemented a SATAPort class.
    static constexpr uint8_t HBA_IPM_ACTIVE = 1;
    static constexpr uint8_t HBA_DET_PRESENT = 3;
    static constexpr uint8_t HBA_ACTIVE_PRESENT =
        (HBA_IPM_ACTIVE << 4) | HBA_DET_PRESENT;

    auto *hba_mmio = (volatile HBAMem*) hba_mmio_base;

    // "port_implemented_" field gives bitmap of implemented ports.
    ds::DynArray<SATAPort*> devices;
    for(uint8_t i = 0; i < 32; ++i) {
        volatile HBAPort *port = &hba_mmio->ports_[i];
        bool implemented = hba_mmio->port_implemented_ & (1 << i);
        bool active = (uint8_t) port->sata_status_ == HBA_ACTIVE_PRESENT;
        if(implemented && active) {
            switch(hba_mmio->ports_[i].signature_) {
                case SATA_SIG_ATA: {
                    devices.Append(new SATAPort(hba_mmio, port));
                }
                // There are other cases, but I haven't written drivers for
                // them.
            }
        }
    }

    return devices;
}

SATAPort::SATAPort(volatile HBAMem* mem, volatile HBAPort* port)
    : mem_(mem)
    , port_(port)
    , cmd_header_(nullptr)
    , received_fis_(nullptr)
    , dev_info_((uint16_t*) BuddyAllocator::Allocate(0x1000))
    , num_slots_(NumberSlots())
    , slots_bitmap_(0)
    , ncq_tag_bitmap_(0)
    , disk_cache_(&SATAPort::HandleEviction)
    , max_lba_(-1)
{
    Log("Configuring\n");
    Configure();
    Log("ID Device\n");
    IdentifyDevice();

    if(ds::Optional<GPTHeaderAndEntries> gpt_opt = ReadGPT()) {
        gpt_ = *gpt_opt;
    }
}

SATAPort::SATAPort(SATAPort &&rhs) noexcept
    : mem_(rhs.mem_)
    , port_(rhs.port_)
    , cmd_header_(rhs.cmd_header_)
    , received_fis_(rhs.received_fis_)
    , dev_info_(rhs.dev_info_)
    , num_slots_(rhs.num_slots_)
    , slots_bitmap_(rhs.slots_bitmap_)
    , ncq_tag_bitmap_(rhs.ncq_tag_bitmap_)
    , max_lba_(rhs.max_lba_)
{
    rhs.mem_ = nullptr;
    rhs.port_ = nullptr;
    rhs.cmd_header_ = nullptr;
    rhs.received_fis_ = nullptr;
    rhs.dev_info_ = nullptr;
    rhs.num_slots_ = 0;
}

SATAPort &SATAPort::operator =(SATAPort &&rhs) noexcept
{
    if(this == &rhs) {
        return *this;
    }

    SuspendCommands();
    rhs.SuspendCommands();

    for(uint8_t i = 0; i < num_slots_; ++i) {
        uintptr_t paddr = cmd_header_[i].cmd_tab_addr_lo_;
        paddr |= ((uint64_t) cmd_header_[i].cmd_tab_addr_hi_ << 32);
        BuddyAllocator::Free((void*) paddr);
    }
    BuddyAllocator::Free((void*) cmd_header_);
    BuddyAllocator::Free((void*) received_fis_);

    mem_ = rhs.mem_;
    port_ = rhs.port_;
    cmd_header_ = rhs.cmd_header_;
    received_fis_ = rhs.received_fis_;
    dev_info_ = rhs.dev_info_;
    num_slots_ = rhs.num_slots_;
    slots_bitmap_ = rhs.slots_bitmap_;
    ncq_tag_bitmap_ = rhs.ncq_tag_bitmap_;
    max_lba_ = rhs.max_lba_;

    rhs.mem_ = nullptr;
    rhs.port_ = nullptr;
    rhs.cmd_header_ = nullptr;
    rhs.received_fis_ = nullptr;
    rhs.dev_info_ = nullptr;
    rhs.num_slots_ = 0;
    rhs.max_lba_ = 0;

    return *this;
}


SATAPort::~SATAPort()
{
    disk_cache_.Flush();
    SuspendCommands();
    for(uint8_t i = 0; i < num_slots_; ++i) {
        uintptr_t paddr = cmd_header_[i].cmd_tab_addr_lo_;
        paddr |= ((uint64_t) cmd_header_[i].cmd_tab_addr_hi_ << 32);
        BuddyAllocator::Free((void*) (paddr));
    }
    BuddyAllocator::Free((void*) cmd_header_);
    BuddyAllocator::Free((void*) received_fis_);

    KHeap::Free(gpt_.hdr_);
    KHeap::Free(gpt_.entries_);

}

void SATAPort::ActivateCommands()
{
    while(port_->cmd_status_ & (1 << 15));

    // NOTE: In-place operators with volatile lvalues are deprecated.
    if(! (port_->cmd_status_ & (1 << 4))) {
        port_->cmd_status_ = port_->cmd_status_ | (1 << 4);
    }
    if(! (port_->cmd_status_ & 1)) {
        port_->cmd_status_ = port_->cmd_status_ | 1;
    }
}

void SATAPort::SuspendCommands()
{
    if(port_->cmd_status_ & 1) {
        port_->cmd_status_ = port_->cmd_status_ & ~(1);
    }
    if(port_->cmd_status_ & (1 << 4)) {
        port_->cmd_status_ = (port_->cmd_status_) & ~(1 << 4);
    }
    while(port_->cmd_status_ & (1 << 14) || port_->cmd_status_ & (1 << 15));
}

void SATAPort::Configure()
{
    SuspendCommands();

    // We use buddy allocator, because the command list must be aligned along
    // a 0x1000-byte (e.g. size of page) boundary.
    size_t cmd_list_size = num_slots_ * sizeof(HBACmd);
    cmd_header_ = (HBACmd*) BuddyAllocator::Allocate(cmd_list_size);
    uintptr_t hdr_paddr = (uintptr_t) cmd_header_;
    port_->cmd_list_base_lo_ = (uint32_t) (hdr_paddr);
    port_->cmd_list_base_hi_ = (uint32_t) (hdr_paddr >> 32);
    memset((void*) cmd_header_, 0, cmd_list_size);

    size_t fis_size = num_slots_ * sizeof(ReceivedFIS);
    received_fis_ = (ReceivedFIS*) BuddyAllocator::Allocate(fis_size);
    memset((void*) received_fis_, 0, fis_size);
    received_fis_->reg_fis_.fis_type_ = FIS_REG_DEV_TO_HOST;
    received_fis_->dma_fis_.fis_type_ = FIS_DMA_BIDIRECTIONAL;
    received_fis_->pio_fis_.fis_type_ = FIS_PIO_DEV_TO_HOST;
    uintptr_t fis_paddr = ((uintptr_t) received_fis_);
    port_->fis_base_lo_ = (uint32_t) (fis_paddr);
    port_->fis_base_hi_ = (uint32_t) (fis_paddr >> 32);

    // Buddy allocator is guaranteed to return a ptr aligned along a page bound-
    // ary, and therefore also aligned along 128-byte boundary needed for cmd
    // tables.
    size_t num_prdts = 8;
    size_t tab_align = 128;
    size_t cmd_tab_size = sizeof(HBACmdTable) + num_prdts * sizeof(PRDTEntry);
    size_t tab_padded_size =
            ((cmd_tab_size + tab_align - 1) / tab_align) * tab_align;
    size_t cum_tab_size = num_slots_ * tab_padded_size;
    auto *cmd_tab_region = (uint8_t*) BuddyAllocator::Allocate(cum_tab_size);
    for(uint8_t i = 0; i < num_slots_; ++i) {
        auto cmd_tab = (HBACmdTable*) (void*) &cmd_tab_region[i*tab_padded_size];
        uintptr_t tab_paddr = ((uintptr_t) cmd_tab);
        cmd_header_[i].no_prdt_entries_ = num_prdts;
        cmd_header_[i].cmd_tab_addr_lo_ = (uint32_t) (tab_paddr);
        cmd_header_[i].cmd_tab_addr_hi_ = (uint32_t) (tab_paddr >> 32);
        memset(cmd_tab, 0, tab_padded_size);
        slots_bitmap_ |= (1 << i);
    }
}

bool SATAPort::Read(uint64_t disk_addr, size_t num_sectors, void *buff)
{
    return CacheReadWrite(disk_addr, num_sectors, buff, false);
}

void *SATAPort::Read(uint64_t disk_addr, size_t num_sectors)
{
    if(void *buff = KHeap::Allocate(num_sectors * SECTOR_SIZE)) {
        if(Read(disk_addr, num_sectors, buff)) {
            return buff;
        }
    }
    return nullptr;
}

bool SATAPort::Write(uint64_t disk_addr, size_t num_sectors, void *buff)
{
    return CacheReadWrite(disk_addr, num_sectors, buff, true);
}


bool SATAPort::Read(const ds::DynArray<DiskRange> &ranges, void *buff)
{
    return RangeReadWrite(ranges, buff, false);
}

bool SATAPort::Write(const ds::DynArray<DiskRange> &ranges, void *buff)
{
    return RangeReadWrite(ranges, buff, true);
}


bool SATAPort::RangeReadWrite(const ds::DynArray<DiskRange> &ranges, void *buff,
                              bool write)
{
    // Find n free ranges.
    bool use_ncq = NCQCapable();
    auto buff_ptr = (char*) buff;
    if(use_ncq) {
        size_t range_ind = 0;

        while(range_ind < ranges.Size()) {
            ds::DynArray<uint8_t> open_slots;

            uint8_t  queue_depth      = dev_info_[75] & 0x1F;
            uint8_t needed_slots      = queue_depth < ranges.Size() - range_ind ?
                        queue_depth : ranges.Size() - range_ind;
            uint32_t valid_slots_mask = ((1 << needed_slots) - 1);
            uint32_t slots_map        = (port_->cmd_issue_ & valid_slots_mask);
            while (slots_map != valid_slots_mask) {
                size_t free_slot = slots_map ? __builtin_ffsl(~slots_map) : 1;
                free_slot -= 1;
                slots_map |= 1 << (free_slot);
                open_slots.Append(free_slot);
            }

            uint32_t used_slot_bmap = 0;
            for (int i = 0; i < open_slots.Size(); ++i, ++range_ind) {
                SetupReadWrite(open_slots[i], ranges[i].start_lba_,
                               ranges[i].len_, buff_ptr, write, 0, true);
                used_slot_bmap |= (1 << i);
                buff_ptr += ranges[i].len_ * SECTOR_SIZE;
            }


            while(IsBusy());
            ActivateCommands();

            port_->sata_active_ = used_slot_bmap;
            port_->cmd_issue_ = used_slot_bmap;


            while((port_->sata_active_ & used_slot_bmap) && ! OpFailed());

            if(OpFailed()) {
                Log("Op failed");
                SuspendCommands();
                return false;
            }
            SuspendCommands();
        }
    } else {
        for(int i = 0; i < ranges.Size(); ++i) {
            if(write) {
                if(! Write(ranges[i].start_lba_, ranges[i].len_, buff_ptr)) {
                    return false;
                }
            } else if(! Read(ranges[i].start_lba_, ranges[i].len_, buff_ptr)) {
                return false;
            }
            buff_ptr += ranges[i].len_ * SECTOR_SIZE;
        }
    }

    return true;
}

ds::Optional<uint64_t> SATAPort::GetDiskCapacity()
{
    if(max_lba_ != -1) {
        return max_lba_;
    }

    uint8_t free_slot;
    do { free_slot = FirstFreeSlot(); } while(free_slot > NumberSlots());
    slots_bitmap_ &= ~(1 << free_slot);
    volatile HBACmdTable *tab = SlotToCmdTab(free_slot);

    // Clear out any remaining config info from last use of this table.
    memset((void*) tab, 0, sizeof(HBACmdTable) + 8 * sizeof(PRDTEntry));

    auto fis = (HostToDevRegisterFIS*) &tab->cmd_fis_;
    fis->fis_type_ = FIS_REG_HOST_TO_DEV;
    fis->command_ = GET_MAX_ADDR;
    fis->device_ = LBA_MODE;
    fis->port_multiplier_ = 0;
    fis->cmd_ctrl_ = 1;

    while(IsBusy());
    ActivateCommands();
    IssueCommand(free_slot);
    while(OpInProgress(free_slot) && ! OpFailed());

    if(OpFailed()) {
        slots_bitmap_ |= (1 << free_slot);
        SuspendCommands();
        Log("Op failed\n");
        return ds::NullOpt;
    }

    auto max_disk_sectors = (uint64_t) received_fis_->reg_fis_.lba0;
    max_disk_sectors |= ((uint64_t) received_fis_->reg_fis_.lba1 << 8);
    max_disk_sectors |= ((uint64_t) received_fis_->reg_fis_.lba2 << 16);
    max_disk_sectors |= ((uint64_t) received_fis_->reg_fis_.lba3 << 24);
    max_disk_sectors |= ((uint64_t) received_fis_->reg_fis_.lba4 << 32);
    max_disk_sectors |= ((uint64_t) received_fis_->reg_fis_.lba5 << 40);

    slots_bitmap_ |= (1 << free_slot);
    SuspendCommands();
    return max_disk_sectors;
}

bool SATAPort::IsBusy() const
{
    return port_->task_file_data_ & (0x80 | 0x08);
}

uint8_t SATAPort::NumberSlots() const
{
    uint32_t slots_mask = 0b11111 << 8;
    return (uint8_t) ((mem_->capability_ & slots_mask) >> 8);
}

void SATAPort::ParseError() const
{

}

bool SATAPort::IdentifyDevice()
{
    Log("\nID DEVICE\n");
    uint8_t free_slot;
    do { free_slot = FirstFreeSlot(); } while(free_slot > NumberSlots());
    slots_bitmap_ &= ~(1 << free_slot);
    volatile HBACmd *header = &cmd_header_[free_slot];
    volatile HBACmdTable *tab = SlotToCmdTab(free_slot);

    // Clear out any remaining config info from last use of this table.
    memset((void*) tab, 0, sizeof(HBACmdTable));

    auto fis = (HostToDevRegisterFIS*) &tab->cmd_fis_;
    fis->fis_type_ = FIS_REG_HOST_TO_DEV;
    fis->device_ = 0;
    fis->port_multiplier_ = 0;
    fis->cmd_ctrl_ = 1;
    fis->command_ = IDENTIFY_DEVICE;

    header->cmd_fis_len_ = sizeof(DeviceToHostRegisterFIS) / 4;
    header->write_ = 0;
    header->no_prdt_entries_ = 1;

    // Identify device always returns a 512-byte struct, so only one PRDT
    // is needed.
    uintptr_t buff_paddr = KHeap::ToPAddr((uintptr_t) dev_info_);
    tab->prdt_entries[0].data_addr_lo_ = (uint32_t) (buff_paddr);
    tab->prdt_entries[0].data_addr_hi_ = (uint32_t) (buff_paddr >> 32);
    tab->prdt_entries[0].byte_count_ = 511;

    while (IsBusy());
    ActivateCommands();

    IssueCommand(free_slot);
    while(OpInProgress(free_slot) && ! OpFailed());

    bool ret = ! OpFailed();
    slots_bitmap_ |= (1 << free_slot);
    SuspendCommands();
    Log("WORD 75 IS 0x%x\n", dev_info_[75]);
    Log("MAX SIZE OF DISK IS 0x%x\n", *((uint64_t*)&dev_info_[100]));
    max_lba_ = (int64_t) *((uint64_t*)&dev_info_[100]);
    return ret;
}

bool SATAPort::CacheReadWrite(uint64_t disk_addr, size_t num_sectors, void *buff,
                              bool write, uint8_t prio)
{
    uintptr_t buff_paddr = KHeap::ToPAddr((uintptr_t) buff);
    ds::Optional<CachedSector*> cached_block;
    for(size_t i = 0; i < num_sectors; ++i) {
        // Iterate through blocks until we find one that has been cached.
        size_t num_uncached = 0;
        size_t first_uncached = i;
        for(; i < num_sectors; ++i, ++num_uncached) {
            if((cached_block = disk_cache_.Lookup(disk_addr + i))) {
                break;
            }
        }

        // If there are uncached blocks and this is a read, read from the disk.
        // If it's a write, just write to the cache and it'll get written back
        // eventually.
        uintptr_t uncached_dma_base = SECTOR_SIZE * first_uncached + buff_paddr;
        if(num_uncached && ! write) {
            bool ret = DiskReadWrite(disk_addr + first_uncached, num_uncached,
                                     (void*) uncached_dma_base, false, prio);
            if(! ret) {
                return false;
            }
        }

        // For each uncached block, read it into cache and alter its contents
        // there. This will eventually be written back to the disk.
        for(size_t j = 0; j < num_uncached; ++j) {
            void *cache_entry = KHeap::Allocate(SECTOR_SIZE);
            memcpy(cache_entry, (void*) (uncached_dma_base + j * SECTOR_SIZE),
                   SECTOR_SIZE);
            auto cached_sector = new CachedSector {
                .sector_ = cache_entry,
                .dirty_ = write,
                .disk_ = this
            };

            disk_cache_.Insert(disk_addr + first_uncached + j, cached_sector);
        }

        uintptr_t cached_dma_base = SECTOR_SIZE * i + buff_paddr;
        if(cached_block) {
            if(write) {
                (*cached_block)->dirty_ = true;
                memcpy((*cached_block)->sector_, (void*) cached_dma_base,
                       SECTOR_SIZE);
            } else {
                memcpy((void*) cached_dma_base, (*cached_block)->sector_,
                       SECTOR_SIZE);
            }
        }
    }
    return true;
}

bool SATAPort::DiskReadWrite(uint64_t disk_addr, size_t num_sectors, void *buff,
                             bool write, uint8_t priority)
{
    size_t capacity = GetDiskCapacity();
    if(disk_addr > max_lba_) {
        Log("disk_addr 0x%x exceeds disk capacity 0x%x\n", disk_addr, capacity);
        return false;
    }

    uint8_t free_slot;
    bool use_ncq = NCQCapable();
    do { free_slot = FirstFreeSlot(); } while(free_slot > NumberSlots());
    SetupReadWrite(free_slot, disk_addr, num_sectors, buff, write, priority,
                   use_ncq);

    while (IsBusy());
    ActivateCommands();

    IssueCommand(free_slot, use_ncq);

    while(OpInProgress(free_slot, use_ncq) && ! OpFailed());

    bool status = true;
    if(OpFailed()) {
        Log("Op failed\n");
        status = false;
    }
    slots_bitmap_ |= (1 << free_slot);

    SuspendCommands();
    return status;
}


void SATAPort::SetupReadWrite(uint8_t free_slot, uint64_t disk_addr,
                              size_t num_sectors, void *buff, bool write,
                              uint8_t priority, bool use_ncq)
{
    volatile HBACmd *header = &cmd_header_[free_slot];
    volatile HBACmdTable *tab = SlotToCmdTab(free_slot);

    // Clear out any remaining config info from last use of this table.
    memset((void*) tab, 0, sizeof(HBACmdTable));


    header->cmd_fis_len_ = sizeof(DeviceToHostRegisterFIS) / 4;
    header->write_ = write;
    header->no_prdt_entries_ = ((num_sectors - 1) / SECTORS_PER_PRDT) + 1;

    uintptr_t buff_paddr = KHeap::ToPAddr((uintptr_t) buff);
    size_t rem_bytes = num_sectors * SECTOR_SIZE;

    for(uint16_t i = 0; i < header->no_prdt_entries_ && rem_bytes > 0; ++i) {
        uintptr_t dma_base = KHeap::ToPAddr(buff_paddr + i * PRDT_SIZE);
        size_t bytes = (rem_bytes < PRDT_SIZE) ? rem_bytes : PRDT_SIZE;
        rem_bytes -= bytes;

        tab->prdt_entries[i].data_addr_lo_ = (uint32_t) (dma_base);
        tab->prdt_entries[i].data_addr_hi_ = (uint32_t) (dma_base >> 32);
        tab->prdt_entries[i].byte_count_ = bytes - 1;
        tab->prdt_entries[i].interrupt_ = 1;

        //Log("Reading 0x%x bytes into 0x%x\n", bytes, dma_base);
    }

    auto fis = (HostToDevRegisterFIS*) &tab->cmd_fis_;
    fis->fis_type_ = FIS_REG_HOST_TO_DEV;
    fis->command_ = use_ncq ? (write ? DMA_FPDMA_WRITE : DMA_FPDMA_READ) :
                              (write ? DMA_WRITE : DMA_READ);
    fis->device_ = LBA_MODE;
    fis->port_multiplier_ = 0;
    fis->cmd_ctrl_ = 1;
    fis->lba0 = (uint8_t) (disk_addr);
    fis->lba1 = (uint8_t) (disk_addr >> 8);
    fis->lba2 = (uint8_t) (disk_addr >> 16);
    fis->lba3 = (uint8_t) (disk_addr >> 24);
    fis->lba4 = (uint8_t) (disk_addr >> 32);
    fis->lba5 = (uint8_t) (disk_addr >> 40);

    if(use_ncq) {
        fis->feature_lo_ = (uint8_t) (num_sectors);
        fis->feature_hi_ = (uint8_t) (num_sectors >> 8);
        fis->count_lo_ = (uint8_t) free_slot << 3;
        fis->count_hi_ = (uint8_t) priority << 6;
    } else {
        fis->count_lo_ = (uint8_t) (num_sectors);
        fis->count_hi_ = (uint8_t) (num_sectors >> 8);
    }
}


void SATAPort::IssueCommand(uint8_t slot, bool ncq)
{
    // Can't use |= due to deprecation of compound ops on volatile ptrs.
    //port_->cmd_issue_ = port_->cmd_issue_ | (1 << slot);
    port_->cmd_issue_ = (1 << slot);
    if(ncq) {
        port_->sata_active_ = port_->sata_active_ | ((1 << slot));
    }
}

uint8_t SATAPort::FirstFreeSlot(bool ncq) const
{
    uint8_t queue_depth = dev_info_[75] & 0x1F;
    uint32_t valid_slots = port_->cmd_issue_ & ((1 << queue_depth) - 1);
    return __builtin_ffsl(~(valid_slots)) - 1;
}

HBACmdTable *SATAPort::SlotToCmdTab(uint8_t slot)
{
    auto addr_lo = (uintptr_t) cmd_header_[slot].cmd_tab_addr_lo_;
    auto addr_hi = (uintptr_t) cmd_header_[slot].cmd_tab_addr_hi_;
    return (HBACmdTable*) ((addr_hi << 32) | addr_lo);
}

bool SATAPort::OpInProgress(uint8_t slot, bool ncq) const
{
    if(ncq) {
        return port_->sata_active_ & (1 << slot);
    }
    return (port_->cmd_issue_ & (1 << slot));
}

bool SATAPort::OpFailed() const
{
    //return received_fis_->reg_fis_.status_ & STATUS_ERR ||
    //       port_->int_status_ & DISK_ERR;
    bool val = received_fis_->reg_fis_.status_ & STATUS_ERR ||
               port_->int_status_ & DISK_ERR || port_->sata_error_;
    return val;
}

bool SATAPort::NCQCapable() const
{
    return false;
    return mem_->capability_ & NCQ_CAPABLE;
}

void SATAPort::HandleEviction(uint64_t sector_no, CachedSector *cache_entry)
{
    if(cache_entry->dirty_) {
        cache_entry->disk_->DiskReadWrite(sector_no, 1, cache_entry->sector_,
                                         true);
    }

    KHeap::Free(cache_entry->sector_);
    delete cache_entry;
}
ds::Optional<GPTEntry> SATAPort::GetNthPartition(size_t n)
{
    if (n < gpt_.hdr_->num_part_entries_) {
        return ds::NullOpt;
    }

    auto entry = (GPTEntry *) (gpt_.entries_ + gpt_.hdr_->entry_size_ * n);
    return *entry;
}

ds::Optional<GPTEntry> SATAPort::FindPartition(uint64_t part_guid_lo,
                                               uint64_t part_guid_hi)
{
    for (size_t i = 0; i < gpt_.hdr_->num_part_entries_; ++i) {
        auto entry = (GPTEntry *) (gpt_.entries_ + gpt_.hdr_->entry_size_ * i);

        bool guid_matches = part_guid_lo == entry->part_type_guid_lo_;
        guid_matches &= part_guid_hi == entry->part_type_guid_hi_;

        if (guid_matches) {
            return *entry;
        }
    }

    return ds::NullOpt;
}

ds::Optional<ds::DynArray<DiskRange>>
SATAPort::GetPartitionRanges(const GPTEntry &part)
{
    ds::DynArray<DiskRange> ranges;
    for (size_t i = 0; i < gpt_.hdr_->num_part_entries_; ++i) {
        auto entry = (GPTEntry *) (gpt_.entries_ + gpt_.hdr_->entry_size_ * i);
        if(memcmp(part.uniq_part_guid_, entry->uniq_part_guid_, 16) == 0) {
            uint64_t len = entry->end_lba_ - entry->start_lba_;
            ranges.Append({ entry->start_lba_, len });
        }
    }

    return ranges;
}

ds::Optional<SATAPort::GPTHeaderAndEntries> SATAPort::ReadGPT()
{
    void *lba1 = Read(1, 1);
    auto gpt_hdr = (GPTHeader*) lba1;
    if(gpt_hdr->signature_ != GPT_MAGIC) {
        KHeap::Free(lba1);
        return ds::NullOpt;
    }

    ds::DynArray<DiskRange> ranges;
    size_t arr_size = gpt_hdr->num_part_entries_ * gpt_hdr->entry_size_;
    size_t no_sectors = arr_size / SECTOR_SIZE + (arr_size % SECTOR_SIZE > 0);
    void *entry_arr = Read(gpt_hdr->entry_arr_lba_, no_sectors);

    return (GPTHeaderAndEntries) {
            .hdr_ = (GPTHeader*) lba1,
            .entries_ = (char*) entry_arr
    };
}

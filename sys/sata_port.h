#ifndef AHCI_DISK_H
#define AHCI_DISK_H

#include <sys/kheap.h>
#include <ds/dyn_array.h>
#include <ds/owning_ptr.h>
#include <ds/cache.h>

constexpr uint64_t GPT_MAGIC = 0x5452415020494645;
constexpr uint64_t ESP_GUID_LO = 0x11d2f81fc12a7328;
constexpr uint64_t ESP_GUID_HI = 0x3bc93ec9a0004bba;

struct GPTHeader {
    uint64_t signature_;
    uint32_t gpt_revision_;
    uint32_t header_size_;
    uint32_t header_checksum_;
    uint32_t reserved0;
    uint64_t header_lba_;
    uint64_t alt_header_lba_;
    uint64_t first_usable_lba_;
    uint64_t last_usable_lba_;
    uint8_t disk_guid[16];
    uint64_t entry_arr_lba_;
    uint32_t num_part_entries_;
    uint32_t entry_size_;
    uint32_t part_arr_checksum_;
} __attribute__((packed));

struct GPTEntry {
    uint64_t part_type_guid_lo_;
    uint64_t part_type_guid_hi_;
    uint8_t uniq_part_guid_[16];
    uint64_t start_lba_;
    uint64_t end_lba_;
    uint64_t attrs_;
    char part_name_[];
} __attribute__((packed));

struct DMASetupFIS {
    uint8_t fis_type_;
    uint8_t port_multiplier_        :   4;
    uint8_t reserved0               :   1;
    uint8_t direction_              :   1;
    uint8_t interrupt_              :   1;
    uint8_t auto_activate_          :   1;
    uint16_t reserved1;
    uint64_t dma_buffer_id_;
    uint32_t reserved2;
    uint32_t dma_offset_;
    uint32_t xfer_count_;
    uint32_t reserved3;
} __attribute__((packed));

struct PIOSetupFIS {
    uint8_t fis_type_;
    uint8_t port_multiplier_        :   4;
    uint8_t reserved0               :   1;
    uint8_t direction_              :   1;
    uint8_t interrupt_              :   1;
    uint8_t auto_activate_          :   1;
    uint8_t status_;
    uint8_t err_;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device_;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t reserved1;
    uint8_t count_lo_;
    uint8_t count_hi_;
    uint8_t reserved2;
    uint8_t new_status_;
    uint16_t xfer_count_;
    uint16_t reserved3;
} __attribute__((packed));

struct HostToDevRegisterFIS {
    uint8_t fis_type_;
    uint8_t port_multiplier_    :   4;
    uint8_t reserved0           :   3;
    uint8_t cmd_ctrl_           :   1;
    uint8_t command_;
    uint8_t feature_lo_;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device_;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t feature_hi_;
    uint8_t count_lo_;
    uint8_t count_hi_;
    uint8_t isochronous_cmd_completion_;
    uint8_t ctrl_;
    uint8_t reserved1[4];
} __attribute__((packed));

struct DeviceToHostRegisterFIS {
    uint8_t fis_type_;
    uint8_t port_multiplier_    :   4;
    uint8_t reserved0           :   2;
    uint8_t interrupt_          :   1;
    uint8_t reserved1           :   1;
    uint8_t status_;
    uint8_t err_;
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device_;
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t reserved2;
    uint8_t count_lo_;
    uint8_t count_hi_;
    uint8_t reserved3[6];
} __attribute__((packed));


struct ReceivedFIS {
    DMASetupFIS dma_fis_;
    uint32_t reserved0;
    PIOSetupFIS pio_fis_;
    uint8_t reserved1[12];
    DeviceToHostRegisterFIS reg_fis_;
    uint8_t reserved2[4];
    uint8_t set_dev_bits_fis_[8];
    uint8_t unknown_fis[64];
    uint8_t reserved3[0x60];
} __attribute__((packed));

struct HBACmd {
    uint8_t cmd_fis_len_        :   5;
    uint8_t atapi_              :   1;
    uint8_t write_              :   1;
    uint8_t prefetchable_       :   1;
    uint8_t reset_              :   1;
    uint8_t bist_               :   1;
    uint8_t clear_busy_         :   1;
    uint8_t reserved0           :   1;
    uint8_t port_multiplier_    :   4;
    uint16_t no_prdt_entries_;
    volatile uint32_t prd_xfer_count_;
    uint32_t cmd_tab_addr_lo_;
    uint32_t cmd_tab_addr_hi_;
    uint8_t reserved1[16];
} __attribute__((packed));

struct PRDTEntry {
    uint32_t data_addr_lo_;
    uint32_t data_addr_hi_;
    uint32_t reserved0;
    uint32_t byte_count_        :   22;
    uint32_t reserved1          :   9;
    uint32_t interrupt_         :   1;
} __attribute__((packed));

struct HBACmdTable {
    uint8_t cmd_fis_[64];
    uint8_t atapi_cmd_[16];
    uint8_t reserved0[48];
    PRDTEntry prdt_entries[];
} __attribute__((packed));

struct HBAPort {
    uint32_t cmd_list_base_lo_;
    uint32_t cmd_list_base_hi_;
    uint32_t fis_base_lo_;
    uint32_t fis_base_hi_;
    uint32_t int_status_;
    uint32_t int_enable_;
    uint32_t cmd_status_;
    uint32_t reserved0_;
    uint32_t task_file_data_;
    uint32_t signature_;
    uint32_t sata_status_;
    uint32_t sata_control_;
    uint32_t sata_error_;
    uint32_t sata_active_;
    uint32_t cmd_issue_;
    uint32_t sata_notification_;
    uint32_t fis_based_switch_control_;
    uint32_t rsv1_[11];
    uint32_t vendor_[4];
} __attribute((packed));

struct HBAMem {
    uint32_t capability_;
    uint32_t global_host_control_;
    uint32_t interrupt_status_;
    uint32_t port_implemented_;
    uint32_t version_;
    uint32_t cmd_comp_coalesce_control_;
    uint32_t cmd_comp_coalesce_ports_;
    uint32_t enc_man_location_;
    uint32_t enc_man_control_;
    uint32_t capability_upper_;
    uint32_t bios_os_handoff_control_;
    uint8_t  reserved_[0xA0-0x2C];
    uint8_t  vendor_[0x100-0xA0];

    HBAPort ports_[];
} __attribute__((packed));

enum ahci_dev_t
{
    AHCI_DEV_NULL = 0,
    AHCI_DEV_SATA = 1,
    AHCI_DEV_SEMB = 2,
    AHCI_DEV_PM  = 3,
    AHCI_DEV_SATAPI = 4
};

enum ahci_dev_sig_t
{
    SATA_SIG_ATA   = 0x00000101, // SATA drive
    SATA_SIG_ATAPI = 0xEB140101, // SATAPI drive
    SATA_SIG_SEMB  = 0xC33C0101, // Enclosure management bridge
    SATA_SIG_PM    = 0x96690101 // Port multiplier
};

enum fis_t {
    FIS_REG_HOST_TO_DEV = 0x27,
    FIS_REG_DEV_TO_HOST = 0x34,
    FIS_DMA_DEV_TO_HOST = 0x39,
    FIS_DMA_BIDIRECTIONAL = 0x41,
    FIS_DATA_BIDIRECTIONAL = 0x46,
    FIS_BIST_BIDIRECTIONAL = 0x58,
    FIS_PIO_DEV_TO_HOST = 0x5F,
    FIS_BITS_DEV_TO_HOST = 0xA1
};

ds::DynArray<class SATAPort*> EnumerateDevices(volatile void *hba_mmio_base);

struct DiskRange {
    uint64_t start_lba_;
    size_t len_;
};

class SATAPort : public KernelAllocated<SATAPort>
{
public:
    SATAPort(volatile HBAMem *mem, volatile HBAPort *port);
    SATAPort(SATAPort &&rhs) noexcept;
    SATAPort& operator=(SATAPort &&rhs) noexcept;
    SATAPort(const SATAPort&) = delete;
    SATAPort& operator=(const SATAPort&) = delete;
    ~SATAPort();

    void ActivateCommands();

    void SuspendCommands();

    void Configure();

    bool Read(uint64_t disk_addr, size_t num_sectors, void *buff);

    void *Read(uint64_t disk_addr, size_t num_sectors);

    bool Read(const ds::DynArray<DiskRange> &ranges, void *buff);

    bool Write(uint64_t disk_addr, size_t num_sectors, void *buff);

    bool Write(const ds::DynArray<DiskRange> &ranges, void *buff);

    ds::Optional<uint64_t> GetDiskCapacity();

    bool IsBusy() const;

    uint8_t NumberSlots() const;

    void ParseError() const;

    ds::Optional<GPTEntry> FindPartition(uint64_t part_guid_lo,
                                         uint64_t part_guid_hi);

    ds::Optional<ds::DynArray<DiskRange>>
    GetPartitionRanges(const GPTEntry &part);

    ds::Optional<GPTEntry> GetNthPartition(size_t n);

private:
    volatile HBAMem *mem_;
    volatile HBAPort *port_;
    volatile HBACmd *cmd_header_;
    volatile ReceivedFIS *received_fis_;
    uint16_t *dev_info_;
    uint8_t num_slots_;
    uint32_t slots_bitmap_;
    // NCQ tag must not exceed value in bits 0:4 of word 75 of ID Dev info.
    // Hence, 32-bit bitmap can store all possible vals.
    uint32_t ncq_tag_bitmap_;
    struct CachedSector {
        void *sector_;
        bool dirty_;
        SATAPort *disk_;
    };
    ds::LRUCache<uint64_t, CachedSector*> disk_cache_;
    int64_t max_lba_;
    size_t padded_cmd_tab_size_;

    struct GPTHeaderAndEntries {
        GPTHeader *hdr_;
        char *entries_;
    } gpt_;

    static constexpr size_t num_prdts_ = 8;
    static constexpr size_t SECTOR_SIZE = 512;
    static constexpr size_t PRDT_SIZE = 4 * 1024 * 1024;
    static constexpr size_t SECTORS_PER_PRDT = PRDT_SIZE / SECTOR_SIZE;
    static constexpr uint32_t LBA_MODE = 1 << 6;
    static constexpr uint32_t DISK_ERR = (1 << 30);
    static constexpr uint32_t NCQ_CAPABLE = (1 << 30);
    static constexpr uint8_t STATUS_ERR = 1;
    static constexpr uint8_t DMA_READ = 0x25;
    static constexpr uint8_t DMA_WRITE = 0x35;
    static constexpr uint8_t DMA_FPDMA_READ = 0x60;
    static constexpr uint8_t DMA_FPDMA_WRITE = 0x61;
    static constexpr uint8_t IDENTIFY_DEVICE = 0xEC;
    static constexpr uint8_t GET_MAX_ADDR = 0x78;

    bool IdentifyDevice();

    /**
     * A generic wrapper for reads and writes to the disk.
     *
     * @param disk_addr The address of the disk to which we read/write.
     * @param num_sectors The number of sectors to read/write to.
     * @param buff The buffer that is read into (read) or written to the disk
     *             (write).
     * @param write Is this a write or a read?
     * @return Did op succeed?
     */
    bool DiskReadWrite(uint64_t disk_addr, size_t num_sectors, void *buff,
                       bool write, uint8_t prio= 0b00);

    bool CacheReadWrite(uint64_t disk_addr, size_t num_sectors, void *buff,
                        bool write, uint8_t priority=0b00);

    int8_t AssignNCQTag();

    void FreeNCQTag(int8_t tag);

    void IssueCommand(uint8_t slot, bool ncq=false);

    uint8_t FirstFreeSlot(bool ncq=false) const;

    HBACmdTable *SlotToCmdTab(uint8_t slot);
    HBACmd *SlotToCmdHeader(uint8_t slot);

    bool OpInProgress(uint8_t slot, bool ncq=false) const;

    bool OpFailed() const;

    bool NCQCapable() const;

    void SetupReadWrite(uint8_t slot, uint64_t lba, size_t sectors,
                        void *buff, bool write, uint8_t priority=0b00,
                        bool ncq=false);

    bool RangeReadWrite(const ds::DynArray<DiskRange> &ranges, void *buff,
                        bool write);

    static void HandleEviction(uint64_t sector_no, CachedSector *cache_entry);

    ds::Optional<GPTHeaderAndEntries> ReadGPT();
};

#endif

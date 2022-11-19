//#ifndef GPT_H
//#define GPT_H
//
//#include <stdint.h>
//#include <stddef.h>
//#include <sys/sata_port.h>
//#include <libc/string.h>
//
//constexpr uint64_t GPT_MAGIC = 0x5452415020494645;
//constexpr uint64_t ESP_GUID_LO = 0x11d2f81fc12a7328;
//constexpr uint64_t ESP_GUID_HI = 0x3bc93ec9a0004bba;
//
//struct GPTHeader {
//    uint64_t signature_;
//    uint32_t gpt_revision_;
//    uint32_t header_size_;
//    uint32_t header_checksum_;
//    uint32_t reserved0;
//    uint64_t header_lba_;
//    uint64_t alt_header_lba_;
//    uint64_t first_usable_lba_;
//    uint64_t last_usable_lba_;
//    uint8_t disk_guid[16];
//    uint64_t entry_arr_lba_;
//    uint32_t num_part_entries_;
//    uint32_t entry_size_;
//    uint32_t part_arr_checksum_;
//} __attribute__((packed));
//
//struct GPTEntry {
//    uint64_t part_type_guid_lo_;
//    uint64_t part_type_guid_hi_;
//    uint8_t uniq_part_guid_[16];
//    uint64_t start_lba_;
//    uint64_t end_lba_;
//    uint64_t attrs_;
//    char part_name_[];
//} __attribute__((packed));
//
//ds::Optional<GPTEntry> FindPartition(SATAPort &port, uint64_t part_guid_lo,
//                                     uint64_t part_guid_hi);
//
//ds::Optional<ds::DynArray<DiskRange>> GetPartitionRanges(const GPTEntry &part);
//
//ds::Optional<GPTEntry> GetNthPartition(SATAPort &port, size_t n);
//
//#endif
//
#ifndef ACPI_H
#define ACPI_H

#include <ds/string.h>
#include <ds/optional.h>
#include <ds/hash_map.h>
#include <sys/log.h>
#include <sys/stivale2.h>

namespace ACPI {
/******************************* RSD(T/P)-RELATED TYPES. **********************/
struct RSDPDescriptor {
    // V1.
    char signature_[8];
    uint8_t checksum_;
    char oemid_[6];
    uint8_t revision_;
    uint32_t rsdt_addr_;

    // V2.
    uint32_t length_;
    uint64_t xsdt_addr_;
    uint8_t extended_checksum_;
    uint8_t reserved_[3];
} __attribute__((packed));

struct SDTHeader {
    char signature_[4];
    uint32_t length_;
    uint8_t revision_;
    uint8_t checksum_;
    char oemid_[6];
    char oem_table_[8];
    uint32_t oem_revision_;
    uint32_t creator_id_;
    uint32_t creator_revision_;
} __attribute__((packed));

struct RSDT {
    SDTHeader header_;
    uint32_t next_[];
} __attribute__((packed));

struct XSDT {
    SDTHeader header_;
    uint64_t next_[];
} __attribute__((packed));

/******************************* MCFG-RELATED TYPES. **************************/
struct MCFG {
    SDTHeader header_;
    uint64_t reserved_;
    struct ConfigSpace {
        uint64_t base_addr_;
        uint16_t pci_seg_num_;
        uint8_t start_bus_;
        uint8_t end_bus_;
        uint32_t reserved_;
    } __attribute__((packed)) config_spaces_[];
} __attribute((packed));



static constexpr uint8_t RSDP_V1_DESC_SIZE		=	20;
static constexpr uint8_t RSDP_V2_DESC_SIZE		=	36;
static constexpr uint8_t ACPI_VERSION_1			=	0;
static constexpr uint8_t ACPI_VERSION_2			=	2;

bool ValidateRSDP(RSDPDescriptor *desc);

bool ValidateSDT(void* sdt, uint32_t len);

ds::Optional<ds::HashMap<ds::String, void*>>
ParseRoot(stivale2_struct_tag_rsdp *rsdp_hdr);
}

#endif

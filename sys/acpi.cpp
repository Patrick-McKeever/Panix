#include <sys/acpi.h>

namespace ACPI {
bool ValidateRSDP(RSDPDescriptor* desc)
{
    int      checksum   = 0;
    uint8_t* rsdp_bytes = (uint8_t*) desc;

    for (int i = 0; i < RSDP_V1_DESC_SIZE; ++i) {
        checksum += rsdp_bytes[i];
    }

    // Checksums are valid if lower byte is all set to 0.
    bool v1_valid = (checksum & 0xff) == 0x00;
    if (desc->revision_ == ACPI_VERSION_1) {
        return v1_valid;
    }

    for (uint8_t i = RSDP_V1_DESC_SIZE; i < RSDP_V2_DESC_SIZE; ++i) {
        checksum += rsdp_bytes[i];
    }

    bool v2_valid = (checksum & 0xff) == 0x00;
    return v1_valid && v2_valid;
}

bool ValidateSDT(void* sdt, uint32_t len)
{
    int checksum = 0;

    // First byte is actually a bool telling us if this is XSDT or RSDT. All
    // subsequent bytes are identical to the RSDT/XSDT from memory.
    uint8_t* sdt_bytes = (uint8_t*) (sdt);
    for (size_t i = 0; i < len; ++i) {
        checksum += sdt_bytes[i];
    }
    return (checksum & 0xff) == 0x00;
}

ds::Optional<ds::HashMap<ds::String, void*>>
ParseRoot(stivale2_struct_tag_rsdp* rsdp_hdr)
{
    RSDPDescriptor* rsdp = (RSDPDescriptor*) (rsdp_hdr->rsdp);
    if (! ValidateRSDP(rsdp)) {
        Log("RSDP NOT VALIDATED\n");
        return ds::NullOpt;
    }

    bool      uses_xsdt = rsdp->revision_ >= ACPI_VERSION_2;
    XSDT*     xsdt      = uses_xsdt ? (XSDT*) rsdp->xsdt_addr_ : nullptr;
    RSDT*     rsdt = uses_xsdt ? nullptr : (RSDT*) (uintptr_t) rsdp->rsdt_addr_;
    SDTHeader sdt_head  = uses_xsdt ? xsdt->header_ : rsdt->header_;
    bool      validated = ValidateSDT((uses_xsdt ? (void*) xsdt : (void*) rsdt),
                                 sdt_head.length_);
    if (! validated) {
        return ds::NullOpt;
    }

    uint8_t entry_size  = uses_xsdt ? 8 : 4;
    size_t  num_entries = (sdt_head.length_ - sizeof(SDTHeader)) / entry_size;
    ds::HashMap<ds::String, void*> tables;

    for (size_t i = 0; i < num_entries; ++i) {
        // Seems to cause protection fault when actually accessing this stuff.
        // Log("Next addr is 0x%x\n", uses_xsdt ? xsdt_next[i] : rsdt_next[i]);
        auto* sdt = (SDTHeader*) (uses_xsdt ? xsdt->next_[i] : rsdt->next_[i]);

        if (ValidateSDT((void*) sdt, sdt->length_)) {
            // Need to specify len, since signature isn't null-terminated.
            ds::String tab_name(sdt->signature_, 4);
            if (! tables.Insert(tab_name, (void*) sdt)) {
                Log("Insert to hash map failed.\n");
                return ds::NullOpt;
            }
        } else {
            Log("SDT not validated.\n");
            return ds::NullOpt;
        }
    }
    return tables;
}
}
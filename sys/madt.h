#ifndef MADT_H
#define MADT_H

#include <sys/acpi.h>
#include <ds/dyn_array.h>

namespace ACPI::MADT {
struct MADTRecord
{
    SDTHeader header;
    uint32_t  local_apic_addr;
    uint32_t  local_apic_flags;
}  __attribute__((packed));

struct MADTRecordHeader
{
    uint8_t record_type;
    uint8_t record_len;
} __attribute__((packed));

struct LAPICRecord
{
    MADTRecordHeader header;
    uint8_t          acpi_processor_id;
    uint8_t          apic_id;
    uint32_t         flags;
} __attribute__((packed));

struct IOAPICRecord
{
    MADTRecordHeader header;
    uint8_t          io_apic_id;
    uint8_t          reserved;
    uint32_t         io_apic_addr;
    uint32_t         gsi_base;
} __attribute__((packed));

// Interrupt source override (ISO) record.
struct ISORecord
{
    MADTRecordHeader header;
    uint8_t          bus_source;
    uint8_t          irq_source;
    uint32_t         gsi;
    uint16_t         flags;
} __attribute__((packed));

// Non-maskable interrupt (NMI) source record.
struct NMISourceRecord
{
    MADTRecordHeader header;
    uint8_t          nmi_source;
    uint8_t          reserved;
    uint16_t         flags;
    uint32_t         gsi;
} __attribute__((packed));

struct NMIRecord
{
    MADTRecordHeader header;
    uint8_t          acpi_processor_id;
    uint16_t         flags;
    bool             lint;
} __attribute__((packed));

// With this, ACPI gives the physical 64-bit addr of the APIC, since the
// previous addr had been in 32 bits. There will be at most one of these
// records in the MADT.
struct LAPICAddrRecord
{
    MADTRecordHeader header;
    uint16_t         reserved;
    uint64_t         local_apic_addr;
} __attribute__((packed));

struct X2LAPICRecord
{
    MADTRecordHeader header;
    uint16_t         reserved;
    uint32_t         local_x2_apic_id;
    uint32_t         flags;
    uint32_t         acpi_id;
} __attribute__((packed));

enum MADTRecordType
{
    PROCESSOR_LOCAL_APIC          = 0x00,
    IO_APIC                       = 0x01,
    INTERRUPT_SOURCE_OVERRIDE     = 0x02,
    NON_MASKABLE_INTERRUPT_SOURCE = 0x03,
    LOCAL_NON_MASKABLE_INTERRUPT  = 0x04,
    LOCAL_APIC_ADDRESS_OVERRIDE   = 0x05,
    PROCESSOR_LOCAL_X2_APIC       = 0x09,
};
}


#endif

#ifndef INT_HANDLER_H
#define INT_HANDLER_H

#include <ds/ref_cnt_ptr.h>
#include <sys/int/ioapic.h>
#include <sys/madt.h>

namespace Interrupt {
struct IntState {
    uint64_t eflags;
    uint64_t cs;
    uint64_t eip;
    uint64_t err;
} __attribute__((packed));

struct RegList {
    uint64_t    rax;
    uint64_t    rbx;
    uint64_t    rcx;
    uint64_t    rdx;
    uint64_t    rdi;
    uint64_t    rsi;
    uint64_t    rsp;
    uint64_t    rbp;
    uint64_t    r8;
    uint64_t    r9;
    uint64_t    r10;
    uint64_t    r11;
    uint64_t    r12;
    uint64_t    r13;
    uint64_t    r14;
    uint64_t    r15;
    uint64_t    rip;
} __attribute__((packed));

struct IntStackFrame {
    uint64_t ss;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t cs;
    uint64_t rip;
} __attribute__((packed));

struct IntStackFrame {
    
};

typedef void (*vec_handler_t)(uint64_t, const RegList*, const IntStackFrame*);

struct IdtEntry {
    uint16_t    handler_low;
    uint16_t    segment;
    uint8_t     ist;
    uint8_t     attributes;
    uint16_t    handler_mid;
    uint32_t    handler_high;
    uint32_t    reserved;
} __attribute__((packed));

struct IdtDescriptor {
    uint16_t bounds;
    uint64_t base;
} __attribute__((packed));

// SDT is 0x24 bytes, local APIC addr is 4 bytes, flags are 4 bytes, so
// actual records begin at offset 0x2C of MADT.
static constexpr uint8_t MADT_RECORDS_OFFSET = 0x2C;
static constexpr uint8_t MAX_ISA_IRQ = 16;
static constexpr uint32_t DEFAULT_LAPIC_ADDR = 0xFEE00000;

static ds::DynArray<uint8_t> lapic_ids_;
static ds::HashMap<uint32_t, ACPI::MADT::NMISourceRecord> gsi_to_nmi_;
static ds::HashMap<uint8_t, ds::DynArray<uint8_t>> lapic_nmis_;
static uintptr_t lapic_addr_;
static ds::HashMap<uint32_t, ACPI::MADT::ISORecord> gsi_to_iso_;
static ds::HashMap<uint8_t, uint32_t> irq_to_gsi_;
static ds::DynArray<ds::RefCntPtr<IOAPIC>> ioapics_;

static vec_handler_t VEC_HANDLERS[256];
static IdtDescriptor IDT_DESC;

static const volatile ACPI::MADT::MADTRecord *raw_madt_;

static void ParseMADT()
{
    uint32_t madt_len = raw_madt_->header.length_;
    uint8_t *madt_record_base = ((uint8_t*) raw_madt_ + MADT_RECORDS_OFFSET);
    uint8_t *record;
    uint8_t record_type, record_len;

    for(size_t i = 0; (MADT_RECORDS_OFFSET + i) < madt_len; i += record_len) {
        record = (madt_record_base + i);
        record_type = record[0];
        record_len = record[1];

        switch(record_type) {
            case ACPI::MADT::PROCESSOR_LOCAL_APIC: {
                ACPI::MADT::LAPICRecord lapic {};
                memcpy(&lapic, record, sizeof(ACPI::MADT::LAPICRecord));
                lapic_ids_.Append(lapic.acpi_processor_id);
            } break;
            case ACPI::MADT::IO_APIC: {
                ACPI::MADT::IOAPICRecord ioapic_rec {};
                memcpy(&ioapic_rec, record, sizeof(ACPI::MADT::IOAPICRecord));
                ioapics_.Append(ds::MakeRefCntPtr(new IOAPIC(ioapic_rec)));
            } break;
            case ACPI::MADT::INTERRUPT_SOURCE_OVERRIDE: {
                ACPI::MADT::ISORecord iso {};
                memcpy(&iso, record, sizeof(ACPI::MADT::ISORecord));
                gsi_to_iso_.Insert(iso.irq_source, iso);
                irq_to_gsi_.Insert(iso.irq_source, iso.gsi);
            } break;
            case ACPI::MADT::NON_MASKABLE_INTERRUPT_SOURCE: {
                ACPI::MADT::NMISourceRecord nmi_source {};
                memcpy(&nmi_source, record, sizeof(ACPI::MADT::NMISourceRecord));
                gsi_to_nmi_.Insert(nmi_source.gsi, nmi_source);
            } break;
            case ACPI::MADT::LOCAL_NON_MASKABLE_INTERRUPT: {
                ACPI::MADT::NMIRecord nmi {};
                memcpy(&nmi, record, sizeof(ACPI::MADT::NMIRecord));
                if(lapic_nmis_.Contains(nmi.acpi_processor_id)) {
                    lapic_nmis_[nmi.acpi_processor_id].Append(nmi.lint);
                } else {
                    lapic_nmis_.Insert(nmi.acpi_processor_id, { nmi.lint });
                }
            } break;
            case ACPI::MADT::LOCAL_APIC_ADDRESS_OVERRIDE: {
                ACPI::MADT::LAPICAddrRecord lapic_addr {};
                memcpy(&lapic_addr, record, sizeof(ACPI::MADT::LAPICAddrRecord));
                lapic_addr_ = lapic_addr.local_apic_addr;
            }
            default: break;
        }
    }
}

/**
 * Get the IOAPIC responsible for a given interrupt, identified by its GSI.
 * @param gsi Global System Interrupt (GSI) number.
 * @return The ID of the IOAPIC which handles the given GSI.
 */
static ds::Optional<ds::RefCntPtr<IOAPIC>> GSIToIOAPIC(uint32_t gsi)
{
    // Goal is to find the APIC record with the largest GSIB that is less
    // than the given GSI. max_gsib tracks the maximum GSIB found so far
    // that satisfies this condition, and apic_id gives the ID of the IOAPIC
    // it belongs to.
    for(int i = 0; i < ioapics_.Size(); ++i) {
        if(ioapics_[i]->ContainsGSI(gsi)) {
            return ioapics_[i];
        }
    }
    return ds::NullOpt;
}

static pin_polarity_t GetPinPolarity(uint16_t flags)
{
    return (flags & 2) ? ACTIVE_HIGH : ACTIVE_LOW;
}

static trigger_mode_t GetTriggerMode(uint16_t flags)
{
    return (flags & 8) ? LEVEL_SENSITIVE : EDGE_SENSITIVE;
}

static void InitializeInts()
{
    using namespace ACPI::MADT;
    for(int i = 0; i < ioapics_.Size(); ++i) {
        for(uint32_t gsi = ioapics_[i]->MinGSI(); gsi <= ioapics_[i]->MaxGSI();
            ++gsi)
        {
            // GSI 1. External interrupt from 8259 APIC; ignore.
            if(gsi == 1) {
                continue;
            }

            uint8_t gsi_vector;
            pin_polarity_t gsi_polarity;
            trigger_mode_t gsi_tmode;
            icr_delivery_t gsi_dmode;
            bool masked;

            // If an ISO exists, then the assumed GSI->IRQ identity mapping
            // is not valid; instead, an alternative IRQ is specified in
            // a MADT record, as well as flags suggesting configuration.
            if(ds::Optional<ISORecord> iso = gsi_to_iso_.Lookup(gsi)) {
                gsi_vector = iso->irq_source + 0x20;
                gsi_dmode = ICR_FIXED;
                gsi_polarity = GetPinPolarity(iso->flags);
                gsi_tmode = GetTriggerMode(iso->flags);
                masked = true;
            }

            // If a non-maskable interrupt (NMI) exists, map it to vector
            // 2 and do not mask it.
            else if(ds::Optional<NMISourceRecord> nmi = gsi_to_nmi_.Lookup(gsi)) {
                gsi_vector = 2;
                gsi_dmode = ICR_NMI;
                gsi_polarity = GetPinPolarity(nmi->flags);
                gsi_tmode = GetTriggerMode(nmi->flags);
                masked = false;
            }

            // GSIs [0, 16).
            else if(gsi < MAX_ISA_IRQ) {
                gsi_vector = gsi + 0x20;
                gsi_dmode = ICR_FIXED;
                gsi_polarity = ACTIVE_HIGH;
                gsi_tmode = EDGE_SENSITIVE;
                masked = true;
            }

            // GSIs [16, 256). PCI interrupts; must be level sensitive.
            else {
                gsi_vector = gsi + 0x20;
                gsi_dmode = ICR_FIXED;
                gsi_polarity = ACTIVE_HIGH;
                gsi_tmode = LEVEL_SENSITIVE;
                masked = true;
            }

            ioapics_[i]->RouteGSI(gsi, 0, gsi_vector, gsi_dmode, gsi_polarity,
                                  gsi_tmode, masked);
        }
    }
}

static uint32_t IRQToGSI(uint8_t irq)
{
    // irq_to_gsi_ hash map is populated based on ISO entries; IRQs without
    // ISO entries are assumed to be identity-mapped.
    if(ds::Optional<uint32_t> gsi_opt = irq_to_gsi_.Lookup(irq)) {
        return *gsi_opt;
    }
    return irq;
}

static bool WakeupProcessor(uint8_t acpi_proc_id);

static void LoadIDT()
{
    IDT_DESC.base = (uintptr_t) &VEC_HANDLERS;
    IDT_DESC.bounds = sizeof(IdtEntry) * 256 - 1;

    __asm__ __volatile__("lidt %0"
    : /* No outputs */
    : "r" (&IDT_DESC));
}

void Init(uintptr_t base_addr)
{
    // Initialize global vars, since we don't have a C++ runtime to do so for us
    new (&lapic_ids_)   ds::DynArray<uint8_t>();
    new (&ioapics_)     ds::DynArray<ds::RefCntPtr<IOAPIC>>();
    new (&gsi_to_nmi_)  ds::HashMap<uint32_t, ACPI::MADT::NMISourceRecord>();
    new (&lapic_nmis_)  ds::HashMap<uint8_t, ds::DynArray<uint8_t>>();
    new (&gsi_to_iso_)  ds::HashMap<uint32_t, ACPI::MADT::ISORecord>();
    new (&irq_to_gsi_)  ds::HashMap<uint8_t, uint32_t>();
    lapic_addr_      =  DEFAULT_LAPIC_ADDR;
    raw_madt_        =  (ACPI::MADT::MADTRecord*) base_addr;

    ParseMADT();
    InitializeInts();
}


void RegisterISR(uint8_t irq, vec_handler_t isr)
{
    VEC_HANDLERS[irq + 0x20] = isr;
}

void MaskIRQ(uint8_t irq)
{

}

void UnmaskIRQ(uint8_t irq)
{

}


extern "C" {
    void VecHandler(uint64_t int_no, const RegList *regs)
    {
        if(VEC_HANDLERS[int_no]) {
            (*VEC_HANDLERS[int_no])(int_no, regs);
        }
    }

    void SendEOI() {

    }
}
}

#endif

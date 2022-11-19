#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>
#include <sys/madt.h>
#include <ds/dyn_array.h>
#include <ds/hash_map.h>

namespace Interrupt
{
/** ICR Delivery modes.
 * Some overlap between this and LVT delivery modes.
 * Difference between INIT_DELIVERY and INIT_LEVEL_DEASSERT is that the latter
 * requires that level flag be set to 0.
**/
enum icr_delivery_t {
    ICR_FIXED                    =     0b000,
    ICR_LOW                      =     0b001,
    ICR_SMI                      =     0b010,
    ICR_NMI                      =     0b100,
    ICR_INIT                     =     0b101,
    ICR_STARTUP                  =     0b100
};

enum pin_polarity_t {
    ACTIVE_HIGH = 0,
    ACTIVE_LOW  = 1
};

enum trigger_mode_t {
    EDGE_SENSITIVE  = 0,
    LEVEL_SENSITIVE = 1
};

class IOAPIC
{
    /** Redirection table entry (inside I/O APIC).
     * This needs the -features=extensions option to compile. **/
    union ioredtbl_t {
        struct {
            uint8_t  vector;
            unsigned delivery_mode          : 3;
            unsigned destination_mode       : 1;
            unsigned delivery_status        : 1;
            unsigned polarity               : 1;
            unsigned remote_irr_pending     : 1;
            unsigned trigger_mode           : 1;
            unsigned mask                   : 1;
            uint64_t reserved               : 39;
            uint8_t  destination;
        } __attribute__((packed));

        struct {
            uint32_t lower_dword;
            uint32_t upper_dword;
        } __attribute__((packed));
    };

    // APIC IDs are 4 bits, so 2^4 possible APICs. I believe this can go higher
    // with X2APIC, so revise if you ever support that.
    static constexpr uint8_t  MAX_IOAPICS      = 16;
    static constexpr uint8_t  ID_SHIFT         = 24;
    static constexpr uint32_t ID_MASK          = 0b1111 << ID_SHIFT;
    static constexpr uint8_t  VERSION_MASK     = 0xFF;
    static constexpr uint8_t  MAX_RED_SHIFT    = 16;
    static constexpr uint32_t MAX_RED_MASK     = 0xFF << MAX_RED_SHIFT;
    static constexpr uint8_t  MAX_ISA_IRQ      = 16;

public:

    enum dest_mode_t {
        PHYSICAL = 0,
        LOGICAL = 1
    };

    enum ioapic_reg_t {
        IOAPIC_ID  = 0,
        IOAPIC_VER = 1,
        IOAPIC_ARB = 2,
        IOREDTBL   = 3
    };

    IOAPIC(const ACPI::MADT::IOAPICRecord &record)
        : ioregsel_((uint32_t*) KHeap::ToVAddr(record.io_apic_addr))
        , iowin_((uint32_t*) (KHeap::ToVAddr(record.io_apic_addr) + 0x10))
        , min_gsi_(record.gsi_base)
        , apic_id_((Read(IOAPIC_ID) & ID_MASK) >> ID_SHIFT)
        , num_pins_(((Read(IOAPIC_VER) & MAX_RED_MASK) >> MAX_RED_SHIFT) + 1)
        , has_eoi_((Read(IOAPIC_VER) & MAX_RED_SHIFT) >= 0x20)
    {}

    bool ContainsGSI(uint32_t gsi) const
    {
        return gsi > min_gsi_ && gsi < min_gsi_ + num_pins_;
    }

    uint32_t MinGSI() const
    {
        return min_gsi_;
    }

    uint32_t MaxGSI() const
    {
        return min_gsi_ + num_pins_ - 1;
    }

    bool RegisterSMI(uint32_t gsi)
    {
        if(ContainsGSI(gsi)) {
            return false;
        }

        // MP spec mandates that vector of SMI be set to 0.
        ioredtbl_t entry {
            .vector = 0,
            .delivery_mode = ICR_SMI,
            .polarity = ACTIVE_HIGH,
            .trigger_mode = EDGE_SENSITIVE,
            .mask = 0
        };
        WriteEntry(gsi, entry);
        return true;
    }

    bool SetGSIMask(uint32_t gsi, bool masked)
    {
        if(ContainsGSI(gsi)) {
            return false;
        }

        ioredtbl_t current_entry = ReadEntry(gsi);
        if(current_entry.mask != masked) {
            current_entry.mask = masked;
            WriteEntry(gsi, current_entry);
        }
        return true;
    }

    bool RouteGSI(uint32_t gsi, uint8_t lapic_id, uint8_t vector,
                  icr_delivery_t delivery=ICR_FIXED,
                  pin_polarity_t polarity=ACTIVE_HIGH,
                  trigger_mode_t trigger_mode=EDGE_SENSITIVE,
                  bool masked=true)
    {
        if(ContainsGSI(gsi)) {
            return false;
        }

        ioredtbl_t entry = ReadEntry(gsi);
        entry.vector = vector;
        entry.polarity = polarity;
        entry.delivery_mode = delivery;
        entry.trigger_mode = trigger_mode;
        entry.destination = lapic_id;
        entry.mask = masked;
        entry.destination_mode = PHYSICAL;
        WriteEntry(gsi, entry);

        return true;
    }


private:
    volatile uint32_t *ioregsel_;
    volatile uint32_t *iowin_;
    uint32_t min_gsi_;
    uint8_t num_pins_;
    uint8_t apic_id_;
    bool has_eoi_;

    uint32_t Read(uint8_t offset)
    {
        *(ioregsel_) = offset;
        return *(iowin_);
    }

    void Write(uint8_t offset, uint32_t val)
    {
        *(ioregsel_) = offset;
        *(iowin_) = val;
    }

    ioredtbl_t ReadEntry(uint32_t gsi)
    {
        ioredtbl_t entry {};
        uint32_t pin = gsi - min_gsi_;
        entry.lower_dword = Read(0x10 + pin * 2);
        entry.upper_dword = Read(0x11 + pin * 2);
        return entry;
    }

    void WriteEntry(uint32_t gsi, ioredtbl_t entry)
    {
        uint32_t pin = gsi - min_gsi_;
        Write(0x10 + pin * 2, entry.lower_dword);
        Write(0x11 + pin * 2, entry.upper_dword);
    }
};
}

#endif

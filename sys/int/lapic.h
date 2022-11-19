#ifndef LAPIC_H
#define LAPIC_H

#include <ds/hash_map.h>

namespace Interrupt
{
class LAPIC
{
    // Class-specific types.
    enum lapic_reg_t
    {
        LAPIC_ID_REG            = 0x020,
        LAPIC_VERSION_REG       = 0x030,
        LAPIC_EOI_REG           = 0x0B0,
        LAPIC_SPURIOUS_INT_REG  = 0x0F0,
        LAPIC_ICR0_REG          = 0x300,
        LAPIC_ICR1_REG          = 0x310,
        LAPIC_TIMER_REG         = 0x320,
        LAPIC_THERMAL_REG       = 0x330,
        LAPIC_PERFORMANCE_REG   = 0x340,
        LAPIC_LINT0_REG         = 0x350,
        LAPIC_LINT1_REG         = 0x360,
        LAPIC_ERROR_REG         = 0x370,
        LAPIC_INIT_COUNT_REG    = 0x380,
        LAPIC_CURRENT_COUNT_REG = 0x390,
        LAPIC_DIVIDE_CONFIG_REG = 0x3E0
    };

    enum timer_mode_t
    {
        ONE_SHOT     = 0b00,
        PERIODIC     = 0b01,
        TSC_DEADLINE = 0b10
    };

    /** Local Vector Table (LVT) entry. **/
    union lvt_entry_t
    {
        struct
        {
            uint8_t  vector;
            unsigned delivery_mode: 3;
            unsigned reserved0: 1;
            unsigned delivery_status: 1;
            unsigned pin_polarity: 1;
            unsigned remote_irr_flag: 1;
            unsigned trigger_mode: 1;
            unsigned mask: 1;
            unsigned timer_mode: 2;
            unsigned reserved1: 12;
        } __attribute__((packed));

        uint32_t dword;
    };

    union interproc_int_t
    {
        struct
        {
            uint8_t  vector;
            unsigned delivery_mode: 3;
            unsigned delivery_status: 1;
            unsigned reserved0: 1;
            unsigned level: 1;
            unsigned trigger_mode: 1;
            unsigned reserved1: 2;
            unsigned destination_shorthand: 2;
            uint64_t reserved2: 36;
            uint8_t  destination_field;
        };

        struct
        {
            uint32_t lower_dword;
            uint32_t upper_dword;
        };
    } __attribute__((packed));

    // Static members.
    static constexpr uint8_t IO_APIC_IRQ_BASE = 0x10;
    static constexpr uint8_t ALL_LAPICS       = 0xFF;
    static constexpr uint8_t UNSET            = 0;
    static constexpr uint8_t NMI_INT_VECTOR   = 0xFF;

public:
    void Enable();

    void Disable();

    uint8_t GetId() const;

    uint8_t GetVersion() const;

    void SetEoiBroadcast(bool broadcast);

    bool EoiIsBroadcast() const;

    void TimerInit(uint8_t vector);

private:
    uintptr_t lapic_base_;
    uint8_t   id_, version_;
    bool      eoi_is_broadcast_;

    void Write(lapic_reg_t reg, uint32_t val);
    uint32_t Read(lapic_reg_t reg);
};
}

#endif

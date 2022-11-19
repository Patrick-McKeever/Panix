#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <sys/int/lapic.h>
#include <sys/int/ioapic.h>
#include <sys/madt.h>

namespace Interrupt
{
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

typedef void (*isr_t)(const RegList*);

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

class IntHandler
{
public:
    IntHandler(uintptr_t madt_base)
        : madt_(madt_base)
    {
        for(madt_.)
    }

    void Init();

    void SetISR(uint8_t vector, isr_t isr, uint8_t flags);

    void MaskVector(uint8_t vector);

    void UnmaskVector(uint8_t vector);

    bool RouteIRQToBSP(uint8_t irq, uint8_t vector, bool masked);

    bool RouteIRQ(uint8_t irq, uint8_t lapic_id, uint8_t vector, bool masked);

private:
    ACPI::MADT madt_;

    IdtEntry idt_[256];
    IdtDescriptor IDT_DESC_;
    uint8_t vector_to_irq_[256];

    ds::HashMap<uint8_t, LAPIC> LAPICs_;
    ds::DynArray<uint8_t, IOAPIC> IOAPICs_;
};
}


#endif

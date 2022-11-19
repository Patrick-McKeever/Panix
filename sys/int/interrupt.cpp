#include "interrupt.h"
#include <sys/madt.h>

namespace Interrupt {
namespace {
ACPI::MADT madt_;

IdtEntry idt_[256];
IdtDescriptor IDT_DESC_;

uint8_t vector_to_irq_[256];
uint32_t irq_to_gsi_[256];

ds::DynArray<LAPIC> LAPICs_;
ds::DynArray<IOAPIC> IOAPICs_;
}


}
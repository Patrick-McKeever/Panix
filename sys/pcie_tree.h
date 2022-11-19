#ifndef PCI_H
#define PCI_H

#include <ds/optional.h>
#include <ds/hash_map.h>
#include <ds/dyn_array.h>
#include <sys/acpi.h>

struct PCIeDevice {
    uint8_t bus_no_;
    uint8_t dev_no_;
    uint8_t func_no_;

    struct PCIHeader {
        uint16_t vend_id_;
        uint16_t dev_id_;
        uint16_t command_;
        uint16_t status_;
        uint8_t revision_id_;
        uint8_t prog_if_;
        uint8_t subclass_;
        uint8_t class_;
        uint8_t cache_line_size_;
        uint8_t lat_timer_;
        uint8_t header_type_;
        uint8_t bist_;
    } __attribute__((packed)) *header_;

    ds::DynArray<PCIeDevice> devices_;
};

class PCIeTree {
public:
    PCIeTree(void *mcfg_ptr);

    // All members are primitive or RAII, so no need for copy constructor /
    // copy assignment / destructor.

    ds::Optional<ds::DynArray<struct PCIeDevice>>
    GetDevicesByClass(uint8_t class_no);

    ds::DynArray<PCIeDevice> GetDevicesBySubclass(uint8_t class_no,
                                                   uint8_t subclass_no);

private:
    bool initialized_;
    static constexpr uint16_t MAX_BUSES	= 256;
    static constexpr uint16_t MAX_DEVS	= 32;
    static constexpr uint16_t MAX_FUNCS	= 8;

    ds::DynArray<PCIeDevice> buses_;
    ds::HashMap<uint16_t, ds::DynArray<PCIeDevice>> devices_by_class_;


    ds::DynArray<PCIeDevice>
    EnumerateDevices(uint8_t bus_no, uintptr_t mmio_base);

    ds::DynArray<PCIeDevice>
    EnumerateFunctions(uint8_t bus_no, uint8_t dev_no, uintptr_t mmio_base);
};

#endif

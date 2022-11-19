#include "pcie_tree.h"

PCIeTree::PCIeTree(void *mcfg_ptr)
    : initialized_(false)
{
    auto *mcfg = (ACPI::MCFG*) mcfg_ptr;
    if (! mcfg || ! ACPI::ValidateSDT(mcfg, mcfg->header_.length_)) {
        Log("Not validated\n");
        return;
    }

    size_t num_spaces = (mcfg->header_.length_ - sizeof(ACPI::SDTHeader)) /
                        sizeof(ACPI::MCFG::ConfigSpace);

    for (size_t i = 0; i < num_spaces; ++i) {
        ACPI::MCFG::ConfigSpace seg = mcfg->config_spaces_[i];
        uintptr_t mmio_base = seg.base_addr_;

        for(uint8_t j = seg.start_bus_; j < seg.end_bus_; ++j) {
            uintptr_t header_addr = ((j << 20) + mmio_base);
            PCIeDevice bus = {
                .bus_no_ = j,
                .dev_no_ = 0,
                .func_no_ = 0,
                .header_ = (PCIeDevice::PCIHeader*) header_addr,
                .devices_ = ds::DynArray<PCIeDevice>()
            };

            if(bus.header_->dev_id_ != 0 && bus.header_->dev_id_ != 0xFFFF) {
                // Log("Enumerating bus %d\n", j);
                bus.devices_ = EnumerateDevices(j, mmio_base);
                buses_.Append(bus);
            }
        }
    }

    initialized_ = true;
}

ds::Optional<ds::DynArray<PCIeDevice>>
PCIeTree::GetDevicesByClass(uint8_t class_no)
{
    if(initialized_) {
        return devices_by_class_.Lookup(class_no);
    }
    return ds::NullOpt;
}


ds::DynArray<PCIeDevice>
PCIeTree::GetDevicesBySubclass(uint8_t class_no, uint8_t subclass_no)
{
    using dev_arr_t = ds::Optional<ds::DynArray<PCIeDevice>>;
    ds::DynArray<PCIeDevice> matching_devs;
    if(initialized_) {
        if(dev_arr_t devs = devices_by_class_.Lookup(class_no)) {
            for(int i = 0; i < devs->Size(); ++i) {
                PCIeDevice dev = devs->Lookup(i);
                if(dev.header_->subclass_ == subclass_no) {
                    matching_devs.Append(dev);
                }
            }
        }
    }
    return matching_devs;
}

ds::DynArray<PCIeDevice>
PCIeTree::EnumerateDevices(uint8_t bus_no, uintptr_t mmio_base)
{
    // Log("\tEnumerating devices on bus %d\n", bus_no);
    ds::DynArray<PCIeDevice> devices;

    for(uint8_t i = 0; i < MAX_DEVS; ++i) {
        uintptr_t header_addr = ((bus_no << 20) + (i << 15) + mmio_base);
        PCIeDevice device_func = {
            .bus_no_ = bus_no,
            .dev_no_ = i,
            .func_no_ = 0,
            .header_ = (PCIeDevice::PCIHeader*) header_addr,
            .devices_ = ds::DynArray<PCIeDevice>()
        };

        if(device_func.header_->dev_id_ != 0 &&
           device_func.header_->dev_id_ != 0xFFFF)
        {
            device_func.devices_ = EnumerateFunctions(bus_no, i, mmio_base);
            devices.Append(device_func);
        }
    }

    return devices;
}

ds::DynArray<PCIeDevice>
PCIeTree::EnumerateFunctions(uint8_t bus_no, uint8_t dev_no,
                             uintptr_t mmio_base)
{
    //Log("\t\tEnumerating functions on device %d\n", dev_no);
    ds::DynArray<PCIeDevice> devices;

    for(uint8_t i = 0; i < MAX_FUNCS; ++i) {
        uintptr_t header_addr = ((bus_no << 20) + (dev_no << 15) + (i << 12)
                                 + mmio_base);

        PCIeDevice device_func = {
            .bus_no_ = bus_no,
            .dev_no_ = dev_no,
            .func_no_ = i,
            .header_ = (PCIeDevice::PCIHeader*) header_addr,
            .devices_ = ds::DynArray<PCIeDevice>()
        };

        if(device_func.header_->dev_id_ != 0 &&
            device_func.header_->dev_id_ != 0xFFFF)
        {
            //Log("\t\t\tFunction %d, class %d, subbclass %d, prog_if %d,"
            //    "header type %d\n",
            //    i, device_func.header_->class_, device_func.header_->subclass_,
            //    device_func.header_->prog_if_,
            //    device_func.header_->header_type_ & 0b1111111);
            devices.Append(device_func);

            uint16_t dev_class = device_func.header_->class_;
            if(devices_by_class_.Contains(device_func.header_->class_)) {
                devices_by_class_[dev_class].Append(device_func);
            } else {
                ds::DynArray<PCIeDevice> funcs({ device_func });
                //ds::DynArray<PCIeDevice> funcs(1);
                //funcs.Append(device_func);
                devices_by_class_.Insert(dev_class, funcs);
                //devices_by_class_[dev_class] = funcs;
            }
        }
    }

    return devices;
}

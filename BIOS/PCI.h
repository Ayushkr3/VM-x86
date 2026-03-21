#pragma once
#include "Global.h"
#include <vector>
#include "IDE_controller.h"
#include "PCI_def.h"
// ---------------------------------------------------------------------------
// HostBridge  (8086:1237)
// ---------------------------------------------------------------------------
extern bool tester;
class HostBridge : public PCIDevice {
public:
    HostBridge() {
        *(uint16_t*)&config[0x00] = 0x8086;
        *(uint16_t*)&config[0x02] = 0x1237;
        config[0x0B] = 0x06;   // class: bridge
        config[0x0A] = 0x00;   // subclass: host
        config[0x0E] = 0x00;   // header type
        for (int i = 0; i < 6; i++)
            *(uint32_t*)&config[0x10 + i * 4] = 0;
        *(uint32_t*)&config[0x30] = 0;
    }
    void config_write(uint32_t offset, uint32_t value);
};

// ---------------------------------------------------------------------------
// ISABridge  (8086:7000)
// ---------------------------------------------------------------------------
class ISABridge : public PCIDevice {
public:
    ISABridge() {
        *(uint16_t*)&config[0x00] = 0x8086;
        *(uint16_t*)&config[0x02] = 0x7000;
        config[0x0B] = 0x06;   // class: bridge
        config[0x0A] = 0x01;   // subclass: ISA
        config[0x0E] = 0x00;
        for (int i = 0; i < 6; i++)
            *(uint32_t*)&config[0x10 + i * 4] = 0;
        *(uint32_t*)&config[0x30] = 0;
    }
    void config_write(uint32_t offset, uint32_t value);
};

// ---------------------------------------------------------------------------
// PCISystemBus
// ---------------------------------------------------------------------------
class PCISystemBus {
private:
    std::vector<PCIDevice*> attachedDevice;
public:
    HostBridge    HB;
    ISABridge     SB;
    IDEController* ID=nullptr;

    uint32_t index = 0;

    PCISystemBus();
    ~PCISystemBus();
    void     out_hook(uint32_t port, uint32_t value, int size);
    uint32_t in_hook(uint32_t port, int size);
    void AttachDevice(PCIDevice* dev) {
        attachedDevice.push_back(dev);
    }
};

//void InitPCI(RaiseIRQ rfwd);
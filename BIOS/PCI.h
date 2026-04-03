#pragma once
#include "Global.h"
#include <vector>
#include "IDE_controller.h"
#include "vga.h"
#include "PCI_def.h"
// ---------------------------------------------------------------------------
// HostBridge  (8086:1237)
// ---------------------------------------------------------------------------
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
class VGAController :public PCIDevice {
    bool rom_bar_sizing_probe = false;
    bool svga_bar_sizing_probe = false;
public:
    VGACommonState* vga;
    VGAController() {
        memset(config, 0, sizeof(config));

        *(uint16_t*)&config[0x00] = 0x1234;
        *(uint16_t*)&config[0x02] = 0x1111;
        *(uint16_t*)&config[0x04] = 0x0003;
        config[0x09] = 0x00;
        config[0x0A] = 0x00;
        config[0x0B] = 0x03;
        config[0x0E] = 0x00;
        *(uint16_t*)&config[0x2C] = 0x1AF4;
        *(uint16_t*)&config[0x2E] = 0x1100;
        config[0x3C] = 0xFF;
        config[0x3D] = 0x00;
        *(uint32_t*)&config[0x30] = VGABIOS_BASE & 0xFFFFF800;
        rom_bar_size = VGABIOS_SIZE; // 64KB

        set_bar(0, 0xE0000000 | 0x08, 0x1000000);
        vga= new VGACommonState;
        vga_common_init(vga);

    }
    void config_write(uint32_t offset, uint32_t value);
    uint32_t config_read(uint32_t offset);
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
    VGAController* vgaC = nullptr;

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
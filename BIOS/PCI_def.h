#pragma once
#include "PIC.h"
#include <cstdint>
#include <memory>
class PCIDevice {
protected:
    uint8_t config[256] = { };
    uint32_t bar_size[6] = {};      // size of each BAR
    uint32_t bar_default[6] = {};   // original BAR address
    uint32_t rom_bar_size = 0;   // original BAR address
public:
    PCIDevice() {
        memset(config, 0xff, sizeof(config));
        memset(bar_size, 0x0, sizeof(uint32_t)*6);
        memset(bar_default, 0x0, sizeof(uint32_t)*6);
    }

    void set_bar(int index, uint32_t address, uint32_t size) {
        bar_default[index] = address;
        bar_size[index] = size;
        *(uint32_t*)&config[0x10 + index * 4] = address;
    }

    virtual uint32_t config_read(uint32_t offset) {
        if (offset >= 0x10 && offset <= 0x24) {
            int bar = (offset - 0x10) / 4;
            uint32_t val = *(uint32_t*)&config[offset];
            if (bar_size[bar] == 0)
                return 0x00000000;
            if (val == 0xFFFFFFFF)
                return (~(bar_size[bar] - 1)) | (bar_default[bar] & 0xF);
        }
        // ROM BAR - return 0 if not implemented
        if (offset == 0x30) {
            uint32_t val = *(uint32_t*)&config[0x30];
            if ((val & 0xFFFFF800) == 0xFFFFFF00) { // sizing probe
                if (rom_bar_size == 0) return 0;
                return (~(rom_bar_size - 1)) | 0x1; // size mask + ROM indicator
            }
            return val;
        }
        return *(uint32_t*)&config[offset];
    }

    virtual void config_write(uint32_t offset, uint32_t value) {
        *(uint32_t*)&config[offset] = value;
    }
};
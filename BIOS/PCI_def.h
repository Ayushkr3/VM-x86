#pragma once
#include "PIC.h"
#include <cstdint>
#include <memory>
class PCIDevice {
protected:
    uint8_t config[256] = { 0x0 };
public:
    PCIDevice() {
        memset(config, 0xFF, sizeof(config));
    }

    virtual uint32_t config_read(uint32_t offset) {
        return *(uint32_t*)&config[offset];
    }

    virtual void config_write(uint32_t offset, uint32_t value) {
        *(uint32_t*)&config[offset] = value;
    }
};


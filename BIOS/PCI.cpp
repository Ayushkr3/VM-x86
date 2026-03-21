
#include "PCI.h"
#include <cstdio>
#include <cstring>
#include <queue>
bool tester = false;
// ============================================================================
// MINIMAL SETUP - Just constructor
// ============================================================================
PCISystemBus::PCISystemBus() : index(0) {
    attachedDevice.push_back(&HB);
    attachedDevice.push_back(&SB);
    // Pre-init secondary with CDROM signature
    
}
void HostBridge::config_write(uint32_t offset, uint32_t value) {
    if ((offset >= 0x10 && offset <= 0x24) || offset == 0x30) return;
    PCIDevice::config_write(offset, value);
}

void ISABridge::config_write(uint32_t offset, uint32_t value) {
    if ((offset >= 0x10 && offset <= 0x24) || offset == 0x30) return;
    PCIDevice::config_write(offset, value);
}

// ============================================================================
// OUT HOOK - Handle all writes
// ============================================================================
void PCISystemBus::out_hook(uint32_t port, uint32_t value, int size) {
    // PCI CONFIG
    if (port == 0xCF8) {
        index = value;
        return;
    }
    if (port >= 0xCFC && port <= 0xCFF) {
        if (!(index & 0x80000000)) return;
        uint8_t bus = (index >> 16) & 0xFF;
        uint8_t dev = (index >> 11) & 0x1F;
        uint32_t reg = index & 0xFC;
        if (bus != 0 || dev >= (uint8_t)attachedDevice.size()) return;
        uint32_t byteOff = port - 0xCFC;
        uint32_t old = attachedDevice[dev]->config_read(reg);
        uint32_t mask = (size == 1) ? (0xFFu << (byteOff * 8))
            : (size == 2) ? (0xFFFFu << (byteOff * 8)) : 0xFFFFFFFFu;
        uint32_t merged = (old & ~mask) | ((value << (byteOff * 8)) & mask);
        attachedDevice[dev]->config_write(reg, merged);
        return;
    }
    uint16_t offset = 0xffff;
    IDEChannel* channel = nullptr;
    IDEInterface* dev = nullptr;
    if (port >= 0x1F0 && port <= 0x1F7) {
        if (!ID->primary)return;
        offset = port - 0x1F0;
        channel = ID->primary;
        dev = channel->current_interface;
        if (dev)goto IDE_COMMAND;
    }
    if (port >= 0x170 && port <= 0x177) {
        if (!ID->secondary)return;
        offset = port - 0x170;
        channel = ID->secondary;
        dev = channel->current_interface;
        if (dev)goto IDE_COMMAND;
    }
    if (port == 0x3F6) {
        if (!ID->primary)return;
        channel = ID->primary;
        channel->writeControl(value);
    }
    if (port == 0x376) {
        if (!ID->secondary)return;
        channel = ID->secondary;
        channel->writeControl(value);
    }

    return;
IDE_COMMAND:
        switch (offset) {
        case 0x00: dev->writeDataPort(value,size);        break;  // Data
        case 0x01: dev->features_reg = (dev->drive_connected)?value:0;             break;  // Features
        case 0x02: dev->sector_count_reg = (dev->drive_connected) ? value : 0;     break;  // Sector count
        case 0x03: dev->lba_low_reg = (dev->drive_connected) ? value : 0;          break;  // LBA Low
        case 0x04: dev->lba_mid_reg = (dev->drive_connected) ? value : 0;          break;  // LBA Mid
        case 0x05: dev->lba_high_reg = (dev->drive_connected) ? value : 0;         break;  // LBA High
        case 0x06: {
            bool select_slave = (value & 0x10);
            if ((select_slave && channel->current_interface == channel->master)
                || (!select_slave && channel->current_interface == channel->slave)) {
                if (select_slave) {
                    channel->current_interface = channel->slave;
                }
                else {
                    channel->current_interface = channel->master;
                }
            }
            channel->current_interface->device_reg = value;
            channel->current_interface->is_lba = value >> 6 & 1;
            channel->current_interface->head = value & 0xf;
            //dev = channel->current_interface;
        }
            break;
        case 0x07:
            channel->current_interface->status_reg &= ~(ATA_SR_ERR | ATA_SR_DF);
            channel->current_interface->ataCommand(value);
            break;
        }
        return;
}
uint32_t PCISystemBus::in_hook(uint32_t port, int size) {
    // PCI CONFIG
    if (port == 0xCF8) return index;
    if (port >= 0xCFC && port <= 0xCFF) {
        if (!(index & 0x80000000)) return 0xFFFFFFFF;
        uint8_t bus = (index >> 16) & 0xFF;
        uint8_t dev = (index >> 11) & 0x1F;
        uint8_t func = (index >> 8) & 0x07;
        uint32_t reg = index & 0xFC;
        if (bus != 0 || dev >= (uint8_t)attachedDevice.size() || func != 0) return 0xFFFFFFFF;
        uint32_t val = attachedDevice[dev]->config_read(reg);
        uint32_t byteOff = port - 0xCFC;
        if (size == 1) return (val >> (byteOff * 8)) & 0xFF;
        if (size == 2) return (val >> (byteOff * 8)) & 0xFFFF;
        return val;
    }
    uint8_t offset = 0xffff;
    IDEInterface* dev;

    if (port >= 0x1F0 && port <= 0x1F7) {
        if (!ID->primary)return 0;
        offset = port - 0x1F0;
        dev = ID->primary->current_interface;
        if (dev)goto IDE_COMMAND;
    }
    if (port == 0x3F6) {
        if (!ID->primary)return 0;
        return ID->primary->readStatus();
    }
    if (port == 0x376) {
        if (!ID->secondary)return 0;
        return ID->secondary->readStatus();
    }
    if (port >= 0x170 && port <= 0x177) {
        if (!ID->secondary)return 0;
        offset = port - 0x170;
        dev = ID->secondary->current_interface;
        if (dev)goto IDE_COMMAND;
    }
    return 0xFF;

IDE_COMMAND:
    switch (offset) {
    case 0x00: return dev->readData(size);           // Data
    case 0x01: return dev->error_reg;
    case 0x02: return dev->sector_count_reg;
    case 0x03: return dev->lba_low_reg;
    case 0x04: return dev->lba_mid_reg;
    case 0x05: return dev->lba_high_reg;          // LBA Hih
    case 0x06: return dev->device_reg;            // Device
    case 0x07: {
        uint64_t ret  = (dev->drive_connected) ? dev->status_reg : 0;
        return ret;
    }
    }
    return 0xFF;
}

PCISystemBus::~PCISystemBus() {
    if (ID) {
        delete ID;
    }
}
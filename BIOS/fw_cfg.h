#pragma once
#include <vector>
#include <string>
#include "memory.h"
#define FW_CFG_SIGNATURE    0x0000  // returns "QEMU"
#define FW_CFG_ID           0x0001
#define FW_CFG_UUID         0x0002
#define FW_CFG_RAM_SIZE     0x0003
#define FW_CFG_NOGRAPHIC    0x0004
#define FW_CFG_NB_CPUS      0x0005
#define FW_CFG_MACHINE_ID   0x0006
#define FW_CFG_BOOT_DEVICE  0x000D
#define FW_CFG_FILE_DIR     0x0019  // file directory
// file entries start at:
#define FW_CFG_FILE_FIRST   0x0020
struct FWCfgFile {
    std::string name;
    std::vector<uint8_t> data;
};

struct FWCfg {
    uint16_t selector = 0;
    uint32_t data_offset = 0;

    // fixed entries
    std::vector<uint8_t> entries[0x20];

    // file entries (for named files)
    std::vector<FWCfgFile> files;
    void buildE820(uint64_t ram_size) {
        // E820 entry: start(8) + size(8) + type(4) = 20 bytes
        struct E820Entry {
            uint64_t start;
            uint64_t size;
            uint32_t type;
        };

        std::vector<E820Entry> map;

        // always fixed low entries
        map.push_back({ 0x00000000, 0x0009F000, 1 }); // RAM 0-640KB
        map.push_back({ 0x0009F000, 0x00001000, 2 }); // RESERVED EBDA
        map.push_back({ 0x000F0000, 0x00010000, 2 }); // RESERVED BIOS ROM
        // main RAM above 1MB up to ram_size
        // but skip 0xA0000-0xF0000 (VGA/ISA hole) already excluded above
        if (ram_size > 0x00100000) {
            uint64_t high_size = ram_size - 0x00100000;

            // if ram crosses the PCI hole at 0xE0000000 (3.5GB) split it
            if (ram_size <= 0xE0000000ULL) {
                map.push_back({ 0x00100000, high_size, 1 });
            }
            else {
                // below PCI hole
                map.push_back({ 0x00100000, 0xE0000000ULL - 0x00100000, 1 });
                // above 4GB
                uint64_t above4g = ram_size - 0xE0000000ULL;
                map.push_back({ 0x100000000ULL, above4g, 1 });
            }
        }

        // RESERVED BIOS flash at top of 4GB
        //map.push_back({ 0xFFFC0000, 0x00040000, 2 });

        // serialize
        std::vector<uint8_t> data;
        for (auto& e : map) {
            uint8_t* p = (uint8_t*)&e;
            data.insert(data.end(), p, p + 20); // 20 bytes per entry
        }
        addFile("etc/e820", data);
    }
    void init(uint64_t ram_size) {
        // FW_CFG_SIGNATURE = "QEMU"
        entries[0x0000] = { 'Q','E','M','U' };

        // FW_CFG_ID = 1
        entries[0x0001] = { 0x01, 0x00, 0x00, 0x00 };

        // FW_CFG_RAM_SIZE
        entries[0x0003] = {
            (uint8_t)(ram_size),
            (uint8_t)(ram_size >> 8),
            (uint8_t)(ram_size >> 16),
            (uint8_t)(ram_size >> 24)
        };

        // FW_CFG_NB_CPUS = 1
        entries[0x0005] = { 0x01, 0x00 };

        // FW_CFG_BOOT_DEVICE = 'd'
        entries[0x000D] = { 'd' };

        // auto build e820 from ram_size
        buildE820(ram_size);

        // bootorder
        //std::string bootorder =
        //    "/pci@i0cf8/*@2/drive@1/disk@0\n"   // CD-ROM (ata1-0)
        //    "/pci@i0cf8/*@2/drive@0/disk@0\n";  // HD (ata0-0)
        //addFile("bootorder",
        //    std::vector<uint8_t>(bootorder.begin(), bootorder.end()));

        buildFileDir();
    }

    void addFile(const std::string& name,
        const std::vector<uint8_t>& data) {
        files.push_back({ name, data });
        buildFileDir();
    }

    void buildFileDir() {
        std::vector<uint8_t> dir;
        uint32_t count = files.size();
        // big endian count
        dir.push_back((count >> 24) & 0xFF);
        dir.push_back((count >> 16) & 0xFF);
        dir.push_back((count >> 8) & 0xFF);
        dir.push_back((count) & 0xFF);

        for (int i = 0; i < files.size(); i++) {
            uint32_t size = files[i].data.size();
            uint16_t sel = 0x0020 + i;
            // size big endian
            dir.push_back((size >> 24) & 0xFF);
            dir.push_back((size >> 16) & 0xFF);
            dir.push_back((size >> 8) & 0xFF);
            dir.push_back((size) & 0xFF);
            // select big endian
            dir.push_back((sel >> 8) & 0xFF);
            dir.push_back((sel) & 0xFF);
            // reserved
            dir.push_back(0);
            dir.push_back(0);
            // name (56 bytes, null padded)
            char name[56] = {};
            strncpy_s(name, files[i].name.c_str(), 55);
            dir.insert(dir.end(), name, name + 56);
        }
        entries[0x0019] = dir;
    }

    std::vector<uint8_t>* getCurrentEntry() {
        if (selector >= 0x0020) {
            int idx = selector - 0x0020;
            if (idx < files.size())
                return &files[idx].data;
            return nullptr;
        }
        if (selector < 0x0020)
            return &entries[selector];
        return nullptr;
    }

    uint8_t read() {
        auto* entry = getCurrentEntry();
        if (!entry || data_offset >= entry->size())
            return 0;
        return (*entry)[data_offset++];
    }
};
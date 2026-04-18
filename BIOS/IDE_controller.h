#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <set>
#include <functional>
#include <cstring>
#include <algorithm>
#include <fstream>
#include "PCI_def.h"
// Forward declarations
class IDEChannel;
class IDEInterface;

// Constants - Sector sizes
constexpr uint32_t CDROM_SECTOR_SIZE = 2048;
constexpr uint32_t HD_SECTOR_SIZE = 512;

// Bus master base address
constexpr uint32_t BUS_MASTER_BASE = 0xB400;

// ATA Register offsets (Command block)
constexpr uint8_t ATA_REG_ERROR = 0x01;
constexpr uint8_t ATA_REG_STATUS = 0x07;
constexpr uint8_t ATA_REG_ALT_STATUS = 0x00;  // Control block
constexpr uint8_t ATA_REG_DATA = 0x00;
constexpr uint8_t ATA_REG_SECTOR = 0x02;
constexpr uint8_t ATA_REG_LBA_LOW = 0x03;
constexpr uint8_t ATA_REG_LBA_MID = 0x04;
constexpr uint8_t ATA_REG_LBA_HIGH = 0x05;
constexpr uint8_t ATA_REG_DEVICE = 0x06;
constexpr uint8_t ATA_REG_FEATURES = 0x01;
constexpr uint8_t ATA_REG_COMMAND = 0x07;
constexpr uint8_t ATA_REG_CONTROL = 0x00;  // Control block

// Bus Master IDE register offsets
constexpr uint8_t BMI_REG_COMMAND = 0x00;
constexpr uint8_t BMI_REG_STATUS = 0x02;
constexpr uint8_t BMI_REG_PRDT = 0x04;

// Error register bits
constexpr uint8_t ATA_ER_ABRT = 0x04;

// Status register bits
constexpr uint8_t ATA_SR_ERR = 0x01;
constexpr uint8_t ATA_SR_COND = 0x01;
constexpr uint8_t ATA_SR_SENS = 0x02;
constexpr uint8_t ATA_SR_AERR = 0x04;
constexpr uint8_t ATA_SR_DRQ = 0x08;
constexpr uint8_t ATA_SR_DSC = 0x10;
constexpr uint8_t ATA_SR_DF = 0x20;
constexpr uint8_t ATA_SR_DRDY = 0x40;
constexpr uint8_t ATA_SR_BSY = 0x80;

// Device register bits
constexpr uint8_t ATA_DR_DEV = 0x10;

// Device Control register bits
constexpr uint8_t ATA_CR_NIEN = 0x02;
constexpr uint8_t ATA_CR_SRST = 0x04;
constexpr uint8_t ATA_CR_HOB = 0x80;

// ATA Commands
constexpr uint8_t ATA_CMD_DEVICE_RESET = 0x08;
constexpr uint8_t ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC = 0x90;
constexpr uint8_t ATA_CMD_FLUSH_CACHE = 0xE7;
constexpr uint8_t ATA_CMD_FLUSH_CACHE_EXT = 0xEA;
constexpr uint8_t ATA_CMD_GET_MEDIA_STATUS = 0xDA;
constexpr uint8_t ATA_CMD_IDENTIFY_DEVICE = 0xEC;
constexpr uint8_t ATA_CMD_IDENTIFY_PACKET_DEVICE = 0xA1;
constexpr uint8_t ATA_CMD_IDLE_IMMEDIATE = 0xE1;
constexpr uint8_t ATA_CMD_INITIALIZE_DEVICE_PARAMETERS = 0x91;
constexpr uint8_t ATA_CMD_MEDIA_LOCK = 0xDE;
constexpr uint8_t ATA_CMD_NOP = 0x00;
constexpr uint8_t ATA_CMD_PACKET = 0xA0;
constexpr uint8_t ATA_CMD_READ_DMA = 0xC8;
constexpr uint8_t ATA_CMD_READ_DMA_EXT = 0x25;
constexpr uint8_t ATA_CMD_READ_MULTIPLE = 0x29;
constexpr uint8_t ATA_CMD_READ_MULTIPLE_EXT = 0xC4;
constexpr uint8_t ATA_CMD_READ_NATIVE_MAX_ADDRESS = 0xF8;
constexpr uint8_t ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT = 0x27;
constexpr uint8_t ATA_CMD_READ_SECTORS = 0x20;
constexpr uint8_t ATA_CMD_READ_SECTORS_EXT = 0x24;
constexpr uint8_t ATA_CMD_READ_VERIFY_SECTORS = 0x40;
constexpr uint8_t ATA_CMD_SECURITY_FREEZE_LOCK = 0xF5;
constexpr uint8_t ATA_CMD_SET_FEATURES = 0xEF;
constexpr uint8_t ATA_CMD_SET_MAX = 0xF9;
constexpr uint8_t ATA_CMD_SET_MULTIPLE_MODE = 0xC6;
constexpr uint8_t ATA_CMD_STANDBY_IMMEDIATE = 0xE0;
constexpr uint8_t ATA_CMD_WRITE_DMA = 0xCA;
constexpr uint8_t ATA_CMD_WRITE_DMA_EXT = 0x35;
constexpr uint8_t ATA_CMD_WRITE_MULTIPLE = 0x39;
constexpr uint8_t ATA_CMD_WRITE_MULTIPLE_EXT = 0xC5;
constexpr uint8_t ATA_CMD_WRITE_SECTORS = 0x30;
constexpr uint8_t ATA_CMD_WRITE_SECTORS_EXT = 0x34;
constexpr uint8_t ATA_CMD_10h = 0x10;
constexpr uint8_t ATA_CMD_F0h = 0xF0;

// ATAPI Commands
constexpr uint8_t ATAPI_CMD_GET_CONFIGURATION = 0x46;
constexpr uint8_t ATAPI_CMD_GET_EVENT_STATUS_NOTIFICATION = 0x4A;
constexpr uint8_t ATAPI_CMD_INQUIRY = 0x12;
constexpr uint8_t ATAPI_CMD_MECHANISM_STATUS = 0xBD;
constexpr uint8_t ATAPI_CMD_MODE_SENSE_6 = 0x1A;
constexpr uint8_t ATAPI_CMD_MODE_SENSE_10 = 0x5A;
constexpr uint8_t ATAPI_CMD_PAUSE = 0x45;
constexpr uint8_t ATAPI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL = 0x1E;
constexpr uint8_t ATAPI_CMD_READ_10 = 0x28;
constexpr uint8_t ATAPI_CMD_READ_12 = 0xA8;
constexpr uint8_t ATAPI_CMD_READ_CAPACITY = 0x25;
constexpr uint8_t ATAPI_CMD_READ_CD = 0xBE;
constexpr uint8_t ATAPI_CMD_READ_DISK_INFORMATION = 0x51;
constexpr uint8_t ATAPI_CMD_READ_SUBCHANNEL = 0x42;
constexpr uint8_t ATAPI_CMD_READ_TOC_PMA_ATIP = 0x43;
constexpr uint8_t ATAPI_CMD_READ_TRACK_INFORMATION = 0x52;
constexpr uint8_t ATAPI_CMD_REQUEST_SENSE = 0x03;
constexpr uint8_t ATAPI_CMD_START_STOP_UNIT = 0x1B;
constexpr uint8_t ATAPI_CMD_TEST_UNIT_READY = 0x00;

// ATAPI command flags
constexpr uint8_t ATAPI_CF_NONE = 0x00;
constexpr uint8_t ATAPI_CF_NEEDS_DISK = 0x01;
constexpr uint8_t ATAPI_CF_UNIT_ATTN = 0x02;

// ATAPI signatures
constexpr uint8_t ATAPI_SIGNATURE_LO = 0x14;
constexpr uint8_t ATAPI_SIGNATURE_HI = 0xEB;

// ATAPI Sense Keys
constexpr uint8_t ATAPI_SK_NO_SENSE = 0;
constexpr uint8_t ATAPI_SK_RECOVERED_ERROR = 1;
constexpr uint8_t ATAPI_SK_NOT_READY = 2;
constexpr uint8_t ATAPI_SK_MEDIUM_ERROR = 3;
constexpr uint8_t ATAPI_SK_HARDWARE_ERROR = 4;
constexpr uint8_t ATAPI_SK_ILLEGAL_REQUEST = 5;
constexpr uint8_t ATAPI_SK_UNIT_ATTENTION = 6;
constexpr uint8_t ATAPI_SK_DATA_PROTECT = 7;
constexpr uint8_t ATAPI_SK_BLANK_CHECK = 8;
constexpr uint8_t ATAPI_SK_ABORTED_COMMAND = 11;

// ATAPI Additional Sense Codes
constexpr uint8_t ATAPI_ASC_INV_FIELD_IN_CMD_PACKET = 0x24;
constexpr uint8_t ATAPI_ASC_MEDIUM_MAY_HAVE_CHANGED = 0x28;
constexpr uint8_t ATAPI_ASC_MEDIUM_NOT_PRESENT = 0x3A;

// Debug logging bits
constexpr uint8_t LOG_DETAIL_NONE = 0x00;
constexpr uint8_t LOG_DETAIL_REG_IO = 0x01;
constexpr uint8_t LOG_DETAIL_IRQ = 0x02;
constexpr uint8_t LOG_DETAIL_RW = 0x04;
constexpr uint8_t LOG_DETAIL_RW_DMA = 0x08;
constexpr uint8_t LOG_DETAIL_CHS = 0x10;
constexpr uint8_t LOG_DETAIL_ALL = 0xFF;

// Configuration structure for IDE device
struct IDEDeviceConfig {
    std::fstream* buffer = nullptr;
    uint64_t buffer_size = 0;
    bool is_cdrom = false;
};

// ATAPI command info structure
struct ATAPICommandInfo {
    const char* name;
    uint8_t flags;
};

// Abstract buffer interface
class IDiskBuffer {
public:
    virtual ~IDiskBuffer() = default;
    virtual uint32_t getByteLength() const = 0;
    virtual void get(uint32_t start, uint32_t length,
        std::function<void(const uint8_t*)> callback) = 0;
    virtual void set(uint32_t start, const uint8_t* data, uint32_t length,
        std::function<void()> callback) = 0;
};

// IDE Interface (Master/Slave device)
class IDEInterface {
public:
    IDEInterface(IDEChannel* channel, uint8_t interface_nr,
        std::fstream* buffer, uint64_t buffer_size, bool is_cd);
    ~IDEInterface() = default;

    // Device operations
    bool hasDisk() const;
    void eject();
    void setCdrom(std::fstream* buffer, uint64_t buffer_size);
    void setDiskBuffer(std::fstream* buffer, uint64_t buffer_size);
    void deviceReset();
    void ataBortCommand();

    // Register access
    uint32_t readData(uint32_t length);
    void writeDataPort(uint32_t data, uint32_t length);
    void writeDataPort8(uint8_t data);
    void writeDataPort16(uint16_t data);
    void writeDataPort32(uint32_t data);

    // Command handling
    void ataCommand(uint8_t cmd);
    void ataReadSectors(uint8_t cmd);
    void ataWriteSectors(uint8_t cmd);
    void ataReadSectorsDma(uint8_t cmd);
    void doAtaReadSectorsDma();
    void ataWriteSectorsDma(uint8_t cmd);
    void doAtaWriteSectorsDma();

    // ATAPI handling
    void atapiHandle();
    void atapiCheckConditionResponse(uint8_t sense_key, uint8_t additional_sense);
    void atapiRead(std::vector<uint8_t> cmd);
    void atapiReadDma(const uint8_t* cmd);
    void doAtapiDma();

    // Data handling
    void dataAllocate(uint32_t len);
    void dataAllocateNoClear(uint32_t len);
    void dataSet(const uint8_t* data, uint32_t len);
    void readEnd();
    void writeEnd();
    void doWrite();

    // Sector calculations
    uint32_t getChs() const;
    uint32_t getLba28() const;
    uint32_t getLba48() const;
    uint32_t getLba(bool is_lba48) const;
    uint32_t getCount(bool is_lba48) const;
    void ataAdvance(uint8_t cmd, uint32_t sectors);

    // IDENTIFY packet creation
    void createIdentifyPacket();

    // State management
    std::vector<uint64_t> getState() const;
    void setState(const std::vector<uint64_t>& state);

    // Reporting
    void reportReadStart();
    void reportReadEnd(uint32_t byte_count);
    void reportWrite(uint32_t byte_count);

    // I/O cancellation
    void readBuffer(uint32_t start, uint32_t length,
        std::function<void(const uint8_t*)> callback);
    void cancelIoOperations();

    // Public members
    IDEChannel* channel;
    std::string name;
    bool drive_connected;

    uint32_t sector_size;
    bool is_atapi;
    uint32_t sector_count;
    uint32_t head_count;
    uint32_t sectors_per_track;
    uint32_t cylinder_count;

    uint16_t sector_count_reg;
    uint16_t lba_low_reg;
    uint16_t features_reg;
    uint16_t lba_mid_reg;
    uint16_t lba_high_reg;

    uint8_t head;
    uint8_t device_reg;
    uint8_t status_reg;
    uint8_t error_reg;
    uint8_t is_lba;

    uint32_t sectors_per_drq;
    uint32_t data_pointer;
    uint64_t data_length;
    uint64_t data_end;
    int32_t current_command;
    uint64_t write_dest;

    std::vector<uint8_t> data;

    // ATAPI-specific
    int32_t current_atapi_command;
    uint8_t atapi_sense_key;
    uint8_t atapi_add_sense;
    bool medium_changed;

private:
    std::fstream* buffer = nullptr;
    uint64_t buffer_size = 0;

    uint32_t channel_nr;
    uint32_t interface_nr;
    

    uint32_t last_io_id = 0;
    std::set<uint32_t> in_progress_io_ids;
    std::set<uint32_t> cancelled_io_ids;

    std::string captureRegs() const;
    void pushIrq();
};

// IDE Channel (Primary/Secondary)
class IDEChannel {
public:
    IDEChannel(class IDEController* controller, uint8_t channel_nr,
        const IDEDeviceConfig* channel_config,
        uint32_t command_base, uint32_t control_base, uint8_t irq);
    ~IDEChannel() = default;

    uint32_t readStatus();
    void writeControl(uint8_t data);

    uint32_t dmaReadAddr();
    void dmaSetAddr(uint32_t data);
    uint32_t dmaReadStatus();
    void dmaWriteStatus(uint8_t value);
    uint32_t dmaReadCommand();
    uint8_t dmaReadCommand8();
    void dmaWriteCommand(uint32_t value);
    void dmaWriteCommand8(uint8_t value);

    std::atomic<bool> IRQRaised = false;
    void pushIrq();
    void LowerIRQ();
    // State management
    std::vector<uint64_t> getState() const;
    void setState(const std::vector<uint64_t>& state);

    // Public members
    class IDEController* controller;
    uint8_t channel_nr;
    std::string name;
    uint32_t command_base;
    uint32_t control_base;
    uint8_t irq;

    IDEInterface* master;
    IDEInterface* slave;
    IDEInterface* current_interface;

    uint8_t device_control_reg =0;
    uint32_t prdt_addr;
    uint8_t dma_status;
    uint8_t dma_command;

private:
};

// IDE Controller
class IDEController:public PCIDevice {
public:
    IDEController(
        const IDEDeviceConfig config[2][2]);
    ~IDEController() = default;

    // State management
    std::vector<uint64_t> getState() const;
    void setState(const std::vector<uint64_t>& state);

    // Public members
    

    IDEChannel* primary;
    IDEChannel* secondary;

    std::string name = "ide";
    uint16_t pci_id = 0x1E << 3;

private:
    void registerPciDevice();
};


#include "IDE_controller.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <string>
#include <queue>
// Forward declaration helper functions
static std::unordered_map<uint8_t, const char*> createAtaCmdNameMap();
static std::unordered_map<uint8_t, ATAPICommandInfo> createAtapiCmdMap();


static const std::unordered_map<uint8_t, const char*> ATA_CMD_NAME =
createAtaCmdNameMap();
static const std::unordered_map<uint8_t, ATAPICommandInfo> ATAPI_CMD =
createAtapiCmdMap();

// Helper function to create ATA command name map
std::unordered_map<uint8_t, const char*> createAtaCmdNameMap() {
    return {
        {ATA_CMD_DEVICE_RESET, "DEVICE RESET"},
        {ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC, "EXECUTE DEVICE DIAGNOSTIC"},
        {ATA_CMD_FLUSH_CACHE, "FLUSH CACHE"},
        {ATA_CMD_FLUSH_CACHE_EXT, "FLUSH CACHE EXT"},
        {ATA_CMD_GET_MEDIA_STATUS, "GET MEDIA STATUS"},
        {ATA_CMD_IDENTIFY_DEVICE, "IDENTIFY DEVICE"},
        {ATA_CMD_IDENTIFY_PACKET_DEVICE, "IDENTIFY PACKET DEVICE"},
        {ATA_CMD_IDLE_IMMEDIATE, "IDLE IMMEDIATE"},
        {ATA_CMD_INITIALIZE_DEVICE_PARAMETERS, "INITIALIZE DEVICE PARAMETERS"},
        {ATA_CMD_MEDIA_LOCK, "MEDIA LOCK"},
        {ATA_CMD_NOP, "NOP"},
        {ATA_CMD_PACKET, "PACKET"},
        {ATA_CMD_READ_DMA, "READ DMA"},
        {ATA_CMD_READ_DMA_EXT, "READ DMA EXT"},
        {ATA_CMD_READ_MULTIPLE, "READ MULTIPLE"},
        {ATA_CMD_READ_MULTIPLE_EXT, "READ MULTIPLE EXT"},
        {ATA_CMD_READ_NATIVE_MAX_ADDRESS, "READ NATIVE MAX ADDRESS"},
        {ATA_CMD_READ_NATIVE_MAX_ADDRESS_EXT, "READ NATIVE MAX ADDRESS EXT"},
        {ATA_CMD_READ_SECTORS, "READ SECTORS"},
        {ATA_CMD_READ_SECTORS_EXT, "READ SECTORS EXT"},
        {ATA_CMD_READ_VERIFY_SECTORS, "READ VERIFY SECTORS"},
        {ATA_CMD_SECURITY_FREEZE_LOCK, "SECURITY FREEZE LOCK"},
        {ATA_CMD_SET_FEATURES, "SET FEATURES"},
        {ATA_CMD_SET_MAX, "SET MAX"},
        {ATA_CMD_SET_MULTIPLE_MODE, "SET MULTIPLE MODE"},
        {ATA_CMD_STANDBY_IMMEDIATE, "STANDBY IMMEDIATE"},
        {ATA_CMD_WRITE_DMA, "WRITE DMA"},
        {ATA_CMD_WRITE_DMA_EXT, "WRITE DMA EXT"},
        {ATA_CMD_WRITE_MULTIPLE, "WRITE MULTIPLE"},
        {ATA_CMD_WRITE_MULTIPLE_EXT, "WRITE MULTIPLE EXT"},
        {ATA_CMD_WRITE_SECTORS, "WRITE SECTORS"},
        {ATA_CMD_WRITE_SECTORS_EXT, "WRITE SECTORS EXT"},
        {ATA_CMD_10h, "<UNKNOWN 10h>"},
        {ATA_CMD_F0h, "<VENDOR-SPECIFIC F0h>"},
    };
}

// Helper function to create ATAPI command map
std::unordered_map<uint8_t, ATAPICommandInfo> createAtapiCmdMap() {
    return {
        {ATAPI_CMD_GET_CONFIGURATION, {"GET CONFIGURATION", ATAPI_CF_NONE}},
        {ATAPI_CMD_GET_EVENT_STATUS_NOTIFICATION, {"GET EVENT STATUS NOTIFICATION", ATAPI_CF_NONE}},
        {ATAPI_CMD_INQUIRY, {"INQUIRY", ATAPI_CF_NONE}},
        {ATAPI_CMD_MECHANISM_STATUS, {"MECHANISM STATUS", ATAPI_CF_NONE}},
        {ATAPI_CMD_MODE_SENSE_6, {"MODE SENSE (6)", ATAPI_CF_NONE}},
        {ATAPI_CMD_MODE_SENSE_10, {"MODE SENSE (10)", ATAPI_CF_NONE}},
        {ATAPI_CMD_PAUSE, {"PAUSE", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL, {"PREVENT ALLOW MEDIUM REMOVAL", ATAPI_CF_NONE}},
        {ATAPI_CMD_READ_10, {"READ (10)", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_12, {"READ (12)", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_CAPACITY, {"READ CAPACITY", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_CD, {"READ CD", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_DISK_INFORMATION, {"READ DISK INFORMATION", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_SUBCHANNEL, {"READ SUBCHANNEL", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_TOC_PMA_ATIP, {"READ TOC PMA ATIP", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_READ_TRACK_INFORMATION, {"READ TRACK INFORMATION", ATAPI_CF_NEEDS_DISK}},
        {ATAPI_CMD_REQUEST_SENSE, {"REQUEST SENSE", ATAPI_CF_NONE}},
        {ATAPI_CMD_START_STOP_UNIT, {"START STOP UNIT", ATAPI_CF_NONE}},
        {ATAPI_CMD_TEST_UNIT_READY, {"TEST UNIT READY", ATAPI_CF_NEEDS_DISK}},
    };
}

// IDEInterface implementation
IDEInterface::IDEInterface(IDEChannel* channel, uint8_t interface_nr,
    std::fstream* buffer, uint64_t buffer_size, bool is_cd)
    : channel(channel), interface_nr(interface_nr),
    is_atapi(is_cd), buffer(buffer), buffer_size(buffer_size),
    drive_connected(is_cd || buffer != nullptr), sector_size(is_cd ? CDROM_SECTOR_SIZE : HD_SECTOR_SIZE),
    sector_count(0), head_count(is_cd ? 1 : 0), sectors_per_track(0),
    cylinder_count(0), is_lba(0), sector_count_reg(0), lba_low_reg(0),
    features_reg(0), lba_mid_reg(0), lba_high_reg(0), head(0),
    device_reg(0), status_reg(ATA_SR_DRDY | ATA_SR_DSC), sectors_per_drq(0x80),
    error_reg(0), data_pointer(0), data_length(0), data_end(0),
    current_command(-1), write_dest(0), current_atapi_command(-1),
    atapi_sense_key(0), atapi_add_sense(0), medium_changed(false),
    channel_nr(channel->channel_nr)
{
    name = channel->name + "." + std::to_string(interface_nr);
    data.resize(64 * 1024);

    if (buffer && buffer_size > 0) {
        setDiskBuffer(buffer, buffer_size);
    }

    if (drive_connected) {
        std::cout << name << ": " << (is_atapi ? "ATAPI CD-ROM" : "ATA HD")
            << " device ready" << std::endl;
    }
}

bool IDEInterface::hasDisk() const {
    return buffer != nullptr;
}

void IDEInterface::eject() {
    if (is_atapi && buffer) {
        medium_changed = true;
        buffer = nullptr;
        buffer_size = 0;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        error_reg = ATAPI_SK_UNIT_ATTENTION << 4;
        pushIrq();
    }
}

void IDEInterface::setCdrom(std::fstream* new_buffer, uint64_t new_buffer_size) {
    if (is_atapi && new_buffer) {
        setDiskBuffer(new_buffer, new_buffer_size);
        medium_changed = true;
    }
}

void IDEInterface::setDiskBuffer(std::fstream* new_buffer, uint64_t new_buffer_size) {
    if (!new_buffer || new_buffer_size == 0) {
        return;
    }

    buffer = new_buffer;
    buffer_size = new_buffer_size;

    if (is_atapi) {
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        error_reg = ATAPI_SK_UNIT_ATTENTION << 4;
    }

    sector_count = buffer_size / sector_size;
    if (buffer_size % sector_size != 0) {
        std::cout << name << ": warning: disk size not aligned with sector size" << std::endl;
        sector_count = (buffer_size + sector_size - 1) / sector_size;
    }

    if (is_atapi) {
        head_count = 1;
        sectors_per_track = 2048;
    }
    else {
        head_count = 16;
        sectors_per_track = 63;
    }

    cylinder_count = sector_count / (head_count * sectors_per_track);
    if (sector_count % (head_count * sectors_per_track) != 0) {
        std::cout << name << ": warning: rounding up cylinder count" << std::endl;
    }

    pushIrq();
}

void IDEInterface::deviceReset() {
    if (is_atapi) {
        status_reg = 0 ;
        sector_count_reg = 1;
        error_reg = 1;
        lba_low_reg = 1;
        lba_mid_reg = ATAPI_SIGNATURE_LO;
        lba_high_reg = ATAPI_SIGNATURE_HI;
    }
    else {
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_ERR;
        sector_count_reg = 1;
        error_reg = 1;
        lba_low_reg = 1;
        lba_mid_reg = 0;
        lba_high_reg = 0;
    }
    cancelIoOperations();
}

void IDEInterface::pushIrq() {
    channel->pushIrq();
}

void IDEInterface::ataBortCommand() {
    error_reg = ATA_ER_ABRT;
    status_reg = ATA_SR_DRDY | ATA_SR_ERR;
    pushIrq();
}

std::string IDEInterface::captureRegs() const {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "ST=%02X ER=%02X SC=%02X LL=%02X LM=%02X LH=%02X FE=%02X",
        status_reg & 0xFF, error_reg & 0xFF, sector_count_reg & 0xFF,
        lba_low_reg & 0xFF, lba_mid_reg & 0xFF, lba_high_reg & 0xFF,
        features_reg & 0xFF);
    return std::string(buf);
}

uint32_t IDEInterface::readData(uint32_t length) {
    uint32_t result = 0;

    for (uint32_t i = 0; i < length && data_pointer < data_end; i++) {
        result |= ((uint32_t)data[data_pointer++] << (i * 8));
    }

    if (data_pointer >= data_end) {
        readEnd();
    }

    return result;
}

void IDEInterface::writeDataPort(uint32_t value, uint32_t length) {
    if (data_pointer >= data_end) {
        std::cout << name << ": redundant write to data port" << std::endl;
    }
    else {
        if (length == 1) {
            data[data_pointer++] = value & 0xFF;
        }
        else if (length == 2) {
            data[data_pointer] = value & 0xFF;
            data[data_pointer + 1] = (value >> 8) & 0xFF;
            data_pointer += 2;
        }
        else if (length == 4) {
            data[data_pointer] = value & 0xFF;
            data[data_pointer + 1] = (value >> 8) & 0xFF;
            data[data_pointer + 2] = (value >> 16) & 0xFF;
            data[data_pointer + 3] = (value >> 24) & 0xFF;
            data_pointer += 4;
        }

        if (data_pointer >= data_end) {
            writeEnd();
        }
    }
}

void IDEInterface::writeDataPort8(uint8_t data) {
    writeDataPort(data, 1);
}

void IDEInterface::writeDataPort16(uint16_t data) {
    writeDataPort(data, 2);
}

void IDEInterface::writeDataPort32(uint32_t data) {
    writeDataPort(data, 4);
}

void IDEInterface::readEnd() {
    //printf("[readEnd CALLED] command=0x%02X, pointer=%d, length=%d\n",current_command, data_pointer, data_length);
    if (current_command == ATA_CMD_PACKET) {
        if (data_end == data_length) {
            status_reg = ATA_SR_DRDY | ATA_SR_DSC;
            sector_count_reg = (sector_count_reg & ~7) | 3;
            pushIrq();
        }
        else {
            status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
            sector_count_reg = (sector_count_reg & ~7) | 2;
            pushIrq();

            uint32_t byte_count = ((lba_high_reg << 8) & 0xFF00) | (lba_mid_reg & 0xFF);
            if (data_end + byte_count > data_length) {
                lba_mid_reg = (data_length - data_end) & 0xFF;
                lba_high_reg = ((data_length - data_end) >> 8) & 0xFF;
                data_end = data_length;
            }
            else {
                data_end += byte_count;
            }
        }
    }
    else {
        error_reg = 0;
        if (data_pointer >= data_length) {
            status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        }
        else {
            uint32_t sector_count;
            if (current_command == ATA_CMD_READ_MULTIPLE ||
                current_command == ATA_CMD_READ_MULTIPLE_EXT) {
                sector_count = min(sectors_per_drq,
                    (data_length - data_end) / 512);
            }
            else {
                sector_count = 1;
            }
            ataAdvance(current_command, sector_count);
            data_end += 512 * sector_count;
            status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
            pushIrq();
        }
    }
}

void IDEInterface::writeEnd() {
    //printf("[writeEnd CALLED] command=0x%02X, pointer=%d, length=%d\n",current_command, data_pointer, data_length);
    if (current_command == ATA_CMD_PACKET) {
        atapiHandle();
    }
    else {
        if (data_pointer >= data_length) {
            doWrite();
        }
        else {
            status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
            data_end += 512;
            pushIrq();
        }
    }
}

void IDEInterface::doWrite() {
    status_reg = ATA_SR_DRDY | ATA_SR_DSC;
    ataAdvance(current_command, data_length / 512);
    pushIrq();
    reportWrite(data_length);
    if (buffer && write_dest + data_length <= buffer_size) {
        buffer->seekp(write_dest, std::ios::beg);
        buffer->write((const char*)data.data(), data_length);
        buffer->flush();
    }
}

uint32_t IDEInterface::getChs() const {
    uint32_t c = (lba_mid_reg & 0xFF) | ((lba_high_reg << 8) & 0xFF00);
    uint32_t h = head;
    uint32_t s = lba_low_reg & 0xFF;
    return (c * head_count + h) * sectors_per_track + s - 1;
}

uint32_t IDEInterface::getLba28() const {
    return (lba_low_reg & 0xFF) |
        ((lba_mid_reg << 8) & 0xFF00) |
        ((lba_high_reg << 16) & 0xFF0000) |
        ((head & 0xF) << 24);
}

uint32_t IDEInterface::getLba48() const {
    return ((lba_low_reg & 0xFF) |
        ((lba_mid_reg << 8) & 0xFF00) |
        ((lba_high_reg << 16) & 0xFF0000) |
        (((lba_low_reg >> 8) << 24) & 0xFF000000));
}

uint32_t IDEInterface::getLba(bool is_lba48) const {
    if (is_lba48) {
        return getLba48();
    }
    else if (is_lba) {
        return getLba28();
    }
    else {
        return getChs();
    }
}

uint32_t IDEInterface::getCount(bool is_lba48) const {
    uint32_t count;
    if (is_lba48) {
        count = sector_count_reg;
        if (count == 0) count = 0x10000;
    }
    else {
        count = sector_count_reg & 0xFF;
        if (count == 0) count = 0x100;
    }
    return count;
}

void IDEInterface::ataAdvance(uint8_t cmd, uint32_t sectors) {
    sector_count_reg -= sectors;

    if (cmd == ATA_CMD_READ_SECTORS_EXT ||
        cmd == ATA_CMD_READ_MULTIPLE ||
        cmd == ATA_CMD_READ_DMA_EXT ||
        cmd == ATA_CMD_WRITE_SECTORS_EXT ||
        cmd == ATA_CMD_WRITE_MULTIPLE ||
        cmd == ATA_CMD_WRITE_DMA_EXT) {

        uint32_t new_sector = sectors + getLba48();
        lba_low_reg = new_sector & 0xFF | ((new_sector >> 16) & 0xFF00);
        lba_mid_reg = (new_sector >> 8) & 0xFF;
        lba_high_reg = (new_sector >> 16) & 0xFF;
    }
    else if (is_lba) {
        uint32_t new_sector = sectors + getLba28();
        lba_low_reg = new_sector & 0xFF;
        lba_mid_reg = (new_sector >> 8) & 0xFF;
        lba_high_reg = (new_sector >> 16) & 0xFF;
        head = (head & ~0xF) | (new_sector >> 24 & 0xF);
    }
    else {
        uint32_t new_sector = sectors + getChs();
        uint32_t c = new_sector / (head_count * sectors_per_track);
        lba_mid_reg = c & 0xFF;
        lba_high_reg = (c >> 8) & 0xFF;
        head = ((new_sector / sectors_per_track) % head_count) & 0xF;
        lba_low_reg = (new_sector % sectors_per_track + 1) & 0xFF;
    }
}

void IDEInterface::dataAllocate(uint32_t len) {
    dataAllocateNoClear(len);
    std::fill(data.begin(), data.end(), 0);
}

void IDEInterface::dataAllocateNoClear(uint32_t len) {
    if (data.size() < len) {
        data.resize((len + 3) & ~3);
    }
    data_length = len;
    data_pointer = 0;
}

void IDEInterface::dataSet(const uint8_t* new_data, uint32_t len) {
    dataAllocateNoClear(len);
    std::copy(new_data, new_data + len, data.begin());
}

void IDEInterface::reportReadStart() {
    // Send "ide-read-start" event to bus
}

void IDEInterface::reportReadEnd(uint32_t byte_count) {
    // Send "ide-read-end" event with byte_count and sector_count
}

void IDEInterface::reportWrite(uint32_t byte_count) {
    // Send "ide-write-end" event with byte_count and sector_count
}

void IDEInterface::readBuffer(uint32_t start, uint32_t length,
    std::function<void(const uint8_t*)> callback) {
    uint32_t id = last_io_id++;
    in_progress_io_ids.insert(id);

    if (!buffer || start + length > buffer_size) {
        // clean up and bail — mirrors JS buffer.get never calling back
        in_progress_io_ids.erase(id);
        return;
    }

    // synchronous "async" read — mirrors what happens inside buffer.get's callback
    {
        // Check if cancelled (mirrors: if(this.cancelled_io_ids.delete(id)) return)
        if (cancelled_io_ids.erase(id)) {
            // JS asserts it's NOT in in_progress at this point because
            // cancelIoOperations() moved it. Mirror that.
            in_progress_io_ids.count(id);
            return;
        }

        // Not cancelled — remove from in_progress and invoke callback
        bool removed = in_progress_io_ids.erase(id);

        buffer->seekg(start, std::ios::beg);
        std::vector<uint8_t> buf(length);
        buffer->read(reinterpret_cast<char*>(buf.data()), length);
        callback(buf.data());
    }
}

void IDEInterface::cancelIoOperations() {
    for (uint32_t id : in_progress_io_ids) {
        cancelled_io_ids.insert(id);
    }
    in_progress_io_ids.clear();
}

std::vector<uint64_t> IDEInterface::getState() const {
    std::vector<uint64_t> state(30);
    state[0] = sector_count_reg;
    state[1] = cylinder_count;
    state[2] = lba_high_reg;
    state[3] = lba_mid_reg;
    state[4] = data_pointer;
    state[5] = 0;
    state[6] = 0;
    state[7] = 0;
    state[8] = 0;
    state[9] = device_reg;
    state[10] = error_reg;
    state[11] = head;
    state[12] = head_count;
    state[13] = is_atapi ? 1 : 0;
    state[14] = is_lba ? 1 : 0;
    state[15] = features_reg;
    // state[16] = data (skipped, handled separately)
    state[17] = data_length;
    state[18] = lba_low_reg;
    state[19] = sector_count;
    state[20] = sector_size;
    state[21] = sectors_per_drq;
    state[22] = sectors_per_track;
    state[23] = status_reg;
    state[24] = write_dest;
    state[25] = current_command;
    state[26] = data_end;
    state[27] = current_atapi_command;
    // state[28] = buffer (skipped)
    return state;
}

void IDEInterface::setState(const std::vector<uint64_t>& state) {
    if (state.size() < 29) return;

    sector_count_reg = state[0];
    cylinder_count = state[1];
    lba_high_reg = state[2];
    lba_mid_reg = state[3];
    data_pointer = state[4];
    device_reg = state[9];
    error_reg = state[10];
    head = state[11];
    head_count = state[12];
    is_atapi = state[13] != 0;
    is_lba = state[14] != 0;
    features_reg = state[15];
    data_length = state[17];
    lba_low_reg = state[18];
    sector_count = state[19];
    sector_size = state[20];
    sectors_per_drq = state[21];
    sectors_per_track = state[22];
    status_reg = state[23];
    write_dest = state[24];
    current_command = state[25];
    data_end = state[26];
    current_atapi_command = state[27];

    drive_connected = is_atapi || (buffer != nullptr);
    medium_changed = false;
}

// Stub implementations for complex methods
void IDEInterface::ataCommand(uint8_t cmd) {
    if (cmd == ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC) {
        std::cout << name << ": ATA not ignored: no drive connected" << std::endl;
    }
    if (!drive_connected && cmd != ATA_CMD_EXECUTE_DEVICE_DIAGNOSTIC) {
        std::cout << name << ": ATA command ignored: no drive connected" << std::endl;
        //ataBortCommand();
        return;
    }

    current_command = cmd;
    error_reg = 0;
    static bool busy = true;
    switch (cmd) {
    case ATA_CMD_DEVICE_RESET:
        deviceReset();
        pushIrq();
        break;
    case ATA_CMD_READ_SECTORS:
        if (is_atapi) {
            lba_mid_reg = ATAPI_SIGNATURE_LO;  // see [ATA8-ACS] 4.3
            lba_high_reg = ATAPI_SIGNATURE_HI;
            ataBortCommand();
        }
        else {
            ataReadSectors(cmd);
        }
        break;
    case ATA_CMD_READ_SECTORS_EXT:
    case ATA_CMD_READ_MULTIPLE:
    case ATA_CMD_READ_MULTIPLE_EXT:
        if (is_atapi)
        {
            ataBortCommand();
        }
        else
        {
            ataReadSectors(cmd);
        }
        break;
    case ATA_CMD_WRITE_SECTORS:
    case ATA_CMD_WRITE_SECTORS_EXT:
    case ATA_CMD_WRITE_MULTIPLE:
    case ATA_CMD_WRITE_MULTIPLE_EXT:
        if (is_atapi)
        {
            ataBortCommand();
        }
        else
        {
            ataWriteSectors(cmd);
        }
        break;
    case ATA_CMD_READ_DMA:
    case ATA_CMD_READ_DMA_EXT:
        ataReadSectorsDma(cmd);
        break;
    case ATA_CMD_WRITE_DMA:
    case ATA_CMD_WRITE_DMA_EXT:
        ataWriteSectorsDma(cmd);
        break;
    case ATA_CMD_IDENTIFY_DEVICE:
        if (is_atapi) {
            lba_mid_reg = ATAPI_SIGNATURE_LO;
            lba_high_reg = ATAPI_SIGNATURE_HI;
            ataBortCommand();
        }
        else {
            createIdentifyPacket();
            status_reg = ATA_SR_DRDY | ATA_SR_DRQ| ATA_SR_DSC;
            pushIrq();
        }
        break;
    case ATA_CMD_IDENTIFY_PACKET_DEVICE:
        if (is_atapi) {
            createIdentifyPacket();
            status_reg = ATA_SR_DRDY | ATA_SR_DRQ | ATA_SR_DSC;
            sector_count_reg = 1;
            pushIrq();
        }
        else {
            ataBortCommand();
        }
        break;
    case ATA_CMD_PACKET:
        if (is_atapi) {
            dataAllocate(12);
            data_end = 12;
            sector_count_reg = 1;
            status_reg = ATA_SR_DRDY|ATA_SR_DSC |ATA_SR_DRQ;
            pushIrq();
        }
        else {
            ataBortCommand();
        }
        break;
    case ATA_CMD_SET_FEATURES:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_FLUSH_CACHE:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;

    case ATA_CMD_FLUSH_CACHE_EXT:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_INITIALIZE_DEVICE_PARAMETERS:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_IDLE_IMMEDIATE:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_SECURITY_FREEZE_LOCK:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_READ_VERIFY_SECTORS:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    case ATA_CMD_STANDBY_IMMEDIATE:
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        pushIrq();
        break;
    default:
        std::cout << "Unknown ATA command" << std::hex << cmd<<std::endl;
        ataBortCommand();
        break;
    }
}

void IDEInterface::ataReadSectors(uint8_t cmd) {
    bool is_lba48 = (cmd == ATA_CMD_READ_SECTORS_EXT ||
        cmd == ATA_CMD_READ_MULTIPLE);           // matches JS exactly
    bool is_single = (cmd == ATA_CMD_READ_SECTORS ||
        cmd == ATA_CMD_READ_SECTORS_EXT);

    uint32_t count = getCount(is_lba48);
    uint32_t lba = getLba(is_lba48);
    uint32_t byte_count = count * sector_size;
    uint32_t start = lba * sector_size;

    if (start + byte_count > buffer_size) {
        status_reg = 0xFF;
        pushIrq();
        return;
    }

    status_reg = ATA_SR_DRDY | ATA_SR_BSY;
    reportReadStart();

    readBuffer(start, byte_count, [this, cmd, count, byte_count, is_single](const uint8_t* buf) {
        dataSet(buf, byte_count);
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;

        data_end = is_single
            ? 512
            : min(byte_count, sectors_per_drq * 512u);

        ataAdvance(cmd, is_single
            ? 1
            : min(count, (uint32_t)sectors_per_track));

        pushIrq();
        reportReadEnd(byte_count);
        });
}

void IDEInterface::ataWriteSectors(uint8_t cmd) {
    bool is_lba48 = (cmd == ATA_CMD_WRITE_SECTORS_EXT ||
        cmd == ATA_CMD_WRITE_MULTIPLE ||
        cmd == ATA_CMD_WRITE_MULTIPLE_EXT);
    uint64_t count = getCount(is_lba48);
    uint64_t lba = getLba(is_lba48);
    uint64_t byte_count = count * sector_size;
    uint64_t start = lba * sector_size;
    bool isSingle = cmd == ATA_CMD_WRITE_SECTORS || cmd == ATA_CMD_WRITE_SECTORS_EXT;
    if (buffer && start + byte_count <= buffer_size) {
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        dataAllocateNoClear(byte_count);
        data_end = isSingle ? 512 : min(byte_count, sectors_per_drq * 512);
        write_dest = start;
    }
    else {
        status_reg = 0xFF;
        pushIrq();
    }
}

void IDEInterface::ataReadSectorsDma(uint8_t cmd) {
    bool is_lba48 = (cmd == ATA_CMD_READ_DMA_EXT);
    uint32_t count = getCount(is_lba48);
    uint32_t lba = getLba(is_lba48);
    uint32_t byte_count = count * sector_size;

    status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
    channel->dma_status |= 1;
}

void IDEInterface::doAtaReadSectorsDma() {
    // DMA read implementation
}

void IDEInterface::ataWriteSectorsDma(uint8_t cmd) {
    bool is_lba48 = (cmd == ATA_CMD_WRITE_DMA_EXT);
    uint32_t count = getCount(is_lba48);
    uint32_t lba = getLba(is_lba48);
    uint32_t byte_count = count * sector_size;

    status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
    channel->dma_status |= 1;
}

void IDEInterface::doAtaWriteSectorsDma() {
    // DMA write implementation
}

void IDEInterface::atapiHandle() {
    uint8_t cmd_packet[12];
    std::copy(data.begin(), data.begin() + 12, cmd_packet);

    uint8_t cmd = data[0];
    auto it = ATAPI_CMD.find(cmd);
    const char* cmd_name = it != ATAPI_CMD.end() ? it->second.name : "<undefined>";
    uint8_t cmd_flags = it != ATAPI_CMD.end() ? it->second.flags : ATAPI_CF_NONE;

    data_pointer = 0;
    current_atapi_command = cmd;

    if (cmd != ATAPI_CMD_REQUEST_SENSE) {
        atapi_sense_key = 0;
        atapi_add_sense = 0;
    }

    if (!buffer && (cmd_flags & ATAPI_CF_NEEDS_DISK)) {
        atapiCheckConditionResponse(ATAPI_SK_NOT_READY, ATAPI_ASC_MEDIUM_NOT_PRESENT);
        pushIrq();
        return;
    }

    switch (cmd) {
    case ATAPI_CMD_TEST_UNIT_READY:
        if (buffer) {
            dataAllocate(0);
            data_end = data_length;
            status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        }
        else {
            atapiCheckConditionResponse(ATAPI_SK_NOT_READY, ATAPI_ASC_MEDIUM_NOT_PRESENT);
        }
        break;
    case ATAPI_CMD_READ_CAPACITY: {
        uint32_t count = sector_count - 1;
        uint8_t capacity_data[] = {
            (uint8_t)(count >> 24), (uint8_t)(count >> 16),
            (uint8_t)(count >> 8), (uint8_t)count,
            0, 0,
            (uint8_t)(sector_size >> 8), (uint8_t)sector_size
        };
        dataSet(capacity_data, sizeof(capacity_data));
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        break;
    }
    case ATAPI_CMD_READ_10:
    case ATAPI_CMD_READ_12:
        atapiRead(data);
        break;
    case ATAPI_CMD_REQUEST_SENSE:
        dataAllocate(data[4]);
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        data[0] = 0x80 | 0x70;
        data[2] = atapi_sense_key;
        data[7] = 8;
        data[12] = atapi_add_sense;
        atapi_sense_key = 0;
        atapi_add_sense = 0;
        break;
    case ATAPI_CMD_INQUIRY: {
        // cmd_packet[4] is the allocation length requested by host
        int length = cmd_packet[4];
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        // Allocate fresh buffer (preserves 64KB backing store via resize-only growth)
        // then fill — never shrink the vector with an assignment literal
        static const uint8_t inquiry_data[36] = {
            // 0: Device-type, Removable, ANSI-Version, Response Format
            0x05, 0x80, 0x01, 0x31,
            // 4: Additional length, Reserved, Reserved, Reserved
            31, 0, 0, 0,
            // 8: Vendor Identification "SONY    "
            0x53, 0x4F, 0x4E, 0x59,
            0x20, 0x20, 0x20, 0x20,
            // 16: Product Identification "CD-ROM CDU-1000 "
            0x43, 0x44, 0x2D, 0x52,
            0x4F, 0x4D, 0x20, 0x43,
            0x44, 0x55, 0x2D, 0x31,
            0x30, 0x30, 0x30, 0x20,
            // 32: Product Revision Level "1.1a"
            0x31, 0x2E, 0x31, 0x61,
        };
        uint32_t response_length = (uint32_t)min(36, length);
        dataAllocate(response_length);
        std::copy(inquiry_data, inquiry_data + response_length, data.begin());
        data_end = data_length;
        break;
    }
    case ATAPI_CMD_MODE_SENSE_6:
        dataAllocate(data[4]);
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        break;
    case ATAPI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
        dataAllocate(0);
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        break;
    case ATAPI_CMD_MODE_SENSE_10: {
        uint64_t length = data[8] | data[7] << 8;
        uint64_t page_code = data[2];
        if (page_code == 0x2A)
        {
            dataAllocate(min(30, length));
        }
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        break;
    }
    case ATAPI_CMD_GET_CONFIGURATION: {
        int length = min(data[8] | data[7] << 8, 32);
        dataAllocate(length);
        data_end = data_length;
        data[0] = length - 4 >> 24 & 0xFF;
        data[1] = length - 4 >> 16 & 0xFF;
        data[2] = length - 4 >> 8 & 0xFF;
        data[3] = length - 4 & 0xFF;
        data[6] = 0x08;
        data[10] = 3;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        break;
    }
    case ATAPI_CMD_READ_CD:
        dataAllocate(0);
        data_end = data_length;
        status_reg = ATA_SR_DRDY | ATA_SR_DSC;
        break;
    case ATAPI_CMD_GET_EVENT_STATUS_NOTIFICATION:
        atapiCheckConditionResponse(ATAPI_SK_ILLEGAL_REQUEST, ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
        break;
    case ATAPI_CMD_READ_TOC_PMA_ATIP: {
        // Note: Big Endian 16-bit length field, bytes 7-8 of command packet
        uint32_t length = (uint32_t)(cmd_packet[7] << 8) | cmd_packet[8];
        // Format field is upper 2 bits of byte 9
        uint8_t format = cmd_packet[9] >> 6;

        dataAllocate(length);       // zeroes buffer, sets data_length = length
        data_end = data_length;

        if (format == 0)
        {
            // Format 0: Return TOC data
            // Response contains two track descriptors:
            //   - Track 1 (data track, ADR/Control=0x14)
            //   - Track 0xAA (lead-out track, ADR/Control=0x16) with LBA = sector_count
            uint32_t sc = sector_count;
            static const uint8_t toc_header[] = {
                0, 18,          // TOC data length (18 bytes follow)
                1,              // first track number in TOC
                1,              // last track number in TOC
            };
            // Track descriptor for track 1
            static const uint8_t track1_descriptor[] = {
                0,              // reserved
                0x14,           // ADR=1 (Q subchannel), Control=4 (data track)
                1,              // track number
                0,              // reserved
                0, 0, 0, 0,     // track start LBA = 0
            };
            // Track descriptor for lead-out (track 0xAA)
            uint8_t leadout_descriptor[] = {
                0,              // reserved
                0x16,           // ADR=1 (Q subchannel), Control=6 (data track, copy permitted)
                0xAA,           // track number: lead-out
                0,              // reserved
                (uint8_t)(sc >> 24),
                (uint8_t)(sc >> 16 & 0xFF),
                (uint8_t)(sc >> 8 & 0xFF),
                (uint8_t)(sc & 0xFF),
            };

            // Copy into data buffer only as far as allocated length allows
            uint32_t pos = 0;
            auto append = [&](const uint8_t* src, uint32_t n) {
                uint32_t to_copy = min(n, length > pos ? length - pos : 0u);
                std::copy(src, src + to_copy, data.begin() + pos);
                pos += n;   // advance by full n so pos tracks "logical" position
            };

            append(toc_header, sizeof(toc_header));
            append(track1_descriptor, sizeof(track1_descriptor));
            append(leadout_descriptor, sizeof(leadout_descriptor));
        }
        else if (format == 1)
        {
            // Format 1: Return session info
            // Single session disc, one complete session
            static const uint8_t session_data[] = {
                0, 10,          // TOC data length (10 bytes follow)
                1,              // first complete session
                1,              // last complete session
                0, 0,           // reserved
                0, 0,           // reserved
                0, 0,           // reserved
                0, 0,           // reserved
            };
            uint32_t to_copy = min((uint32_t)sizeof(session_data), length);
            std::copy(session_data, session_data + to_copy, data.begin());
        }
        else
        {
            // Unsupported format — return CHECK CONDITION
            atapiCheckConditionResponse(ATAPI_SK_ILLEGAL_REQUEST,
                ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
            break;
        }

        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
        break;
    }
    default:
        std::cout << "Unknown ATAPI command" << std::hex << cmd << std::endl;
        atapiCheckConditionResponse(ATAPI_SK_ILLEGAL_REQUEST, ATAPI_ASC_INV_FIELD_IN_CMD_PACKET);
        break;
    }

    sector_count_reg = (sector_count_reg & ~7) | 2;

    if ((status_reg & ATA_SR_BSY) == 0) {
        pushIrq();
    }

    if ((status_reg & ATA_SR_BSY) == 0 && data_length == 0) {
        sector_count_reg |= 1;
        status_reg &= ~ATA_SR_DRQ;
    }
}

void IDEInterface::atapiCheckConditionResponse(uint8_t sense_key, uint8_t additional_sense) {
    dataAllocate(0);
    data_end = data_length;
    status_reg = ATA_SR_DRDY | ATA_SR_COND;
    error_reg = sense_key << 4;
    sector_count_reg = (sector_count_reg & ~7) | 2 | 1;
    atapi_sense_key = sense_key;
    atapi_add_sense = additional_sense;
}

void IDEInterface::atapiRead(std::vector<uint8_t> cmd) {
    uint32_t lba = cmd[2] << 24 | cmd[3] << 16 | cmd[4] << 8 | cmd[5];
    uint32_t count = (cmd[0] == ATAPI_CMD_READ_12) ? (cmd[6] << 24 | cmd[7] << 16 | cmd[8] << 8 | cmd[9]) : (cmd[7] << 8 | cmd[8]);
    uint8_t flags = cmd[1];
    uint32_t byte_count = count * sector_size;
    uint32_t start = lba * sector_size;
    data_length = 0;
    uint32_t req_length = lba_high_reg << 8 & 0xFF00 | lba_mid_reg & 0xFF;
    if (req_length == 0) req_length = 0x10000; // handle wrap
    lba_mid_reg = lba_high_reg = 0;
    if (req_length == 0xFFFF)
        req_length--;

    if (req_length > byte_count)
    {
        req_length = byte_count;
    }

    if (!buffer)
    {
        status_reg = 0xFF;
        error_reg = 0x41;
        pushIrq();
    }
    else if (start >= buffer_size)
    {
        status_reg = 0xFF;
        pushIrq();
    }
    else if (byte_count == 0)
    {
        status_reg = ATA_SR_DRDY | ATA_SR_DSC|ATA_SR_DRQ;
        data_pointer = 0;
        pushIrq();
    }
    else
    {
        byte_count = min(byte_count, buffer_size - start);
        status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_BSY;
        readBuffer(start, byte_count, [&](const uint8_t* data)
        {
            dataSet(data, byte_count);
            status_reg = ATA_SR_DRDY | ATA_SR_DSC | ATA_SR_DRQ;
            sector_count_reg = sector_count_reg & ~7 | 2;

            pushIrq();

            req_length &= ~3;

            data_end = req_length;
            if (data_end > data_length)
            {
                data_end = data_length;
            }
            lba_mid_reg = data_end & 0xFF;
            lba_high_reg = data_end >> 8 & 0xFF;

            reportReadEnd(byte_count);
        });
    }
}

void IDEInterface::atapiReadDma(const uint8_t* cmd) {
    // ATAPI DMA read implementation
}

void IDEInterface::doAtapiDma() {
    if ((channel->dma_status & 1) == 0) {
        return;
    }

    if ((status_reg & ATA_SR_DRQ) == 0) {
        return;
    }

    // DMA transfer implementation
}

void IDEInterface::createIdentifyPacket() {

    auto strcpy_be16 = [&](uint8_t* out_buffer, size_t ofs16, size_t len16, const std::string& str) {
        size_t ofs8 = ofs16 << 1;
        size_t len8 = len16 << 1;
        size_t end8 = ofs8 + len8;

        std::fill(out_buffer + ofs8, out_buffer + end8, 32);

        size_t pos = ofs8;

        for (size_t i = 0; i < str.size() && pos < end8; i++)
        {
            if (i & 1)
            {
                out_buffer[pos] = static_cast<uint8_t>(str[i]);
                pos += 2;
            }
            else
            {
                if (pos + 1 < end8)
                    out_buffer[pos + 1] = static_cast<uint8_t>(str[i]);
            }
        }
    };
    uint32_t cyl_count = min(16383U, cylinder_count);

    std::fill(data.begin(), data.begin() + 512, 0);
    const uint16_t major_version = 0x0000;
    const uint16_t feat_82 = is_atapi ? 1 << 14 | 1 << 9 | 1 << 5 : 1 << 14;
    const uint16_t feat_83 = is_atapi ? 1 << 14 | 1 << 12 : 1 << 14 | 1 << 13 | 1 << 12 | 1 << 10;
    const uint16_t feat_84 = is_atapi ? 1 << 14 : 1 << 14;
    // -----------------------------
// Word 0 – General configuration
// -----------------------------
    uint16_t general_cfg = is_atapi ? 0x8540 : 0x0040;
    data[0] = general_cfg & 0xFF;
    data[1] = (general_cfg >> 8) & 0xFF;

    // -----------------------------
    // Word 1 – Cylinder count
    // -----------------------------
    data[2] = cyl_count & 0xFF;
    data[3] = (cyl_count >> 8) & 0xFF;

    // Word 2 reserved already zero

    // -----------------------------
    // Word 3 – Head count
    // -----------------------------
    data[6] = head_count & 0xFF;
    data[7] = (head_count >> 8) & 0xFF;

    // -----------------------------
    // Word 4 – Unformatted bytes/track
    // -----------------------------
    uint16_t unformatted_track = sectors_per_track / 512;
    data[8] = unformatted_track & 0xFF;
    data[9] = (unformatted_track >> 8) & 0xFF;

    // -----------------------------
    // Word 5 – Unformatted bytes/sector
    // -----------------------------
    data[10] = 0;
    data[11] = 512 >> 8;

    // -----------------------------
    // Word 6 – Sectors per track
    // -----------------------------
    data[12] = sectors_per_track & 0xFF;
    data[13] = (sectors_per_track >> 8) & 0xFF;

    // Words 7–9 vendor unique already zero

    // -----------------------------
    // Word 20 – Buffer type
    // -----------------------------
    data[40] = 3;
    data[41] = 0;

    // -----------------------------
    // Word 21 – Buffer size
    // -----------------------------
    data[42] = 0;
    data[43] = 2;

    // -----------------------------
    // Word 22 – ECC bytes
    // -----------------------------
    data[44] = 4;
    data[45] = 0;

    // -----------------------------
    // Word 47 – Max sectors per interrupt
    // -----------------------------
    data[94] = 0x80;
    data[95] = 0;

    // -----------------------------
    // Word 48 – Doubleword I/O
    // -----------------------------
    data[96] = 1;
    data[97] = 0;

    // -----------------------------
    // Word 49 – Capabilities
    // -----------------------------
    data[98] = 0b00000010;
    data[99] = 0;

    // Word 50 reserved

    // -----------------------------
    // Word 51 – PIO timing
    // -----------------------------
    data[102] = 0;
    data[103] = 2;

    // -----------------------------
    // Word 52 – DMA timing
    // -----------------------------
    data[104] = 0;
    data[105] = 0;

    // -----------------------------
    // Word 53 – Fields valid
    // -----------------------------
    data[106] = 0x03;
    data[107] = 0;

    // -----------------------------
    // Word 54 – Current cylinders
    // -----------------------------
    data[108] = cyl_count & 0xFF;
    data[109] = (cyl_count >> 8) & 0xFF;

    // -----------------------------
    // Word 55 – Current heads
    // -----------------------------
    data[110] = head_count & 0xFF;
    data[111] = (head_count >> 8) & 0xFF;

    // -----------------------------
    // Word 56 – Current sectors/track
    // -----------------------------
    data[112] = sectors_per_track;
    data[113] = 0;

    // -----------------------------
    // Word 57–58 – Current capacity
    // -----------------------------
    data[114] = sector_count & 0xFF;
    data[115] = (sector_count >> 8) & 0xFF;
    data[116] = (sector_count >> 16) & 0xFF;
    data[117] = (sector_count >> 24) & 0xFF;

    // -----------------------------
    // Word 60–61 – LBA sector count
    // -----------------------------
    data[120] = sector_count & 0xFF;
    data[121] = (sector_count >> 8) & 0xFF;
    data[122] = (sector_count >> 16) & 0xFF;
    data[123] = (sector_count >> 24) & 0xFF;

    // -----------------------------
    // Word 63 – Multiword DMA
    // -----------------------------
    //data[126] = multiword_dma_mode & 0xFF;
    //data[127] = (multiword_dma_mode >> 8) & 0xFF;

    // -----------------------------
    // Word 65–68 – cycle times
    // -----------------------------
    data[130] = 0;
    data[132] = 0;
    data[134] = 0;
    data[136] = 0;

    // -----------------------------
    // Word 80 – ATA major version
    // -----------------------------
    data[160] = major_version & 0xFF;
    data[161] = (major_version >> 8) & 0xFF;

    // -----------------------------
    // Word 82 – Features supported
    // -----------------------------
    data[164] = feat_82 & 0xFF;
    data[165] = (feat_82 >> 8) & 0xFF;

    // Word 83
    data[166] = feat_83 & 0xFF;
    data[167] = (feat_83 >> 8) & 0xFF;

    // Word 84
    data[168] = feat_84 & 0xFF;
    data[169] = (feat_84 >> 8) & 0xFF;

    // Word 85
    data[170] = feat_82 & 0xFF;
    data[171] = (feat_82 >> 8) & 0xFF;

    // Word 86
    data[172] = feat_83 & 0xFF;
    data[173] = (feat_83 >> 8) & 0xFF;

    // Word 87
    data[174] = feat_84 & 0xFF;
    data[175] = (feat_84 >> 8) & 0xFF;

    data[176] = 0x00;
    data[177] = 0x00;
    // -----------------------------
    // Word 93 – Hardware reset
    // -----------------------------
    data[186] = 1;
    data[187] = 0x60;

    // -----------------------------
    // Word 100–101 – 48bit LBA
    // -----------------------------
    data[200] = sector_count & 0xFF;
    data[201] = (sector_count >> 8) & 0xFF;
    data[202] = (sector_count >> 16) & 0xFF;
    data[203] = (sector_count >> 24) & 0xFF;


    // -------------------------------------------------
    // Strings (ATA strings are big-endian per word)
    // -------------------------------------------------
    strcpy_be16(data.data(), 10, 10,
        "8086-86" + std::to_string(channel_nr) + std::to_string(interface_nr));

    strcpy_be16(data.data(), 23, 4, "1.00");

    strcpy_be16(data.data(), 27, 20,
        is_atapi ? "TFD ATAPI CD-ROM" : "TFD ATA HD");


    data_length = 512;
    data_end = 512;
    data_pointer = 0; 
    
}

// IDEChannel implementation
IDEChannel::IDEChannel(IDEController* controller, uint8_t channel_nr,
    const IDEDeviceConfig* channel_config,
    uint32_t command_base, uint32_t control_base, uint8_t irq)
    : controller(controller), channel_nr(channel_nr), command_base(command_base),
    control_base(control_base), irq(irq), device_control_reg(ATA_CR_NIEN),
    prdt_addr(0), dma_status(0), dma_command(0)
{
    name = "ide" + std::to_string(channel_nr);

    const IDEDeviceConfig* master_cfg = channel_config ? &channel_config[0] : nullptr;
    const IDEDeviceConfig* slave_cfg = channel_config ? &channel_config[1] : nullptr;

    master = new IDEInterface(this, 0,
        master_cfg ? master_cfg->buffer : nullptr,
        master_cfg ? master_cfg->buffer_size : 0,
        master_cfg ? master_cfg->is_cdrom : false);
    slave = new IDEInterface(this, 1,
        slave_cfg ? slave_cfg->buffer : nullptr,
        slave_cfg ? slave_cfg->buffer_size : 0,
        slave_cfg ? slave_cfg->is_cdrom : false);

    current_interface = master;
}

uint32_t IDEChannel::readStatus() {
    return current_interface->drive_connected ? current_interface->status_reg : 0;
    //uint8_t st = current_interface->status_reg;
    //return st;
}

void IDEChannel::writeControl(uint8_t data) {
    if (data & ATA_CR_SRST) {
        LowerIRQ();
        master->deviceReset();
        slave->deviceReset();
    }
    device_control_reg = data;
}

uint32_t IDEChannel::dmaReadAddr() {
    return prdt_addr;
}

void IDEChannel::dmaSetAddr(uint32_t data) {
    prdt_addr = data;
}

uint32_t IDEChannel::dmaReadStatus() {
    return dma_status;
}

void IDEChannel::dmaWriteStatus(uint8_t value) {
    dma_status &= ~(value & 6);
}

uint32_t IDEChannel::dmaReadCommand() {
    return (dmaReadCommand8()) | (dmaReadStatus() << 16);
}

uint8_t IDEChannel::dmaReadCommand8() {
    return dma_command;
}

void IDEChannel::dmaWriteCommand(uint32_t value) {
    dmaWriteCommand8(value & 0xFF);
    dmaWriteStatus((value >> 16) & 0xFF);
}

void IDEChannel::dmaWriteCommand8(uint8_t value) {
    uint8_t old_command = dma_command;
    dma_command = value & 0x09;

    if ((old_command & 1) == (value & 1)) {
        return;
    }

    if ((value & 1) == 0) {
        dma_status &= ~1;
        return;
    }

    dma_status |= 1;

    switch (current_interface->current_command) {
    case ATA_CMD_READ_DMA:
    case ATA_CMD_READ_DMA_EXT:
        current_interface->doAtaReadSectorsDma();
        break;
    case ATA_CMD_WRITE_DMA:
    case ATA_CMD_WRITE_DMA_EXT:
        current_interface->doAtaWriteSectorsDma();
        break;
    case ATA_CMD_PACKET:
        current_interface->doAtapiDma();
        break;
    default:
        dma_status &= ~1;
        dma_status |= 2;
        pushIrq();
        break;
    }
}

void IDEChannel::pushIrq() {
    if (!(device_control_reg & ATA_CR_NIEN)){
        dma_status |= 4;
        if (!IRQRaised.load()) {
            pic.RaiseIRQ(irq);
            IRQRaised.store(true);
        }
    }
}
void IDEChannel::LowerIRQ() {
     IRQRaised.store(false);
}

std::vector<uint64_t> IDEChannel::getState() const {
    std::vector<uint64_t> state(13);
    state[0] = reinterpret_cast<uint64_t>(master);
    state[1] = reinterpret_cast<uint64_t>(slave);
    state[2] = command_base;
    state[3] = irq;
    state[5] = control_base;
    state[7] = 0; // name index
    state[8] = device_control_reg;
    state[9] = prdt_addr;
    state[10] = dma_status;
    state[11] = (current_interface == master) ? 1 : 0;
    state[12] = dma_command;
    return state;
}

void IDEChannel::setState(const std::vector<uint64_t>& state) {
    if (state.size() < 13) return;

    command_base = state[2];
    irq = state[3];
    control_base = state[5];
    device_control_reg = state[8];
    prdt_addr = state[9];
    dma_status = state[10];
    current_interface = state[11] ? master : slave;
    dma_command = state[12];
}

// IDEController implementation
IDEController::IDEController(
    const IDEDeviceConfig config[2][2])
    :primary(nullptr), secondary(nullptr)
{
    bool has_primary = config && config[0][0].buffer;
    bool has_secondary = config && config[1][0].buffer;

    if (has_primary) {
        primary = new IDEChannel(this,0, config[0], 0x1F0, 0x3F6, 14);
    }
    if (has_secondary) {
        secondary = new IDEChannel(this, 1, config[1], 0x170, 0x376, 15);
    }

    // Initialize PCI configuration space

    const uint16_t vendor_id = 0x8086;    // Intel
    const uint16_t device_id = 0x7010;    // 82371SB PIIX3

    PCIDevice::config[0] = vendor_id & 0xFF;
    PCIDevice::config[1] = (vendor_id >> 8) & 0xFF;
    PCIDevice::config[2] = device_id & 0xFF;
    PCIDevice::config[3] = (device_id >> 8) & 0xFF;
    PCIDevice::config[0x04] = 0x01;
    PCIDevice::config[0xa] = 0x1;
    PCIDevice::config[0xb] = 0x1;
    PCIDevice::config[0x9] = 0x0;
    if (has_primary) {
        set_bar(0, 0x1F0 | 0x1, 8);   // primary cmd, 8 bytes IO
        set_bar(1, 0x3F4 | 0x1, 4);   // primary ctl, 4 bytes IO
    }
    if (has_secondary) {
        set_bar(2, 0x170 | 0x1, 8);   // secondary cmd, 8 bytes IO
        set_bar(3, 0x374 | 0x1, 4);   // secondary ctl, 4 bytes IO
    }
    PCIDevice::config[0x20] = 0x0;  // BAR4
    PCIDevice::config[0x24] = 0x0;  // BAR5
}

std::vector<uint64_t> IDEController::getState() const {
    // FIXED: Don't store pointers, store actual state data
    std::vector<uint64_t> state;

    // Store primary channel state
    if (primary) {
        auto primary_state = primary->getState();
        state.insert(state.end(), primary_state.begin(), primary_state.end());
    }
    else {
        // Empty primary state
        state.resize(13, 0);
    }

    // Store secondary channel state
    if (secondary) {
        auto secondary_state = secondary->getState();
        state.insert(state.end(), secondary_state.begin(), secondary_state.end());
    }
    else {
        // Empty secondary state
        state.resize(26, 0);
    }

    return state;
}

void IDEController::setState(const std::vector<uint64_t>& state) {
    // FIXED: Properly restore from serialized state

    if (primary && state.size() >= 13) {
        // Extract primary channel state (first 13 elements)
        std::vector<uint64_t> primary_state(state.begin(), state.begin() + 13);
        primary->setState(primary_state);
    }

    if (secondary && state.size() >= 26) {
        // Extract secondary channel state (next 13 elements)
        std::vector<uint64_t> secondary_state(state.begin() + 13, state.begin() + 26);
        secondary->setState(secondary_state);
    }
}
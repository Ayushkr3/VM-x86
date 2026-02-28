#pragma once
#include <iostream>
#include "io.h"
#include "PIC.h"
uint8_t CMOS::cmos_data[128];
uint8_t CMOS::cmos_index = 0;
IO ports;
//Forwarded declaration from VM.CPP to stop emulation
StopEmulationFWD StopEmulation_fwd;
//Forwarded declaration to get eflags (wether to deliver int or not)
GetEflagsFWD GetEflags_fwd;
void init_cmos() {
    CMOS::cmos_data[0x15] = 0x80;  // low byte (640 = 0x280)
    CMOS::cmos_data[0x16] = 0x02;  // high byte
    CMOS::cmos_data[0x17] = 0x00;  // low (0xFC00)
    CMOS::cmos_data[0x18] = 0xFC;  // high

    // Mirror at 0x30/0x31
    CMOS::cmos_data[0x30] = 0x00;
    CMOS::cmos_data[0x31] = 0xFC;
    CMOS::cmos_data[0x34] = 0x00;  // low (0x7F00)
    CMOS::cmos_data[0x35] = 0x7F;  // high
    CMOS::cmos_data[0x5B] = 0x00;
    CMOS::cmos_data[0x5C] = 0x00;
    CMOS::cmos_data[0x5D] = 0x00;

    // ── RTC ──
    CMOS::cmos_data[0x0A] = 0x26;  // RTC status A
    CMOS::cmos_data[0x0B] = 0x02;  // RTC status B (24h mode)
    CMOS::cmos_data[0x0C] = 0x00;
    CMOS::cmos_data[0x0D] = 0x80;  // RTC valid

    // ── Equipment ──
    CMOS::cmos_data[0x14] = 0x25;  // color VGA, no floppy

    // ── Hard Disk Configuration ──
    //CMOS::cmos_data[0x11] = 0x00;  // Reserved
    //CMOS::cmos_data[0x12] = 0x2F;  // HD types (0 = use extended type)
    //CMOS::cmos_data[0x13] = 0x00;  // Reserved
    //CMOS::cmos_data[0x19] = 0x2F;  // HD 0 extended type (0 = not installed)
    //CMOS::cmos_data[0x1A] = 0x00;  // HD 1 extended type (0 = not installed)

    //// ── Hard Disk Parameters (0x1B-0x2C) ──
    //// All zeros for no configured drives or modern IDE detection
    //CMOS::cmos_data[0x1B] = 0x00;  // Cylinders low
    //CMOS::cmos_data[0x1C] = 0x04;  // Cylinders high (1024 cyls)
    //CMOS::cmos_data[0x1D] = 0x10;  // Heads (16)
    //CMOS::cmos_data[0x1E] = 0x00;  // Write precomp low
    //CMOS::cmos_data[0x1F] = 0x00;  // Write precomp high
    //CMOS::cmos_data[0x20] = 0x00;  // Control byte
    //CMOS::cmos_data[0x21] = 0x00;  // Landing zone low
    //CMOS::cmos_data[0x22] = 0x04;  // Landing zone high
    //CMOS::cmos_data[0x23] = 0x3F;  // Sectors per track (63)

    // ── Misc ──
    CMOS::cmos_data[0x2D] = 0x00;  // no floppy boot
    CMOS::cmos_data[0x3D] = 0x03;  // Boot from hard disk
    CMOS::cmos_data[0x38] = 0x00;  // No special boot flags
}
void LoadDisk(std::string path, DISK_TYPE type) {
    std::ifstream* iso = new std::ifstream;
    iso->open(path, std::ios::binary);
    ports.IO_P_B[type] = iso;
}
void DoAllHardwareInt(uc_engine* eng) {
    uint32_t eflags;
    uc_reg_read(eng, UC_X86_REG_EFLAGS, &eflags);
    if (eflags & (1 << 9)&&irq.irqRaised) {
        uc_protected_int_call(eng, irq.irqNum);
        irq.irqRaised.store(false);
    }
}
void RaiseIRQN(int intN) {
    EnterCriticalSection(&irq.cs);
    //This can be race condition ,we will always get stale eflags
    StopEmulation_fwd();
    //uint32_t eflags = GetEflags_fwd();
    //if (eflags & (1 << 9)) {
    irq.SetIRQ(intN);
    //}
    LeaveCriticalSection(&irq.cs);
}
void InitIO(StopEmulationFWD sfwd, GetEflagsFWD gfwd) {
    //Forwarded declaration from PIC.h and APIC.h
    RaiseIRQ_fwd = RaiseIRQN;
    GetEflags_fwd = gfwd;
    //Forwarded declaration from VM.CPP to stop emulation
    StopEmulation_fwd = sfwd;
    init_cmos();
}
void ReadIODisk(int index, long long DAP,uc_engine* uc) {
    std::ifstream* disk = ports.IO_P_B[index];
    char daps[16];
    memcpy(daps, RAM + DAP, 16);
    uint8_t  size = daps[0];
    uint8_t  reserved = daps[1];
    uint16_t sectors = *(uint16_t*)&daps[2];
    uint32_t buffer = *(uint32_t*)&daps[4];
    uint64_t lba = *(uint64_t*)&daps[8];
    int sectorSize = 0;
    uint16_t off = *(uint16_t*)&daps[4];
    uint16_t segment = *(uint16_t*)&daps[6];
    if (index == DISK_TYPE::DVD) {
        sectorSize = 2048;
    }
    char* data = (char*)malloc(sectors * sectorSize);
    disk->seekg(lba * sectorSize, std::ios::beg);
    disk->read(data, sectorSize * sectors);
    uint64_t addr = (segment * 16) + (off);
    uc_err err = uc_mem_write(uc, (segment * 16) + (off), data, sectorSize * sectors);
    uc_ctl_remove_cache(uc, (segment * 16) + (off), ((segment * 16) + (off)) + sectorSize * sectors);
    free(data);
}

void BootDisk() {
    std::ifstream* iso = ports.IO_P_B[0xe0];
    if (iso == nullptr) {
        return;
    }
    char sector[2048];
    iso->seekg(2048 * 0x11, std::ios::beg);
    iso->read(sector, 2048);
    uint32_t bootCatalogLBA = *reinterpret_cast<uint32_t*>(sector + 0x47);
    iso->seekg(bootCatalogLBA * 2048, std::ios::beg);
    iso->read(sector, 2048);
    uint32_t lba = *(uint32_t*)&sector[0x28];
    iso->seekg(lba * 2048, std::ios::beg);
    iso->read(sector, 2048);
    memcpy(RAM + 0x7c00, sector, 2048);
}
void RemoveDisk(int index) {
    if (ports.IO_P_B[index] != nullptr) {
        ports.IO_P_B[index]->close();
        delete ports.IO_P_B[index];
        ports.IO_P_B[index] = nullptr;
    }
}
uint32_t hook_in(uc_engine* uc, uint32_t port, int size, void* user_data) {
    HookUserData* hud = (HookUserData*)user_data;
    PICState* pic = (PICState*)hud->pic;
    if ((port & 0xFF00) >= 0x0C00 && (port & 0xFF00) <= 0x0F00) {
        // Type 2 PCI config - return "no device present"
        return 0xFF;  // or 0xFFFFFFFF for 32-bit reads
    }
    switch (port) {
        // 8259 PIC
    case 0x20: return pic->master_icw_state; // master PIC status
    case 0x21: return pic->master_mask; // master PIC mask (all masked)
    case 0xA0: return pic->slave_icw_state; // slave PIC status
    case 0xA1: return pic->slave_mask; // slave PIC mask

    // 8254 PIT
    case 0x40: return 0x00;
    case 0x41: return 0x00;
    case 0x42: return 0x00;
    case 0x43: return 0x00;
    case 0x61: return 0xff;
        // CMOS
    case 0x70: return CMOS::cmos_index;
    case 0x71: {
        switch (CMOS::cmos_index)
        {
        case 0x00:
            return 0x10;
        case 0x02:
            return 0x02;
        case 0x04:
            return 0x18;
        case 0x06:
            return 0x02;  // Monday
        case 0x07:
            return 0x09;
        case 0x08:
            return 0x02;  // February
        case 0x09:
            return 0x26;  // 2026
        case 0x0A:
            return 0x26;  // 32.768kHz, normal operation
        case 0x0B:
            return 0x02;
        case 0x0C:
            return 0x00;
        case 0x0D:
            return 0x80;
        case 0x0E:
            return 0x00;
        case 0x0F:
            return 0x00;
        case 0x10:
            return 0x00;
        case 0x12:
            return 0xF0;
        case 0x14:
            return 0x05;
        case 0x15:
            return 0x80;
        case 0x16:
            return 0x02;
        case 0x17:
            return 0x00;
        case 0x18:
            return 0x7F;
        case 0x30:
            return 0x00;
        case 0x31:
            return 0x7F;
        case 0x32:
            return 0x20;

        case 0x2D:
            return 0x0F;
        case 0x2E:
        case 0x2F:
        case 0x3D:
            return 0x01;
        default: {
            uint8_t val = (CMOS::cmos_index < 128) ? CMOS::cmos_data[CMOS::cmos_index] : 0;
            std::cout << "[CMOS] read " << std::hex << (uint32_t)CMOS::cmos_index << std::endl;
            return val;
        }
        }
    }
             // POST/diagnostic
    case 0x80: return 0x00;

        // COM1 serial (line status: TX empty)
    case 0x3F8: return 0x00;
    case 0x3FD: return 0x60; // THRE + TEMT bits set

    // PS/2 keyboard controller
    case 0x60: return 0x00;
    case 0x64: return 0x10; // input buffer empty

    // PCI config space
    case 0xCF8: return 0x00;
    case 0xCFC: return 0xFF;

        // VGA
    case 0x3C0: case 0x3C1: case 0x3C2: case 0x3C4:
    case 0x3C5: case 0x3CE: case 0x3CF: case 0x3D4:
    case 0x3D5: case 0x3DA: return 0x00;
    
    default:
        printf("[IN ] port=0x%04X size=%d\n", port, size);
        return 0x00;
    }
}

void hook_out(uc_engine* uc, uint32_t port, int size, uint32_t value, void* user_data) {
    HookUserData* hud = (HookUserData*)user_data;
    PICState* pic = (PICState*)hud->pic;
    switch (port) {
        // POST codes
    case 0x80:
        printf("[POST] 0x%02X\n", value & 0xFF);
        break;

        // CMOS
    case 0x70:
        CMOS::cmos_index = value & 0x7F;
        break;
    case 0x71:
        switch (CMOS::cmos_index) {
        case 0xA: {
            uint8_t rs = value & 0x0F;
            pic->rtc_period = std::pow(2.0, rs - 1) / 32768.0;
            break;
        }
        case 0xB:
            pic->rtc_period_enabled = (value >> 6) & 1;
            break;
        }
        if (CMOS::cmos_index < 128) CMOS::cmos_data[CMOS::cmos_index] = value & 0xFF;
        printf("[CMOS] write [0x%02X] = 0x%02X\n", CMOS::cmos_index, value & 0xFF);
        break;

        // COM1 serial output (character output)
    case 0x3F8:
        if (size == 1 && value >= 0x20 && value < 0x7F)
            printf("%c", value);
        else
            printf("%c", value & 0xFF);
        break;

        // 8259 PIC
    case 0x20:   // PIC1 Command
        if (value & 0x10) {
            pic->master_icw_state = 1;
        }
        break;
    case 0x21:   // PIC1 Data
        if (pic->master_icw_state == 1) {
            pic->master_offset = value;
            pic->master_icw_state = 2;
            std::cout << "PIC1: Base = 0x"
                << std::hex << (int)value << std::endl;
        }
        else if (pic->master_icw_state == 2) {
            pic->master_icw_state = 3; // ICW3
        }
        else if (pic->master_icw_state == 3) {
            pic->master_icw_state = 0; // ICW4 done
            std::cout << "PIC1: Init complete\n";
        }
        else {
            pic->master_mask.store(value);
            std::cout << "PIC1: Mask = 0x"
                << std::hex << (int)value << std::endl;
        }
        break;
    case 0xA0:   // PIC2 Command
        if (value & 0x10) {
            pic->slave_icw_state = 1;
            std::cout << "PIC2: ICW1\n";
        }
        else if (value == 0x20) {
            std::cout << "PIC2: EOI\n";
        }
        else {
            std::cerr << "Err: Invalid PIC2 command\n" << std::hex << value << std::endl;
        }
        break;

    case 0xA1:   // PIC2 Data
        if (pic->slave_icw_state == 1) {
            pic->slave_offset = value;
            pic->slave_icw_state = 2;
            std::cout << "PIC2: Base = 0x"<< std::hex << (int)value << std::endl;
        }
        else if (pic->slave_icw_state == 2) {
            pic->slave_icw_state = 3;
        }
        else if (pic->slave_icw_state == 3) {
            pic->slave_icw_state = 0;
            std::cout << "PIC2: Init complete\n";
        }
        else {
            pic->slave_mask.store(value);
            std::cout << "PIC2: Mask = 0x"<< std::hex << (int)value << std::endl;
        }
        break;
    case 0x40: case 0x41: case 0x42: case 0x43:
        break;
    case 0x60: case 0x64:
        break;
    case 0xCF8: case 0xCFC:
        break;
    case 0x3C0: case 0x3C2: case 0x3C4: case 0x3C5:
    case 0x3CE: case 0x3CF: case 0x3D4: case 0x3D5:
        break;
    case 0x402:
        //printf("%c",value);
        break;
    case 0x92:

        break;
    case 0x61: ports.portsVal[port] = value;
        break;
    default:
        printf("[OUT] port=0x%04X size=%d val=0x%X\n", port, size, value);
        break;
    }
}

void hook_intr(uc_engine* uc, uint32_t intno, void* user_data) {
    HookUserData* hud = (HookUserData*)user_data;
    PICState* pic = (PICState*)hud->pic;
    uint32_t eax, ebx, ecx, edx,eflags,ds,esi;
    uc_reg_read(uc, UC_X86_REG_EAX, &eax);
    uc_reg_read(uc, UC_X86_REG_EBX, &ebx);
    uc_reg_read(uc, UC_X86_REG_ECX, &ecx);
    uc_reg_read(uc, UC_X86_REG_EDX, &edx);
    uc_reg_read(uc, UC_X86_REG_EFLAGS, &eflags);
    uc_reg_read(uc, UC_X86_REG_DS, &ds);
    uc_reg_read(uc, UC_X86_REG_ESI, &esi);

    uint8_t* ah = reinterpret_cast<uint8_t*>(&eax) + 1; //type of access
    uint8_t* al = reinterpret_cast<uint8_t*>(&eax); //Number of sectors to read (1–127)
    uint8_t* bh = reinterpret_cast<uint8_t*>(&ebx) + 1; //type of access
    uint8_t* bl = reinterpret_cast<uint8_t*>(&ebx); //Number of sectors to read (1–127)
    uint8_t* ch = reinterpret_cast<uint8_t*>(&ecx) + 1; //Cylinder number(low 8 bits)
    uint8_t* cl = reinterpret_cast<uint8_t*>(&ecx); //Sector number
    uint8_t* dl = reinterpret_cast<uint8_t*>(&edx); // drive address
    uint8_t* dh = reinterpret_cast<uint8_t*>(&edx) + 1; //Head number
    switch (intno) {
    case 0x10: // VGA BIOS
        if (*ah == 0x0E) {
            // teletype output
            printf("%c", *al);
            fflush(stdout);
        }
        else {
            printf("[INT10] AH=0x%02X AL=0x%02X\n", *ah, *al);
        }
        break;

    case 0x11: // Equipment check — return basic equipment word
        eax = (eax & 0xFFFF0000) | 0x0021;
        uc_reg_write(uc, UC_X86_REG_EAX, &eax);
        break;

    case 0x12: // Memory size — return 640KB
        eax = (eax & 0xFFFF0000) | 0x0280;
        uc_reg_write(uc, UC_X86_REG_EAX, &eax);
        break;
    case 0x15: // System services
        printf("[INT15] AX=0x%04X\n", eax & 0xFFFF);
        uc_emu_stop(uc);
        if (*ah == 0xe8) {
            if (*al == 0x20) {
                uc_protected_int_call(uc, 0x15);
                uint32_t eip, cs;
                uc_reg_read(uc, UC_X86_REG_EIP, &eip);
                uc_reg_read(uc, UC_X86_REG_CS, &cs);
            }
        }
        else if (*ah == 0x53) {
            if (*al == 0x0) {
                *ah = 0x1;
                *al = 0x2;
                *bh = 'P';
                *bl = 'M';
                uint16_t cx = 0x3;
                uc_reg_write(uc, UC_X86_REG_AH, ah);
                uc_reg_write(uc, UC_X86_REG_AL, al);
                uc_reg_write(uc, UC_X86_REG_BH, bh);
                uc_reg_write(uc, UC_X86_REG_BL, bl);
                uc_reg_write(uc, UC_X86_REG_CX, &cx);
                eflags &= ~0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            else if (*al == 0x1) {
                eflags &= ~0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            else if (*al == 0x2) {
                uint16_t ax = 0xf000;
                uint16_t si = 0xfff0;
                uint16_t bx = 0x0;
                uint16_t cx = 0xf000;
                uint16_t di = 0xfff0;
                uc_reg_write(uc, UC_X86_REG_AX, &ax);
                uc_reg_write(uc, UC_X86_REG_SI, &si);
                uc_reg_write(uc, UC_X86_REG_BX, &bx);
                uc_reg_write(uc, UC_X86_REG_DI, &di);
                uc_reg_write(uc, UC_X86_REG_CX, &cx);
                eflags &= ~0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            else if (*al == 0x4) {
                eflags &= ~0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            else if (*al == 0xe) {
                *ah = 0x1;
                *al = 0x2;
                uc_reg_write(uc, UC_X86_REG_AH, ah);
                uc_reg_write(uc, UC_X86_REG_AL, al);
                eflags &= ~0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
        }
        break;

    case 0x19: { // Boot — just halt
        printf("[INT19] Boot interrupt — halting\n");
        uint32_t EDX = 0xe0;
        uint32_t cr0 = 0x10; {
            uc_reg_write(uc, UC_X86_REG_DX, &EDX);
            uc_reg_write(uc, UC_X86_REG_CR0, &cr0);
            int flag = 0x02;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &flag);
        }
        uint32_t ip=0x7c00;
        uint32_t cs =0x0;
        uc_emu_stop(uc);
        uc_reg_write(uc, UC_X86_REG_EIP, &ip);
        uc_reg_write(uc, UC_X86_REG_CS, &cs);
        pic->slave_mask = 0xfc;
        pic->master_mask = 0xfc;
        pic->master_offset = 0x20;
        pic->slave_offset = 0x1c;
        pic->rtc_period_enabled |= 0x1;
        BootDisk();
        break;
    }
    case 0x1a: {
        switch (*ah) {
        case 0x00: { // Read system clock counter
            uint64_t ms_since_boot = get_ms_since_midnight();
            uint32_t ticks = (uint32_t)(ms_since_boot / 54.9254);
            uint16_t cx = (ticks >> 16) & 0xFFFF;
            uint16_t dx = ticks & 0xFFFF;
            *al = (ticks >= 1573040) ? 1 : 0;
            *ah = 0;
            uc_reg_write(uc, UC_X86_REG_CX, &cx);
            uc_reg_write(uc, UC_X86_REG_DX, &dx);
            uc_reg_write(uc, UC_X86_REG_AL, al);
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            eflags &= ~0x1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        case 0x2: {
            HostDatenTime tim = get_host_time();
            *ch = to_bcd(tim.hour);
            *cl = to_bcd(tim.minute);
            *dh = to_bcd(tim.second);
            uc_reg_write(uc, UC_X86_REG_CH, ch);
            uc_reg_write(uc, UC_X86_REG_CL, cl);
            uc_reg_write(uc, UC_X86_REG_DH, dh);
            uc_reg_write(uc, UC_X86_REG_AH, 0);
            eflags &= ~0x1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        }
        break;
    }
    case 0xD: {
        uint32_t ip = 0x7c00;
        uint32_t cs = 0x0;
        uc_reg_read(uc, UC_X86_REG_EIP, &ip);
        uc_reg_read(uc, UC_X86_REG_CS, &cs);
        PRINT_HEX(ip);
        PRINT_HEX(cs);
        break;
    }
    case 0x16: {
        if (*ah == 0x1) {
            //This is partial imeplementation
            //TODO: implement actual implementation when the buffer is not zero
            eflags |= (1 << 6);
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
        }
        break;
    }
    case 0x13: {
        switch (*ah)
        {
        case 0x42: {
            //long long dap = (long long)(((uint16_t)in.pcpu->DS * 16) + (uint16_t)in.pcpu->RSI);
            long long dap = (long long)(((uint16_t)ds* 0x10) + (uint16_t)esi);
            ReadIODisk(*dl, dap, uc);
            *ah = 0;
            eflags &= ~(1 << 0);
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        case 0x2:
        {
            //ReadHDD(*ch, *dh,*al, *cl,*dl, loc,ud);
            *ah = 0x101;
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            eflags |= 1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        case 0x08: {
            uint16_t cylinders = 1023;   // CHS limit
            uint8_t heads = 16;
            uint8_t sectors = 63;
            *dl = 0;
            *ah = 0;
            eflags &= ~1; // clear CF
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            uc_reg_write(uc, UC_X86_REG_DL, dl);
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            break;
        }
        case 0x4b: {
            *ah = 0;
            *al = 0;
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            uc_reg_write(uc, UC_X86_REG_AL, al);
            eflags &= ~1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        case 0x41: {
            uint32_t cx = 0x0007;
            *ah = 0;
            uc_reg_write(uc, UC_X86_REG_CX, &cx);
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            eflags &= ~0x1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);

            break;
        }
        case 0xd:
            *ah = 0;
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            eflags &= ~1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        case 0x15: {
            if (*dl >= 0x80) {
                *ah = 0x3;
                uint32_t dl = 0x1;
                uc_reg_write(uc, UC_X86_REG_AH, ah);
                uc_reg_write(uc, UC_X86_REG_DL, &dl);
                eflags &= ~1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            else {
                *ah = 0x2;
                uc_reg_write(uc, UC_X86_REG_AH, ah);
                uint32_t dl = 0x1;
                uc_reg_write(uc, UC_X86_REG_DL, &dl);
                eflags &= 0x1;
                uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            }
            break;
        }
        case 0x0: {
            *ah = 0;
            uc_reg_write(uc, UC_X86_REG_AH, ah);
            eflags &= ~0x1;
            uc_reg_write(uc, UC_X86_REG_EFLAGS, &eflags);
            break;
        }
        default:
            std::cout << "unhandled interupt " << intno << "read  " << ah << std::endl;
            break;
        }
        break;
    }
    default:
        printf("[INTR] int=0x%02X AX=0x%04X\n", intno, eax & 0xFFFF);
        break;
    }
}
void hook_wrmsr(uc_engine* uc, void* user_data) {
    uint32_t eax;
    uint32_t edx;
    uint32_t ecx;
    uc_reg_read(uc, UC_X86_REG_EAX, &eax);
    uc_reg_read(uc, UC_X86_REG_EDX, &edx);
    uc_reg_read(uc, UC_X86_REG_ECX, &ecx);
    uint32_t eip;
    uc_reg_read(uc, UC_X86_REG_EIP, &eip);
    if (ecx != 0x8b) {
        std::cout << "WRMSR " << std::hex << eip << std::endl;
    }
}
void hook_rdmsr(uc_engine* uc, void* user_data) {
    uint32_t eax;
    uint32_t edx;
    uint32_t ecx;
    uc_reg_read(uc, UC_X86_REG_EAX, &eax);
    uc_reg_read(uc, UC_X86_REG_EDX, &edx);
    uc_reg_read(uc, UC_X86_REG_ECX, &ecx);
    std::cout << "RDMSR " << std::hex << ecx << std::endl;
}
void hook_cpuid(uc_engine* uc, void* user_data) {
    std::cout << "cpuid";
}
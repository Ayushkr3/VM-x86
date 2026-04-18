#pragma once
#include <iostream>
#include "io.h"
uint8_t CMOS::cmos_data[128];
uint8_t CMOS::cmos_index = 0;
std::unordered_map<uint64_t, uint64_t> MSR::msr_values;
uint32_t A20 = 0;
IO ports;
FWCfg fw_cfg;
UD ud;
//Forwarded declaration from VM.CPP to stop emulation
//Forwarded declaration to get eflags (wether to deliver int or not)
GetEflagsFWD GetEflags_fwd;
void init_cmos() {
    CMOS::cmos_data[0x15] = 0x80; // base 640KB low
    CMOS::cmos_data[0x16] = 0x02;
    CMOS::cmos_data[0x17] = 0xFF;
    CMOS::cmos_data[0x18] = 0xFF;
    CMOS::cmos_data[0x30] = 0x00;
    CMOS::cmos_data[0x31] = 0xFC;
    CMOS::cmos_data[0x34] = 0x00; // above 16MB in 64KB units = 0x7F00
    CMOS::cmos_data[0x35] = 0x7F;
    CMOS::cmos_data[0x5B] = 0x00; // nothing above 4GB
    CMOS::cmos_data[0x5C] = 0x00;
    CMOS::cmos_data[0x5D] = 0x00;

    // ── Equipment ──
    CMOS::cmos_data[0x14] = 0x0;  // color VGA, no floppy
    CMOS::cmos_data[0x10] = 0x00;

    // ── Misc ──
    CMOS::cmos_data[0x2D] = 0x00;  // no floppy boot
    //CMOS::cmos_data[0x3D] = 0x31;  // Boot from hard disk
    CMOS::cmos_data[0x38] = 0x00;  // No special boot flags
}

void InitIDE(PCISystemBus* sb, std::string isoPath) {
    IDEDeviceConfig config[2][2] = {};
    //TODO: Delete this later
    std::fstream* hd = new std::fstream;
    hd->open("D:\\windows nt\\disk0.diskX", std::ios::in | std::ios::out | std::ios::binary);
    config[0][0].buffer = hd;
    config[0][0].is_cdrom = false;
    hd->seekg(0, std::ios::end);
    config[0][0].buffer_size = hd->tellg();
    hd->seekg(0, std::ios::beg);

    config[0][1].buffer = nullptr;
    config[0][1].is_cdrom = false;
    config[0][1].buffer_size = 0;

    config[1][0].buffer = nullptr;
    config[1][0].is_cdrom = false;
    config[1][0].buffer_size = 0;

    if (isoPath != "") {
        std::fstream* iso = new std::fstream;
        iso->open(isoPath, std::ios::in | std::ios::binary);
        config[1][0].buffer = iso;
        config[1][0].is_cdrom = true;
        iso->seekg(0, std::ios::end);
        config[1][0].buffer_size = iso->tellg();
        iso->seekg(0, std::ios::beg);
    }
    sb->ID = new IDEController(config);
    sb->AttachDevice((PCIDevice*)(sb->ID));
}
void InitIO(PCISystemBus* sb, RaiseIRQ_f rfwd, std::string isoPath) {
    PICState::Init(rfwd);
    Timers::Init();
    InitIDE(sb, isoPath);
    init_cmos();
    KernelDebugger::com1_init_pipe([]() {
        pic.RaiseIRQ(0x4);
        });
    ud.kbd = new PS2Keyboard([rfwd](int irq) {
        pic.RaiseIRQ(irq);
        });
    PS2Keyboard* kbd = ud.kbd;
    sb->vgaC = new VGAController;
    sb->AttachDevice((PCIDevice*)sb->vgaC);
}
uint32_t hook_in(uint16_t port, int size, void* user_data) {
    UD* ud = (UD*)user_data;
    PICState* pic = (PICState*)ud->pic;
    PCISystemBus* sb = (PCISystemBus*)ud->sb;
    uint32_t return_val;
    switch (port) {
        // 8259 PIC
    case 0x20: return_val = pic->master_icw_state; break; // master PIC status
    case 0x21: return_val = pic->master_mask; break; // master PIC mask (all masked)
    case 0xA0: return_val = pic->slave_icw_state; break; // slave PIC status
    case 0xA1: return_val = pic->slave_mask; break; // slave PIC mask

    // 8254 PIT
    case 0x40: return_val = 0x00; break;
    case 0x41: return_val = 0x00; break;
    case 0x42: return_val = 0x00; break;
    case 0x43: return_val = 0x00; break;
    case 0x61: return_val = 0xff; break;
        // CMOS
    case 0x70: return_val = CMOS::cmos_index; break;
    case 0x71: {
        switch (CMOS::cmos_index)
        {
        case 0x00:
        case 0x02:
        case 0x04:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x32:
        case 0x37:
            return_val = Timers::ReturnCMOSData(CMOS::cmos_index);
            break;
        case 0xA:return_val = Timers::GetCMOSregA(); break;
        case 0xB:return_val = Timers::GetCMOSregB(); break;
        case 0xC:return_val = Timers::GetCMOSregC(); break;
        case 0xD:return_val = Timers::GetCMOSregD(); break;
        case 0x0092: return_val = 0x02; break;
        default: {
            uint8_t val = (CMOS::cmos_index < 128) ? CMOS::cmos_data[CMOS::cmos_index] : 0;
            std::cout << "[CMOS] read " << std::hex << (uint32_t)CMOS::cmos_index << std::endl;
            return_val = val;
            break;
        }
        }
        break;
    }
             // POST/diagnostic
    case 0x80: return_val = 0x00; break;

        // PS/2 keyboard controller

    // PCI config space
    case 0xCF8:
    case 0xCFC:
    case 0xCFD:
    case 0xCFE:
    case 0xCFF:
        return_val = sb->in_hook(port, size);
        break;
        //VGA controller
    case 0x3C0: case 0x3C1: case 0x3C2: case 0x3C3:
    case 0x3C4: case 0x3C5: case 0x3C6: case 0x3C7:
    case 0x3C8: case 0x3C9: case 0x3CA: case 0x3CB:
    case 0x3CC: case 0x3CD: case 0x3CE: case 0x3CF:
    case 0x3D4: case 0x3D5: case 0x3DA:
        return_val = sb->in_hook(port, size); break;
        // Floppy controller ports
    case 0x3F0: return_val = 0x00; break;  // floppy SRA
    case 0x3F2: return_val = 0x00; break;  // floppy DOR
    case 0x3F4: return_val = 0x80; break;  // floppy MSR: RQM=1 (ready), DIO=0, no transfer
    case 0x3F5: return_val = 0x00; break;  // floppy data FIFO
    case 0x3F7: return_val = 0x00; break;  // floppy DIR

    case 0x1F0:
    case 0x1F1:
    case 0x1F2:
    case 0x1F3:
    case 0x1F4:
    case 0x1F5:
    case 0x1F6:
    case 0x1F7:
    case 0x3F6:
    case 0x170:
    case 0x171:
    case 0x172:
    case 0x173:
    case 0x174:
    case 0x175:
    case 0x176:
    case 0x177:
    case 0x376:
    case 0x374:
        return_val = sb->in_hook(port, size);
        break;
    case 0x3d:
        return_val = 0x23;
        break;
    case 0x510:
        return_val = fw_cfg.selector;
        break;
    case 0x511:
        return_val = fw_cfg.read();
        break;
    case 0x92:
        return_val = 0x2;
        break;

    case 0x3F8:
        if (KernelDebugger::lcr & 0x80) {
            return_val = KernelDebugger::baud_lo;
        }
        else {
            KernelDebugger::pipe_poll_rx();
            return_val = KernelDebugger::rx_ready ? KernelDebugger::rx_buf : 0x00;
            KernelDebugger::rx_ready = false;
        }
        break;
    case 0x3F9:
        return_val = (KernelDebugger::lcr & 0x80) ? KernelDebugger::baud_hi : KernelDebugger::ier;
        break;
    case 0x3FA:
        return_val = 0x01;   // IIR: no interrupt pending
        break;
    case 0x3FB:
        return_val = KernelDebugger::lcr;
        break;
    case 0x3FC:
        return_val = KernelDebugger::mcr;
        break;
    case 0x3FD:
        KernelDebugger::pipe_poll_rx();
        return_val = 0x60;                    // THRE + TEMT: TX always ready
        if (KernelDebugger::rx_ready) return_val |= 0x01;     // DR: data ready
        break;
    case 0x3FE:
        return_val = 0xB0;   // MSR: CTS, DSR, DCD all set
        break;
    case 0x3FF:
        return_val = KernelDebugger::scratch;
        break;
    case 0x2f8:
        return_val = 0x0;
        break;
    case 0x2fd:
        return_val = 0x60;
        break;
    case 0x402:
        return_val = Timers::GetPMTimer();
        break;
    case 0x60:
        return_val = ud->kbd->read_data();
        break;
    case 0x64:
        return_val = ud->kbd->read_status();
        break;
    case 0x1CF:
        return_val = sb->in_hook(port, size);
        break;
    default:
        printf("[IN ] port=0x%04X size=%d\n", port, size);
        return_val = 0xff;
    }
    return return_val;
}
void hook_out(uint16_t port, int size, uint32_t value, void* user_data) {
    UD* ud = (UD*)user_data;
    PICState* pic = (PICState*)ud->pic;
    PCISystemBus* sb = (PCISystemBus*)ud->sb;
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
            Timers::rtc_period = std::pow(2.0, rs - 1) / 32768.0;
            break;
        }
        case 0xB:
            Timers::rtc_period_enabled = (value & 0x70) != 0;
            Timers::SetCMOSregB(value);
            break;
        }
        if (CMOS::cmos_index < 128) CMOS::cmos_data[CMOS::cmos_index] = value & 0xFF;
        printf("[CMOS] write [0x%02X] = 0x%02X\n", CMOS::cmos_index, value & 0xFF);
        break;


        // 8259 PIC
    case 0x20:   // PIC1 Command
        if (value & 0x10) {
            pic->master_icw_state = 1;
            pic->master_mask.store(0xFF);
        }
        else if (value == 0x20) {
        }
        else if ((value & 0xF8) == 0x60) {
        }
        else if (value == 0x0A || value == 0x0B) {
        }
        else if ((value & 0xE0) == 0x80) {
        }
        else if ((value & 0xE0) == 0xC0) {
        }
        else if ((value & 0xE0) == 0xA0) {
        }
        else if ((value & 0xF8) == 0x40) {
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
            Timers::UpdateMasks(value, TIMER_MASK_TYPE::PIC1);
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
            //std::cout << "PIC2: EOI\n";
        }
        else {
            std::cerr << "Err: Invalid PIC2 command" << std::hex << value << std::endl;
        }
        break;

    case 0xA1:   // PIC2 Data
        if (pic->slave_icw_state == 1) {
            pic->slave_offset = value;
            pic->slave_icw_state = 2;
            std::cout << "PIC2: Base = 0x" << std::hex << (int)value << std::endl;
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
            Timers::UpdateMasks(value, TIMER_MASK_TYPE::PIC2);
            std::cout << "PIC2: Mask = 0x" << std::hex << (int)value << std::endl;
        }
        break;
    case 0x40: case 0x41: case 0x42: case 0x43:
        break;
    case 0xCF8:
        sb->out_hook(port, value, size);
        break;
    case 0xCFC:
    case 0xCFD:
    case 0xCFE:
    case 0xCFF:
        sb->out_hook(port, value, size);
        break;
        //VGA controller
    case 0x3C0: case 0x3C1: case 0x3C2: case 0x3C3:
    case 0x3C4: case 0x3C5: case 0x3C6: case 0x3C7:
    case 0x3C8: case 0x3C9: case 0x3CA: case 0x3CB:
    case 0x3CC: case 0x3CD: case 0x3CE: case 0x3CF:
    case 0x3D4: case 0x3D5: case 0x3DA:
        sb->out_hook(port, value, size);
        break;
        // Floppy controller ports (silently ignore writes)
    case 0x3F2: break;  // floppy DOR
    case 0x3F4: break;  // floppy MSR (read-only, ignore writes)
    case 0x3F5: break;  // floppy data FIFO
    case 0x3F7: break;  // floppy CCR/DIR

    case 0x1F0:
    case 0x1F1:
    case 0x1F2:
    case 0x1F3:
    case 0x1F4:
    case 0x1F5:
    case 0x1F6:
    case 0x1F7:
    case 0x3F6:
    case 0x170:
    case 0x171:
    case 0x172:
    case 0x173:
    case 0x174:
    case 0x175:
    case 0x176:
    case 0x177:
    case 0x376:
    case 0x374:
        sb->out_hook(port, value, size);
        break;
    case 0x402:
        //printf("%c",value);
        break;
    case 0x61: ports.portsVal[port] = value;
        break;
    case 0x510: {
        if (size == 2) {
            fw_cfg.selector = value & 0xFFFF;
            fw_cfg.data_offset = 0;
        }
        break;
    }
    case 0x92:
        break;


    case 0x3F8:
        if (KernelDebugger::lcr & 0x80) {
            KernelDebugger::baud_lo = value & 0xFF;
        }
        else {
            uint8_t b = value & 0xFF;
            if (KernelDebugger::pipe_out != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                BOOL result = WriteFile(KernelDebugger::pipe_out, &b, 1, &written, NULL);
            }
        }
        break;
    case 0x3F9:
        if (KernelDebugger::lcr & 0x80)
            KernelDebugger::baud_hi = value & 0xFF;
        else
            KernelDebugger::ier = value & 0xFF;
        break;
    case 0x3FB:
        KernelDebugger::lcr = value & 0xFF;
        break;
    case 0x3FC:
        KernelDebugger::mcr = value & 0xFF;
        break;
    case 0x3FF:
        KernelDebugger::scratch = value & 0xFF;
        break;
    case 0x2f8:
        printf("%c", value);
        break;
    case 0x60:
        ud->kbd->write_data(value);
        break;
    case 0x64:
        ud->kbd->write_command(value);
        break;
    case 0x1CE:
    case 0x1CF:
        sb->out_hook(port, value,size);
        break;
    default:
        printf("[OUT] port=0x%04X size=%d val=0x%X\n", port, size, value);
        break;
    }
}

void hook_mmio_in(uint64_t PA, uint8_t* Data, uint16_t size, void* user_data) {
    uint64_t offset = 0;
    if (PA >= IOAPIC_BASE && PA <= IOAPIC_BASE + IOAPIC_SIZE) {
        //IOAPIC
        offset = PA - IOAPIC_BASE;
        uint64_t res = IOapic_mmio_read(offset, user_data);
        memcpy(Data, &res, size);
    }
    else if (PA >= LAPIC_BASE && PA <= LAPIC_BASE + IOAPIC_SIZE) {
        //IOAPIC
        offset = PA - LAPIC_BASE;
        uint64_t res = lapic_mmio_read(offset, user_data);
        memcpy(Data, &res, size);
    }
    else if (PA >= VGA_BASE && PA <= VGA_BASE+VGA_SIZE) {
        uint32_t value = 0;
        value = vga_mem_readb(ud.sb->vgaC->vga, PA);
        memcpy(Data,&value, size);
    }
    // This will rep in/out ins
    else if (PA < RAM_SIZE) {
        memcpy(Data, RAM + PA, size);
    }
    else {
        std::cout << "Unknow MMIO" << std::endl;
    }
}
void hook_mmio_out(uint64_t PA, uint8_t* Data, uint16_t size, void* user_data) {
    uint64_t offset = 0;
    if (PA >= IOAPIC_BASE && PA <= IOAPIC_BASE + IOAPIC_SIZE) {
        //IOAPIC
        offset = PA - IOAPIC_BASE;
        uint64_t value = 0;
        memcpy(&value, Data, size);
        IOapic_mmio_write(offset, value, user_data);
    }
    else if (PA >= LAPIC_BASE && PA <= LAPIC_BASE + IOAPIC_SIZE) {
        //LAPIC
        offset = PA - LAPIC_BASE;
        uint64_t value = 0;
        memcpy(&value, Data, size);
        lapic_mmio_write(offset, value, user_data);
    }
    else if (PA >= VGA_BASE && PA <= VGA_BASE+VGA_SIZE) {
        uint32_t value = 0;
        memcpy(&value, Data, size);
        vga_mem_writeb(ud.sb->vgaC->vga, PA, value);
    }
    // This will rep in/out ins
    else if (PA < RAM_SIZE) {
        memcpy(RAM + PA, Data, size);
    }
    else {
        std::cout << "Unknow MMIO" << std::endl;
    }
}
void hook_wrmsr(uint32_t index, uint32_t rax, uint32_t rdx) {
    UINT64 value = ((UINT64)(rdx) << 32) | (rax & 0xFFFFFFFF);
    MSR::msr_values[index] = value;
    std::cout << "WRMSR" << std::hex << index << std::endl;
}
void hook_rdmsr(uint32_t index, uint32_t* rax, uint32_t* rdx) {
    if (rax == nullptr && rdx == nullptr)throw 0;
    uint64_t return_val = 0;
    switch (index) {
        // MCA Bank Control registers (0x400, 0x404, 0x408, ...)
        // Each bank is 4 MSRs apart: CTL, STATUS, ADDR, MISC
    case 0x400: // IA32_MC0_CTL
    case 0x404: // IA32_MC1_CTL
    case 0x408: // IA32_MC2_CTL
    case 0x40C: // IA32_MC3_CTL
        return_val = 0xFFFFFFFFFFFFFFFF; break;

        // MCA Bank Status registers — return 0 (no errors)
    case 0x401: // IA32_MC0_STATUS
    case 0x405: // IA32_MC1_STATUS
    case 0x409: // IA32_MC2_STATUS
    case 0x40D: // IA32_MC3_STATUS
        return_val = 0; break;

        // MCA Bank ADDR/MISC — return 0
    case 0x402: case 0x403:
    case 0x406: case 0x407:
    case 0x40A: case 0x40B:
    case 0x40E: case 0x40F:
        return_val = 0; break;
    default:
        std::cout << "Unhandled RDMSR" << std::hex << index << std::endl;
        return_val = 0; break;
    }
    *rax = return_val & 0xFFFFFFFF;
    *rdx = (return_val >> 32) & 0xFFFFFFFF;
}
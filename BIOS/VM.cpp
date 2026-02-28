#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <unicorn.h>
#include "io.h"
#include "memory.h"
#include "CPU.h"
#include <thread>
#include "Display.h"
#include "PIC.h"
#include "SDL3/SDL.h"
CPUctx* cx;
PICState pic;
static std::vector<uint8_t> load_bios(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        exit(1);
    }
    size_t sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}
void StopEmulation() {
    uc_emu_stop(cx->currentEngine);
}
uint32_t GetEflags() {
    uc_reg_read(cx->currentEngine,UC_X86_REG_EFLAGS,&cx->currentCPU->Eflag);
    return cx->currentCPU->Eflag;
}
void EmulationLoop() {
    uint32_t eflags = 0x0002;
    uint32_t sp = 0xFFFE;
    uint32_t ss = 0x0000;
    
    cx->currentCPU->CS = 0;
    cx->currentCPU->EIP = 0x7c00;
    cx->currentCPU->CS = 0xF000;
    cx->currentCPU->EIP = 0xFFF0;
    uc_reg_write(cx->uc16, UC_X86_REG_CS, &cx->currentCPU->CS);
    uc_reg_write(cx->uc16, UC_X86_REG_IP, &cx->currentCPU->EIP);
    uc_reg_write(cx->uc16, UC_X86_REG_EFLAGS, &eflags);
    uc_reg_write(cx->uc16, UC_X86_REG_SP, &sp);
    uc_reg_write(cx->uc16, UC_X86_REG_SS, &ss);
    uc_reg_write(cx->uc16, UC_X86_REG_CR0, &ss);
    //uc_err err = uc_emu_start(cx->uc16, 0xFFFF0, 0, 0, 0);
    //cs = 0;
    //ip = 0x7c00;

    while (running) {
        uc_reg_read(cx->currentEngine, UC_X86_REG_EIP,&cx->currentCPU->EIP);
        uc_reg_read(cx->currentEngine, UC_X86_REG_CS, &cx->currentCPU->CS);
        uc_reg_read(cx->currentEngine, UC_X86_REG_CR0, &cx->currentCPU->CR0);
        bool cr0 = cx->currentCPU->CR0 & 0x1;
        uint64_t ip;
        if (!cr0) {
            ip = ((uint16_t)cx->currentCPU->EIP) + (((uint16_t)cx->currentCPU->CS)*0x10);
        }
        else {
            ip = cx->currentCPU->EIP;
        }
        
        ok(uc_emu_start(cx->currentEngine, ip, 0, 0, 0));
        EnterCriticalSection(&irq.cs);
        DoAllHardwareInt(cx->currentEngine);
        LeaveCriticalSection(&irq.cs);
    }
}
void Poll() {
    SDL_Event event;
    while (running.load()) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                StopEmulation();
                //DumpLogs();
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode code = event.key.key;
            }
        }
    }
}
int main(int argc, char* argv[]) {
    const char* bios_path = "D:/windows nt/bios.bin";

    auto bios = load_bios(bios_path);
    //auto vgabios = load_bios("D:/windows nt/vgabios.bin");
    InitMemory();
    InitIO(StopEmulation,GetEflags);
    CPUctx ctx;
    cx = &ctx;
    ctx.hud.pic = &pic;
    size_t offset = BIOS_SIZE - bios.size();
    memcpy(RAM + BIOS_BASE+ offset,bios.data(),bios.size());
    LoadDisk("D:/windows nt/windows xp32.iso", DISK_TYPE::DVD);
    LoadDisk("D:/windows nt/disk0.disk", DISK_TYPE::HDD0);
    DisplayInit();

    std::thread emulation(EmulationLoop);
    std::thread Display(DisplayLoop);
    std::thread PIT(PITThread,&pic);
    Poll();
    PIT.join();
    emulation.join();
    Display.join();
    DeinitMemory();
    DisplayDeInit();
}
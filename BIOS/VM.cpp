#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <Windows.h>
#include "io.h"
#include "memory.h"
#include "CPU.h"
#include <thread>
#include "Display.h"
#include "PIC.h"
#include "PCI.h"
#include "Whpx.h"
#include "SDL3/SDL.h"
WHPX* hypervisior;
PCISystemBus pci;
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
void Poll() {
    SDL_Event event;
    while (running.load()) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                hypervisior->StopVP();
                //DumpLogs();
            }
            else if (event.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode code = event.key.key;
                if (code == SDLK_S) {
                    
                }
                else if(code==SDLK_A){
                    kbd->send_scancode(Scancode1::F2);
                }
                else if (code == SDLK_E) {
                    hypervisior->StopVP();
                }
                else if (code == SDLK_Q) {
                    kbd->send_scancode((Scancode1::ENTER));  // break
                }
            }
            else if (event.type == SDL_EVENT_KEY_UP) {
                SDL_Keycode code = event.key.key;
                if (code == SDLK_A) {
                    kbd->send_scancode(Scancode1::BREAK(Scancode1::F6));  // break
                }
                else if (code == SDLK_Q) {
                    kbd->send_scancode(Scancode1::BREAK(Scancode1::ENTER));  // break
                }
            }
        }
    }
}
void EmulationLoop() {
    hypervisior->RunVP();
}
int main(int argc, char* argv[]) {
    const char* bios_path = "D:/windows nt/bios_d.bin";
    const char* vgabios_path = "D:/windows nt/vgabios.bin";
    std::string isopath = "D:/windows nt/windows xp32_d.iso";
    auto bios = load_bios(bios_path);
    auto vgabios = load_bios(vgabios_path);
    fw_cfg.init(RAM_SIZE);
    InitMemory();
    InitIO(&pci,WHPX::ThunkRaiseInterrupt,isopath);
    size_t offset = BIOS_SIZE - bios.size();
    memcpy((char*)RAM +BIOS_BASE+offset, bios.data(), bios.size());
    memcpy((char*)VGA_ROM, vgabios.data(), vgabios.size());
    UD ud;
    ud.pic = &pic;
    ud.sb = &pci;
    hypervisior = new WHPX(&ud);
    ud.whpxCtx = hypervisior;
    DisplayInit();
    std::thread emulation(EmulationLoop);
    std::thread Display(DisplayLoop);
    std::thread Time(Timers::TimerThread);
    Poll();
    Time.join();
    emulation.join();
    Display.join();
    delete hypervisior;
    DeinitMemory();
    DisplayDeInit();
}
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

//Qemu method of injecting interrupts
//static bool ready_for_pic_interrupt = true;
//int intN = -1;
//void PreRun(WHV_RUN_VP_EXIT_CONTEXT exit_ctx) {
//    WHV_REGISTER_NAME reg_names[2];
//    WHV_REGISTER_VALUE reg_values[2];
//    UINT32 reg_count = 0;
//
//    memset(reg_values, 0, sizeof(reg_values));
//
//    // inject pending interrupt if CPU is ready
//    if ((cx->cpu32->Eflag & (1 << 9)))
//    {
//        if (ready_for_pic_interrupt) {
//            ready_for_pic_interrupt = false;
//
//            int vector = intN; // get + ack highest priority
//            if (vector >= 0) {
//                reg_values[reg_count].PendingInterruption.InterruptionPending = 1;
//                reg_values[reg_count].PendingInterruption.InterruptionType = WHvX64PendingInterrupt;
//                reg_values[reg_count].PendingInterruption.InterruptionVector = vector;
//                reg_names[reg_count] = WHvRegisterPendingInterruption;
//                reg_count++;
//            }
//        }
//    }    
//    if (!cx->cpu32->window_registered && intN>=0) {
//        reg_values[reg_count].DeliverabilityNotifications.InterruptNotification = 1;
//        reg_names[reg_count] = WHvX64RegisterDeliverabilityNotifications;
//        cx->cpu32->window_registered = true;
//        reg_count++;
//    }
//
//    if (reg_count > 0) {
//        HRESULT hr = WHvSetVirtualProcessorRegisters(
//            partition, cx->vpindex,
//            reg_names, reg_count, reg_values);
//        if (FAILED(hr)) {
//            printf("pre_run: failed to set registers hr=0x%08X\n", hr);
//        }
//        WHV_REGISTER_VALUE hlt_reg = {};
//        WHV_REGISTER_NAME hlt= WHvRegisterInternalActivityState;
//        WHvGetVirtualProcessorRegisters(partition,cx->vpindex,&hlt ,1, &hlt_reg);
//        if (hlt_reg.InternalActivity.HaltSuspend) {
//            hlt_reg.InternalActivity.HaltSuspend = 0;
//            WHvSetVirtualProcessorRegisters(partition, cx->vpindex, &hlt,1, &hlt_reg);
//        }
//        pic.IRQRaised = false;
//        intN = -1;
//    }
//}
//void PostRun(WHV_RUN_VP_EXIT_CONTEXT exit_ctx) {
//    cx->cpu32->Eflag = exit_ctx.VpContext.Rflags;
//    cx->cpu32->interruption_pending = exit_ctx.VpContext.ExecutionState.InterruptionPending;
//    cx->cpu32->interruptable = !exit_ctx.VpContext.ExecutionState.InterruptShadow;
//}
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
                    //running = false;
                    //hypervisior->StopVP();
                    pic.master_mask = 0xff;
                    test = false;
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
    std::string isopath = "D:/windows nt/windows xp32_d.iso";
    auto bios = load_bios(bios_path);
    fw_cfg.init(RAM_SIZE);
    InitMemory();
    InitIO(&pci,WHPX::ThunkRaiseInterrupt,isopath);
    size_t offset = BIOS_SIZE - bios.size();
    memcpy((char*)RAM +BIOS_BASE+offset, bios.data(), bios.size());
    
    UD ud;
    ud.pic = &pic;
    ud.sb = &pci;
    hypervisior = new WHPX(&ud);
    ud.whpxCtx = hypervisior;
    DisplayInit();
    std::thread emulation(EmulationLoop);
    std::thread Display(DisplayLoop);
    std::thread PIT(PITThread,&pic);
    Poll();
    PIT.join();
    emulation.join();
    Display.join();
    delete hypervisior;
    DeinitMemory();
    DisplayDeInit();
}
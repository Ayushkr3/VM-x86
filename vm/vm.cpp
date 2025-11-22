#include "vm.h"

AllCPU cpus;
bool isIn32Mode = true;

char* RAM = nullptr;
xed_state_t state;
uc_err err;
UnicornData* SetupArch() {
    UnicornData* ud = new UnicornData;
    err = uc_open(UC_ARCH_X86, UC_MODE_16, &cpus.uc16);
    err = uc_open(UC_ARCH_X86, UC_MODE_32, &cpus.uc32);
    err = uc_mem_map_ptr(cpus.uc16, 0, RAM_SIZE, UC_PROT_ALL, RAM);
    err = uc_mem_map_ptr(cpus.uc32, 0, RAM_SIZE, UC_PROT_ALL, RAM);
    err = uc_hook_add(cpus.uc16, &ud->PerBlockhook, UC_HOOK_BLOCK, PerBlockHook, (void*)ud, 0, RAM_SIZE);
    err = uc_hook_add(cpus.uc32, &ud->PerBlockhook, UC_HOOK_BLOCK, PerBlockHook, (void*)ud, 0, RAM_SIZE);
#ifdef DEBUG
    err = uc_hook_add(cpus.uc16, &ud->PerLinehook, UC_HOOK_BLOCK, PerLineHook, (void*)ud, 0, RAM_SIZE);
    err = uc_hook_add(cpus.uc16, &ud->SpecialIns, UC_HOOK_INSN, LookUpSpecialIns, (void*)ud, 0, RAM_SIZE);
    //err = uc_hook_add(uc, &MemRead, UC_HOOK_MEM_READ_AFTER, MemR, ud, 0, RAM_SIZE);
    //err = uc_hook_add(uc, &MemWrite, UC_HOOK_MEM_WRITE, MemW, ud, 0, RAM_SIZE);
    err = uc_hook_add(cpus.uc16, &ud->InvalidIns, UC_HOOK_INSN_INVALID, InvalidInsHook, (void*)ud, 0, 0);
    err = uc_hook_add(cpus.uc32, &ud->PerLinehook, UC_HOOK_BLOCK, PerLineHook, (void*)ud, 0, RAM_SIZE);
    err = uc_hook_add(cpus.uc32, &ud->SpecialIns, UC_HOOK_INSN, LookUpSpecialIns, (void*)ud, 0, RAM_SIZE);
    //err = uc_hook_add(uc, &MemRead, UC_HOOK_MEM_READ_AFTER, MemR, ud, 0, RAM_SIZE);
    //err = uc_hook_add(uc, &MemWrite, UC_HOOK_MEM_WRITE, MemW, ud, 0, RAM_SIZE);
    err = uc_hook_add(cpus.uc32, &ud->InvalidIns, UC_HOOK_INSN_INVALID, InvalidInsHook, (void*)ud, 0, 0);
#endif
    err = uc_hook_add(cpus.uc16, &ud->Inthook, UC_HOOK_INTR, LookUpUnicorn, (void*)ud, 0, 0);
    err = uc_hook_add(cpus.uc32, &ud->Inthook, UC_HOOK_INTR, LookUpUnicorn, (void*)ud, 0, 0);
    ud->cpu = cpus.cpu16;
    ud->uc = cpus.uc16;
    ud->cpus = &cpus;
    return ud;
}
void EmulationLoop(UnicornData* ud) {
    cpus.cpu16->RIP = 0x7c00;
    err = uc_reg_write(cpus.uc16, UC_X86_REG_IP, &cpus.cpu16->RIP);
    while (running) {
        ReadRegUni(ud);
        uint64_t ip = ud->cpu->GetFlatMemoryIP();
        err = uc_emu_start(ud->uc, ip, 0, 0, 0);
        EnterCriticalSection(&IRQ.cs);

        LeaveCriticalSection(&IRQ.cs);
    }
    std::cout << "VM Exit";
}
int main()
{
    xed_tables_init();
    RAM = (char*)malloc(2147483648); //2 GB ram
    UnicornData* ud = SetupArch();
    cpus.cpu16->SetRegOrder(RegOrder16);
    cpus.cpu32->SetRegOrder(RegOrder32);
    LoadDisk("D:/windows nt/windows xp32.iso", DISK_TYPE::DVD);
    LoadDisk("D:/windows nt/disk0.disk", DISK_TYPE::HDD0);
    ZeroMemory(RAM, RAM_SIZE);

    BootDisk();
    running = true;
    cpus.cpu16->RDX = 0xe0; {
        uc_reg_write(cpus.uc16, UC_X86_REG_AX, &cpus.cpu16->RAX);
        uc_reg_write(cpus.uc16, UC_X86_REG_BX, &cpus.cpu16->RBX);
        uc_reg_write(cpus.uc16, UC_X86_REG_CX, &cpus.cpu16->RCX);
        uc_reg_write(cpus.uc16, UC_X86_REG_DX, &cpus.cpu16->RDX);
        uc_reg_write(cpus.uc16, UC_X86_REG_SP, &cpus.cpu16->RSP);
        uc_reg_write(cpus.uc16, UC_X86_REG_BP, &cpus.cpu16->RBP);
        uc_reg_write(cpus.uc16, UC_X86_REG_SI, &cpus.cpu16->RSI);
        uc_reg_write(cpus.uc16, UC_X86_REG_DI, &cpus.cpu16->RDI);
        uc_reg_write(cpus.uc16, UC_X86_REG_ES, &cpus.cpu16->ES);
        uc_reg_write(cpus.uc16, UC_X86_REG_SS, &cpus.cpu16->SS);
        uc_reg_write(cpus.uc16, UC_X86_REG_FS, &cpus.cpu16->FS);
        uc_reg_write(cpus.uc16, UC_X86_REG_DS, &cpus.cpu16->DS);
        uc_reg_write(cpus.uc16, UC_X86_REG_CS, &cpus.cpu16->CS);
        int flag = 0x202;
        uc_reg_write(cpus.uc16, UC_X86_REG_EFLAGS, &flag);
        xed_state_zero(&state);
        xed_state_init(&state, XED_MACHINE_MODE_REAL_16, XED_ADDRESS_WIDTH_16b, XED_ADDRESS_WIDTH_16b);
    }
    /////////////////////////////////////////////////////////////////////////
    InitIO(&cpus.uc16);
    std::thread PIC(PICThread);
    EmulationLoop(ud);
    //PIC.join();
    RemoveDisk(DISK_TYPE::DVD);
    RemoveDisk(DISK_TYPE::HDD0);
    free(RAM);
}


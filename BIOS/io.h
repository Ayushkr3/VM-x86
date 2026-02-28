#pragma once
#include <cstdint>
#include "unicorn.h"
#include "Global.h"
#include <string>
#include <sstream>
#include <fstream>
#include "memory.h"
extern IO ports;
typedef void (*StopEmulationFWD)(void);
extern StopEmulationFWD StopEmulation_fwd;
typedef uint32_t (*GetEflagsFWD)(void);
extern GetEflagsFWD GetEflags_fwd;
struct CMOS {
	static uint8_t cmos_index;
	static uint8_t cmos_data[128];
};
uint32_t hook_in(uc_engine* uc, uint32_t port, int size, void* user_data);
void hook_out(uc_engine* uc, uint32_t port, int size, uint32_t value, void* user_data);
void hook_intr(uc_engine* uc, uint32_t intno, void* user_data);
void hook_wrmsr(uc_engine* uc, void* user_data);
void hook_rdmsr(uc_engine* uc, void* user_data);
void hook_cpuid(uc_engine* uc, void* user_data);
void InitIO(StopEmulationFWD sfwd, GetEflagsFWD gfwd);
void init_cmos();
void RaiseIRQN(int intN);
void DoAllHardwareInt(uc_engine* eng);

void LoadDisk(std::string path, DISK_TYPE type);
void ReadIODisk(int index, long long DAP,uc_engine* uc);
void RemoveDisk(int index);
void BootDisk();
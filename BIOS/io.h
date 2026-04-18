#pragma once
#include <cstdint>
#include "Global.h"
#include <string>
#include <sstream>
#include <fstream>
#include "memory.h"
#include "PCI.h"
#include "kd.h"
#include "fw_cfg.h"
#include "timer.h"
#include "i8402.h"
extern FWCfg fw_cfg;
extern IO ports;
typedef uint32_t (*GetEflagsFWD)(void);
struct CMOS {
	static uint8_t cmos_index;
	static uint8_t cmos_data[128];
};
struct MSR {
	static std::unordered_map<uint64_t, uint64_t> msr_values;
};
struct UD {
	PCISystemBus* sb;
	PICState* pic;
	PS2Keyboard* kbd;
	void* whpxCtx;
};
extern UD ud;

uint32_t hook_in(uint16_t port, int size, void* user_data);
void hook_out(uint16_t port, int size, uint32_t value, void* user_data);
void hook_wrmsr(uint32_t index,uint32_t rax,uint32_t rdx);
void hook_rdmsr(uint32_t index, uint32_t* rax,uint32_t* rdx);
void hook_mmio_in(uint64_t PA, uint8_t* Data, uint16_t size,void* user_data);
void hook_mmio_out(uint64_t PA, uint8_t* Data, uint16_t size, void* user_data);
void InitIO(PCISystemBus* sb, RaiseIRQ_f rfwd, std::string isoPath);
void init_cmos();

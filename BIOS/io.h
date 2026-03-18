#pragma once
#include <cstdint>
#include "Global.h"
#include <string>
#include <sstream>
#include <fstream>
#include "memory.h"
#include "PIC.h"
#include "PCI.h"
#include "kd.h"
#include "fw_cfg.h"
extern FWCfg fw_cfg;
extern IO ports;
typedef uint32_t (*GetEflagsFWD)(void);
struct CMOS {
	static uint8_t cmos_index;
	static uint8_t cmos_data[128];
};
struct UD {
	PCISystemBus* sb;
	PICState* pic;
	void* whpxCtx;
};
uint32_t hook_in(uint16_t port, int size, void* user_data);
void hook_out(uint16_t port, int size, uint32_t value, void* user_data);
void hook_mmio_in(uint64_t PA, uint8_t* Data, uint16_t size,void* user_data);
void hook_mmio_out(uint64_t PA, uint8_t* Data, uint16_t size, void* user_data);
void InitIO(PCISystemBus* sb, RaiseIRQ rfwd,std::string isopath);
void init_cmos();

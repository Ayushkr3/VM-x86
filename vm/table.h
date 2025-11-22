#pragma once
#include <Windows.h>
#include "CPU.h"
#include "xed_table.h"
#include <vector>
#include <stack>
#include "io.h"
#include "unicorn.h"
#include "unic.h"
#define REVERSE_4_BYTE(ptr) ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
#define STACK (RAM+cpu->RSP)
#define MASK(value,width) value & ((1ULL << width) - 1);

void ReadInstruction(CPU* cpu, INT64* offset,xed_decoded_inst_t* Pins);
void LookUp(CPU* cpu,xed_decoded_inst_t* Pins, INT64* ripOffset, char* FileOffset);


void MemDump(char* at,int size);



void LookUpUnicorn(uc_engine* uc, uint32_t address, void* user_data);
void LookUpSpecialIns(uc_engine* uc, uint32_t address, void* user_data);
void PerBlockHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
void PerLineHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
void ProtectedModeSwitchHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
void InvalidInsHook(uc_engine* uc, void* user_data);
void MemR(uc_engine* uc, uc_mem_type type,
	uint64_t address, int size, int64_t value, void* user_data);
void MemW(uc_engine* uc, uc_mem_type type,
	uint64_t address, int size, int64_t value, void* user_data);

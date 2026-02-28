#pragma once
#include "unicorn.h"
#include "Global.h"
#include "APIC.h"
static const uint64_t RAM_BASE = 0x00000000;
static const uint64_t RAM_SIZE = 2147483648; // 1MB real mode RAM
static const uint64_t BIOS_BASE = 0x000E0000; // 128KB BIOS region
static const uint64_t BIOS_SIZE = 0x20000;
static const uint64_t RESET_BASE = 0xFFFF0; // reset vector region
static const uint64_t RESET_SIZE = 0x00010000;
static const uint64_t LAPIC_BASE = 0xFEE00000;
static const uint64_t LAPIC_SIZE = 0x00001000;
static const uint64_t IOAPIC_BASE = 0xFEC00000;
static const uint64_t IOAPIC_SIZE = 0x00001000;

extern char* RAM;

void InitMemory();
void DeinitMemory();

void unicorn_mem_init(uc_engine* uc);
void unicorn_add_mmio_region(uc_engine* uc,void* cpuS,uc_cb_mmio_read_t rLAPICHook, uc_cb_mmio_write_t wLAPIChook, uc_cb_mmio_read_t rIOAPICHook, uc_cb_mmio_write_t wIOAPIChook);
#pragma once
#include "Global.h"
#include "APIC.h"
static const uint64_t RAM_BASE = 0x00000000;
static const uint64_t RAM_SIZE = 0x80000000; // 1MB real mode RAM
static const uint64_t BIOS_BASE = 0x000E0000; // 128KB BIOS region
static const uint64_t BIOS_SIZE = 0x20000;
static const uint64_t RESET_BASE = 0xFFFF0; // reset vector region
static const uint64_t RESET_SIZE = 0x00010000;
static const uint64_t LAPIC_BASE = 0xFEE00000;
static const uint64_t LAPIC_SIZE = 0x00001000;
static const uint64_t IOAPIC_BASE = 0xFEC00000;
static const uint64_t IOAPIC_SIZE = 0x00001000;
static const uint64_t SVGA_BASE = 0xE0000000;
static const uint64_t SVGA_SIZE = 0x1000000;

static const uint64_t VGABIOS_BASE = 0xFEB00000;
static const uint64_t VGABIOS_SIZE = 0x10000;

extern char* RAM;
extern char* SVGA;
extern char* VGA_ROM;
void InitMemory();
void DeinitMemory();

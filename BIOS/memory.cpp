#include "memory.h"
char* RAM = nullptr;
void InitMemory() {
	RAM = (char*)malloc(RAM_SIZE);
	ZeroMemory(RAM, RAM_SIZE);
}
void DeinitMemory() {
	free(RAM);
}
void unicorn_mem_init(uc_engine* uc) {
	if (RAM == nullptr) {
		throw 0x0;
	}
	ok(uc_mem_map_ptr(uc, RAM_BASE, RAM_SIZE, UC_PROT_ALL, RAM));
}
void unicorn_add_mmio_region(uc_engine* uc, void* cpuS, uc_cb_mmio_read_t rLAPICHook, uc_cb_mmio_write_t wLAPIChook, uc_cb_mmio_read_t rIOAPICHook, uc_cb_mmio_write_t wIOAPIChook) {
	ok(uc_mmio_map(uc, LAPIC_BASE, LAPIC_SIZE, rLAPICHook, cpuS, wLAPIChook, cpuS));
	ok(uc_mmio_map(uc, IOAPIC_BASE, IOAPIC_SIZE, rIOAPICHook, cpuS, wIOAPIChook, cpuS));
}
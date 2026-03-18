#include "memory.h"
char* RAM = nullptr;
char* BIOS = nullptr;
void InitMemory() {
	RAM= (char*)VirtualAlloc(nullptr, RAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	//BIOS = (char*)VirtualAlloc(nullptr, BIOS_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(RAM, 0,RAM_SIZE);
	//ZeroMemory(BIOS, BIOS_SIZE);
}
void DeinitMemory() {
	
	VirtualFree(RAM,RAM_SIZE, MEM_RELEASE| MEM_DECOMMIT);
	//VirtualFree(BIOS, BIOS_SIZE, MEM_RELEASE | MEM_DECOMMIT);
}
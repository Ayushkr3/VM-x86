#include "memory.h"
char* RAM = nullptr;
char* SVGA = nullptr;
char* VGA_ROM = nullptr;
void InitMemory() {
	RAM= (char*)VirtualAlloc(nullptr, RAM_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	SVGA = (char*)VirtualAlloc(nullptr, SVGA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	VGA_ROM = (char*)VirtualAlloc(nullptr,VGABIOS_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	memset(RAM, 0,RAM_SIZE);
	memset(VGA_ROM, 0, VGABIOS_SIZE);
	memset(SVGA, 0, SVGA_SIZE);
}
void DeinitMemory() {
	VirtualFree(VGA_ROM, VGABIOS_SIZE, MEM_RELEASE | MEM_DECOMMIT);
	VirtualFree(RAM, RAM_SIZE, MEM_RELEASE| MEM_DECOMMIT);
	VirtualFree(SVGA, SVGA_SIZE, MEM_RELEASE | MEM_DECOMMIT);
}
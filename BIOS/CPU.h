#pragma once
#include <Windows.h>
#include <iostream>
#include <intrin.h>
#include "APIC.h"
#include "memory.h"
#include "Global.h"
#include "io.h"
//////////////////////////////////////////////////////////////////////////////
enum CPU_ARCH {
	X86_16,
	X86_32
};
//////////////////////////////////////////////////////////////////////////////
struct CPU {
	INT64 EAX = 0; //A
	INT64 EBX = 0; //B
	INT64 ECX = 0; //C
	INT64 EDX = 0; //D
	INT64 EBP = 0; //Base pointer
	INT64 ESP = 0; //Stack pointer
	INT64 ESI = 0; //
	INT64 EDI = 0;
	INT64 EIP = 0; //PC

	INT64 GS = 0;
	INT64 FS = 0;
	INT64 ES = 0;
	INT64 DS = 0;
	INT64 CS = 0;
	INT64 SS = 0;

	INT64 CR0 = 0;
	INT64 CR1 = 0;
	INT64 CR2 = 0;
	INT64 CR3 = 0;
	INT64 CR4 = 0;
	INT64 CR8 = 0;
	struct mmrs {
		uint32_t selector = 0;
		uint64_t base = 0;
		uint32_t limit = 0;
		uint32_t flags = 0;
	};
	mmrs GDT;
	mmrs LDT;
	mmrs IDT;
	INT64 Eflag = 0;
	LIOAPIC lapic;
	INT64* Reg[21];
	CPU() {
		Reg[0] = &EAX;
		Reg[1] = &ECX;
		Reg[2] = &EDX;
		Reg[3] = &EBX;
		Reg[4] = &ESP;
		Reg[5] = &EBP;
		Reg[6] = &ESI;
		Reg[7] = &EDI;
		Reg[14]= &EIP;
		//Segment
		Reg[8] = &ES;
		Reg[9] = &CS;
		Reg[10] = &SS;
		Reg[11] = &DS;
		Reg[12] = &FS;
		Reg[13] = &GS;
		//Control
		Reg[15] = &CR0;
		Reg[16] = &CR1;
		Reg[17] = &CR2;
		Reg[18] = &CR3;
		Reg[19] = &CR4;
		Reg[20] = &Eflag;
		lapic.CPU_DATA = (void*)this;
		Init_APIC(&lapic);
	}
	UINT32 interruption_pending;
	UINT32 interruptable;
	bool window_registered;
	static bool inProtectedMode;
	virtual void SetRegOrder(const int* _RegOrder) = 0;
	virtual const int* GetRegOrder() = 0;
	virtual const int  GetNumberofRegister() = 0;
	const int* RegOrder;
	virtual uint64_t GetFlatMemoryIP() = 0;
	virtual CPU_ARCH GetArchitecture() = 0;
};
struct CPU32 :public CPU {
	void SetRegOrder(const int* _RegOrder) {
		RegOrder = _RegOrder;
	}
	const int* GetRegOrder() {
		return RegOrder;
	}
	const int  GetNumberofRegister() {
		return 21;
	}
	uint64_t GetFlatMemoryIP() {
		return  (uint64_t)(EIP);
	}
	CPU_ARCH GetArchitecture() {
		return CPU_ARCH::X86_32;
	}
};
struct CPU16 :public CPU {
	void SetRegOrder(const int* _RegOrder) {
		RegOrder = _RegOrder;
	}
	const int* GetRegOrder() {
		return RegOrder;
	};
	const int  GetNumberofRegister() {
		return 21;
	}
	uint64_t GetFlatMemoryIP() {
		return  EIP + CS * 16;
	}
	CPU_ARCH GetArchitecture() {
		return CPU_ARCH::X86_16;
	}
};


struct CPUctx {
	CPU* cpu16;
	CPU* cpu32;
	CPU* currentCPU;
	HookUserData hud;
	CPUctx() {
		/*cpu16 = new CPU16;
		cpu32 = new CPU32;
		cpu16->SetRegOrder(RegOrder16);
		cpu32->SetRegOrder(RegOrder32);
		ok(uc_open(uc_arch::UC_ARCH_X86, uc_mode::UC_MODE_16, &uc16));
		ok(uc_open(uc_arch::UC_ARCH_X86, uc_mode::UC_MODE_32, &uc32));
		unicorn_mem_init(uc16);
		unicorn_mem_init(uc32);
		Init_APIC(&cpu16->lapic);*/
		//unicorn_add_mmio_region(uc16,(void*)&(cpu16->lapic),lapic_mmio_read, lapic_mmio_write,IOapic_mmio_read,IOapic_mmio_write);
		////unicorn_add_mmio_region(uc32, (void*)&(cpu32->lapic), lapic_mmio_read, lapic_mmio_write, IOapic_mmio_read, IOapic_mmio_write);
		//uc_hook_add(uc16, &hookdata.INS_IN, UC_HOOK_INSN, (void*)hook_in, (void*)&hud, 1, 0, UC_X86_INS_IN);
		//uc_hook_add(uc16, &hookdata.INS_OUT, UC_HOOK_INSN, (void*)hook_out, (void*)&hud, 1, 0, UC_X86_INS_OUT);
		//uc_hook_add(uc16, &hookdata.INS_INT, UC_HOOK_INTR, (void*)hook_intr, (void*)&hud, 1, 0);
		//uc_hook_add(uc16, &hookdata.INS_WRMSR, UC_HOOK_INSN, (void*)hook_wrmsr, (void*)&hud, 1, 0, UC_X86_INS_WRMSR);
		//uc_hook_add(uc16, &hookdata.INS_RDMSR, UC_HOOK_INSN, (void*)hook_rdmsr, (void*)&hud, 1, 0, UC_X86_INS_RDMSR);
		//uc_hook_add(uc16, &hookdata.INS_CPUID, UC_HOOK_INSN, (void*)hook_cpuid, (void*)&hud, 1, 0, UC_X86_INS_CPUID);
		//uc_hook_add(uc32, &hookdata.INS_IN, UC_HOOK_INSN, (void*)hook_in, (void*)&hud, 1, 0, UC_X86_INS_IN);
		//uc_hook_add(uc32, &hookdata.INS_OUT, UC_HOOK_INSN, (void*)hook_out, (void*)&hud, 1, 0, UC_X86_INS_OUT);
		//uc_hook_add(uc32, &hookdata.INS_INT, UC_HOOK_INTR, (void*)hook_intr, (void*)&hud, 1, 0);

		//uc_hook_add(uc32, &hookdata.ModeSwitch, UC_HOOK_BLOCK, (void*)PerBlockHook, (void*)this, 0, 0xFFFFFFFF);
		//uc_hook_add(uc16, &hookdata.ModeSwitch, UC_HOOK_BLOCK, (void*)PerBlockHook, (void*)this, 0, 0xFFFFFFFF);
		currentCPU = cpu16;
	}
	~CPUctx() {
		//uc_close(uc16);
		//uc_close(uc32);
		//delete cpu16;
		//delete cpu32;
	}
};

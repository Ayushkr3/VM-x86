#pragma once
#include "unicorn.h"
#include "CPU.h"

struct UnicornData
{
	uc_hook Inthook;
	uc_hook PerLinehook;
	uc_hook PerBlockhook;
	uc_hook MemRead;
	uc_hook MemWrite;
	uc_hook InvalidIns;
	uc_hook SpecialIns;
	uc_hook ModeProtection;

	uc_engine* uc;
	CPU* cpu;
	AllCPU* cpus;
};
const int RegOrder32[]{
UC_X86_REG_EAX,
UC_X86_REG_ECX,
UC_X86_REG_EDX,
UC_X86_REG_EBX,
UC_X86_REG_ESP,
UC_X86_REG_EBP,
UC_X86_REG_ESI,
UC_X86_REG_EDI,
UC_X86_REG_ES,
UC_X86_REG_CS,
UC_X86_REG_SS,
UC_X86_REG_DS,
UC_X86_REG_FS,
UC_X86_REG_GS,
UC_X86_REG_EIP,
UC_X86_REG_CR0,
UC_X86_REG_CR1,
UC_X86_REG_CR2,
UC_X86_REG_CR3,
UC_X86_REG_CR4,
UC_X86_REG_EFLAGS,
};
const int RegOrder16[]{
UC_X86_REG_AX,
UC_X86_REG_CX,
UC_X86_REG_DX,
UC_X86_REG_BX,
UC_X86_REG_SP,
UC_X86_REG_BP,
UC_X86_REG_SI,
UC_X86_REG_DI,
UC_X86_REG_ES,
UC_X86_REG_CS,
UC_X86_REG_SS,
UC_X86_REG_DS,
UC_X86_REG_FS,
UC_X86_REG_GS,
UC_X86_REG_IP,
UC_X86_REG_CR0,
UC_X86_REG_CR1,
UC_X86_REG_CR2,
UC_X86_REG_CR3,
UC_X86_REG_CR4,
UC_X86_REG_EFLAGS,
};
inline void ReadRegUni(UnicornData* ud) {
	uc_err err = uc_reg_read_batch(ud->uc, ud->cpu->GetRegOrder(), (void**)&ud->cpu->Reg, 21);
	err = uc_reg_read(ud->uc, UC_X86_REG_GDTR, &ud->cpu->GDT);
	
}

inline void WriteRegUni(UnicornData* ud) {
	uc_err err = uc_reg_write_batch(ud->uc, ud->cpu->GetRegOrder(), (void**)&ud->cpu->Reg, 21);
}

inline void WriteRegUni(UnicornData* ud, uc_x86_reg REG, INT64 val) {
	uc_err err = uc_reg_write(ud->uc, REG, &val);
}
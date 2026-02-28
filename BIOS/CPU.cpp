#include "CPU.h"
bool CPU::inProtectedMode = false;
bool deltaMode = false; //32 bit requested
bool changing = false;
void helper_Transfer(uc_engine* fromUC,CPU* fromCPU,uc_engine* toUC,CPU* toCPU) {
	ok(uc_reg_read_batch(fromUC,fromCPU->GetRegOrder(), (void**)&fromCPU->Reg, 21));
	//Read mmrs
	uc_reg_read(fromUC, UC_X86_REG_GDTR, &fromCPU->GDT);
	uc_reg_read(fromUC, UC_X86_REG_LDTR, &fromCPU->LDT);
	uc_reg_read(fromUC, UC_X86_REG_IDTR, &fromCPU->IDT);
	//write mmrs
	uc_reg_write(toUC, UC_X86_REG_GDTR, &fromCPU->GDT);
	uc_reg_write(toUC, UC_X86_REG_LDTR, &fromCPU->LDT);
	uc_reg_write(toUC, UC_X86_REG_IDTR, &fromCPU->IDT);
	//===============================================
	uc_reg_write(toUC, UC_X86_REG_EIP, &fromCPU->EIP);
	uc_reg_write(toUC, UC_X86_REG_EAX, &fromCPU->EAX);
	uc_reg_write(toUC, UC_X86_REG_EBX, &fromCPU->EBX);
	uc_reg_write(toUC, UC_X86_REG_ECX, &fromCPU->ECX);
	uc_reg_write(toUC, UC_X86_REG_EDX, &fromCPU->EDX);
	uc_reg_write(toUC, UC_X86_REG_ESP, &fromCPU->ESP);
	uc_reg_write(toUC, UC_X86_REG_EBP, &fromCPU->EBP);
	uc_reg_write(toUC, UC_X86_REG_ESI, &fromCPU->ESI);
	uc_reg_write(toUC, UC_X86_REG_EDI, &fromCPU->EDI);
	uc_reg_write(toUC, UC_X86_REG_EFLAGS, &fromCPU->Eflag);
	uc_reg_write(toUC, UC_X86_REG_CR0, &fromCPU->CR0);
	uc_reg_write(toUC, UC_X86_REG_CR1, &fromCPU->CR1);
	uc_reg_write(toUC, UC_X86_REG_CR2, &fromCPU->CR2);
	uc_reg_write(toUC, UC_X86_REG_CR3, &fromCPU->CR3);
	uc_reg_write(toUC, UC_X86_REG_CR4, &fromCPU->CR4);
	for (int i = 0; i < 9; i++) {
		CPU::mmrs* cahceMMRFrom = nullptr;
		CPU::mmrs* cahceMMRTo = nullptr;
		uc_get_Cached_Seg(fromUC, (void**)&cahceMMRFrom, i);
		uc_get_Cached_Seg(toUC, (void**)&cahceMMRTo, i);
		memcpy(cahceMMRTo, cahceMMRFrom, sizeof(CPU::mmrs));
	}
	uc_reg_read(toUC, UC_X86_REG_ES, &toCPU->ES);
	uc_reg_read(toUC, UC_X86_REG_SS, &toCPU->SS);
	uc_reg_read(toUC, UC_X86_REG_FS, &toCPU->FS);
	uc_reg_read(toUC, UC_X86_REG_DS, &toCPU->DS);
	uc_reg_read(toUC, UC_X86_REG_CS, &toCPU->CS);
	ok(uc_reg_read_batch(toUC, toCPU->GetRegOrder(), (void**)&toCPU->Reg, 21));
	uc_ctl_flush_tb(fromUC);
	uc_ctl_flush_tlb(fromUC);
	uc_ctl_flush_tb(toUC);
	uc_ctl_flush_tlb(toUC);
}
void ProtectedModeSwitchHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
	CPUctx* ud = (CPUctx*)user_data;
	uint8_t* bytes = (uint8_t*)(RAM + address);
	if (*bytes == 0xCB || *bytes == 0xCA || *bytes == 0xEA || *bytes == 0x9a || *bytes == 0xCF) {
		if (changing)return;
		uc_hook_del(ud->currentEngine, ud->hookdata.ModeSwitch);
		//TODO: Maybe transfer MSR
		if (!ud->currentCPU->inProtectedMode) {
			ud->currentCPU->inProtectedMode = true;
			uc_ctl_flush_tb(uc);
			uc_err e;
			uc_reg_read(ud->currentEngine,UC_X86_REG_CS,&ud->currentCPU->CS);
			uc_reg_read(ud->currentEngine,UC_X86_REG_EIP, &ud->currentCPU->EIP);
			e = uc_emu_stop(ud->currentEngine);
			changing = true;
			ok(uc_emu_start(ud->currentEngine, ud->currentCPU->CS * 0x10 + ud->currentCPU->EIP, 0, 0, 1));
			changing = false;
			uc_reg_read(ud->currentEngine, UC_X86_REG_CS, &ud->currentCPU->CS);
			uc_reg_read(ud->currentEngine, UC_X86_REG_EIP, &ud->currentCPU->EIP);
			e = uc_emu_stop(ud->currentEngine);
			helper_Transfer(ud->uc16,ud->cpu16,ud->uc32,ud->cpu32);
			ud->currentEngine = ud->uc32;
			ud->currentCPU = ud->cpu32;
		}
		else {
			ud->currentCPU->inProtectedMode = false;
			uc_ctl_flush_tb(uc);
			uc_reg_read(ud->currentEngine, UC_X86_REG_EIP, &ud->currentCPU->EIP);
			changing = true;
			ok(uc_emu_start(ud->currentEngine, ud->currentCPU->EIP, 0, 0, 1));
			changing = false;
			uc_reg_read(ud->currentEngine, UC_X86_REG_EIP, &ud->currentCPU->EIP);
			uc_emu_stop(ud->currentEngine);
			helper_Transfer(ud->uc32,ud->cpu32,ud->uc16,ud->cpu16);
			ud->currentEngine = ud->uc16;
			ud->currentCPU = ud->cpu16;
		}
	}
	else {
		return;
	}
}
void PerBlockHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
	/*if (IRQ.stop_req.load() && ((ud->cpu->Eflag >> 9) & 1)) {
		IRQ.stop_req.store(false);
		uc_emu_stop(uc);
	}*/
	CPUctx* ctx = (CPUctx*)user_data;

	uc_reg_read(ctx->currentEngine, UC_X86_REG_CR0, &ctx->currentCPU->CR0);
	/*bool currPE = (ctx->currentCPU->CR0 >> 0) & 1;
	if (currPE != deltaMode &&!changing) {
		deltaMode = currPE;
		uc_hook_add(ctx->currentEngine, &(ctx->hookdata.ModeSwitch), UC_HOOK_CODE, ProtectedModeSwitchHook, (void*)ctx, 1,0);
	}*/
}
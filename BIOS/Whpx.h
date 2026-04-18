#pragma once
#include "Global.h"
#include "memory.h"
#include "CPU.h"
struct whCPUctx {
	static short VPcount;
	//TODO: shift this to hypervisor and keep refrence here only
	static WHV_PARTITION_HANDLE Partition;
	short vpindex;
	CPU* cpu32;
	whCPUctx(WHV_PARTITION_HANDLE partition) {
		cpu32 = new CPU32();
		ok(WHvCreateVirtualProcessor(partition, VPcount, 0));
		vpindex = VPcount;
		VPcount++;
		WHV_REGISTER_NAME  names[10];
		WHV_REGISTER_VALUE values[10];

		names[0] = WHvX64RegisterCr0;
		values[0].Reg64 = 0x00;  // PE=0, ET=1

		// CS — real mode segment, base 0xFFFF0, limit 0xFFFF
		names[1] = WHvX64RegisterCs;
		values[1].Segment.Base = 0xF0000;
		values[1].Segment.Limit = 0xFFFF;
		//values[1].Segment.Selector = 0xF000;
		values[1].Segment.Attributes = 0x9b;  // present, code, readable
		names[2] = WHvX64RegisterRip;
		values[2].Reg64 = 0xFFF0;

		names[3] = WHvX64RegisterSs;
		values[3].Segment.Base = 0;
		values[3].Segment.Limit = 0xFFFF;
		values[3].Segment.Selector = 0;
		values[3].Segment.Attributes = 0x93;

		// DS, ES, FS, GS
		names[4] = WHvX64RegisterDs;
		values[4].Segment.Base = 0; values[4].Segment.Limit = 0xFFFF;
		values[4].Segment.Selector = 0; values[4].Segment.Attributes = 0x93;

		names[5] = WHvX64RegisterEs;
		values[5] = values[4];

		names[6] = WHvX64RegisterFs;
		values[6] = values[4];

		names[7] = WHvX64RegisterGs;
		values[7] = values[4];

		// EFLAGS
		names[8] = WHvX64RegisterRflags;
		values[8].Reg64 = 0x002;  // reserved bit set, IF=0

		names[9] = WHvX64RegisterGdtr;
		values[9].Table.Base = 0;
		values[9].Table.Limit = 0xFFFF;

		ok(WHvSetVirtualProcessorRegisters(partition, vpindex, names, 10, values));
	}
	~whCPUctx() {
		WHvDeleteVirtualProcessor(Partition, vpindex);
		delete cpu32;
	}
};

class WHPX {
	WHV_PARTITION_HANDLE partition;
	void* ud;
	static void* currentCtx;
	std::atomic<bool> InterruptLock;
public:
	whCPUctx* cpuctx;
	WHPX(void* user_data_in_out);
	~WHPX();
	void RunVP();
	void StopVP();
	void GVAtoGPA(WHV_GUEST_VIRTUAL_ADDRESS va,WHV_GUEST_PHYSICAL_ADDRESS* pa);
	void RaiseInterrupt(P_INTERRUPT_TYPE interr);
	void EnableStepMode();
	void BumpRIP(WHV_RUN_VP_EXIT_CONTEXT* exit_ctx);
	void InterruptWindow(int irqN);
	static void ThunkGVAtoGPA(void* ctx	,WHV_GUEST_VIRTUAL_ADDRESS va, WHV_GUEST_PHYSICAL_ADDRESS* pa);
	static void ThunkRaiseInterrupt(P_INTERRUPT_TYPE interr);

	//WinHvEmulator apis
private:
	WHV_EMULATOR_HANDLE emuH;
public:
	static HRESULT ThunkGetVPReg(_In_ VOID* Context,_In_reads_(RegisterCount) const WHV_REGISTER_NAME* RegisterNames,_In_ UINT32 RegisterCount,_Out_writes_(RegisterCount) WHV_REGISTER_VALUE* RegisterValues);
	static HRESULT ThunkSetVPReg(_In_ VOID* Context,_In_reads_(RegisterCount) const WHV_REGISTER_NAME* RegisterNames,_In_ UINT32 RegisterCount,_In_reads_(RegisterCount) const WHV_REGISTER_VALUE* RegisterValues);
	static HRESULT ThunkVAtoPA(_In_ VOID* Context,_In_ WHV_GUEST_VIRTUAL_ADDRESS GvaPage,_In_ WHV_TRANSLATE_GVA_FLAGS TranslateFlags,_Out_ WHV_TRANSLATE_GVA_RESULT_CODE* TranslationResult,_Out_ WHV_GUEST_PHYSICAL_ADDRESS* GpaPage);
	static HRESULT ThunkIOport(_In_ VOID* Context,	_Inout_ WHV_EMULATOR_IO_ACCESS_INFO* IoAccess);
	static HRESULT ThunkMemory(_In_ VOID* Context,_Inout_ WHV_EMULATOR_MEMORY_ACCESS_INFO* MemoryAccess);
};
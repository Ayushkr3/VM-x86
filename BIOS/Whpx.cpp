#include "Whpx.h"
short whCPUctx::VPcount = 0;
WHV_PARTITION_HANDLE whCPUctx::Partition;
void* WHPX::currentCtx = nullptr;

static std::atomic<int> requestedIRQ{ -1 };
void WHPX::RaiseInterrupt(P_INTERRUPT_TYPE interr) {
    //This function have to bashed again and again if it is comming from PIC
    if (interr.isIOAPIC) {
        WHV_INTERRUPT_CONTROL ctrl = {};
        ctrl.Destination = interr.APIC_ID;
        ctrl.DestinationMode = WHvX64InterruptDestinationModeLogical;
        ctrl.TriggerMode = WHvX64InterruptTriggerModeEdge;
        ctrl.Type = WHvX64InterruptTypeFixed;
        int vec = pic.GetVectorFromIRQ(interr.irqN);
        if (vec == -1)return;
        ctrl.Vector = vec;
        ok(WHvRequestInterrupt(partition,&ctrl,sizeof(ctrl)));
        pic.ReleaseTimerLock();
        return;
    }
    WHV_REGISTER_NAME n = WHvX64RegisterDeliverabilityNotifications;
    WHV_REGISTER_VALUE v = {};    
    v.DeliverabilityNotifications.InterruptNotification = 1;
    ok(WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v));
    requestedIRQ.store(interr.irqN);
}

void WHPX::InterruptWindow(int irqN) {
    // check shadow
    WHV_REGISTER_NAME n = WHvRegisterInterruptState;
    WHV_REGISTER_VALUE v = {};
    ok(WHvGetVirtualProcessorRegisters(partition, 0, &n, 1, &v));
    if (v.InterruptState.InterruptShadow) {
        return;
    }
    int vec = pic.GetVectorFromIRQ(irqN);
    v = {};
    v.ExtIntEvent.EventPending = 1;
    v.ExtIntEvent.Vector = vec;
    if (vec == -1) {
        pic.ReleaseTimerLock();
        return;
    }
    n = WHvRegisterPendingEvent;
    v.ExtIntEvent.EventType = WHV_X64_PENDING_EVENT_TYPE::WHvX64PendingEventExtInt;
    ok(WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v));
    pic.ReleaseTimerLock();
}
void WHPX::RunVP() {
    
    WHV_RUN_VP_EXIT_CONTEXT exit_ctx = {};
    while (running) {
        exit_ctx = {};
        ok(WHvRunVirtualProcessor(partition, cpuctx->vpindex, &exit_ctx, sizeof(exit_ctx)));
        
        switch (exit_ctx.ExitReason)
        {
        case WHV_RUN_VP_EXIT_REASON::WHvRunVpExitReasonX64IoPortAccess: {
            //This will be slow but will ensure correctness
            WHV_EMULATOR_STATUS emuS;
            ok(WHvEmulatorTryIoEmulation(emuH, this, &exit_ctx.VpContext, &exit_ctx.IoPortAccess,&emuS));
            if (!emuS.EmulationSuccessful) {
                throw 0;
            }
            break;
        }
        case WHvRunVpExitReasonX64InterruptWindow: {
            InterruptWindow(requestedIRQ.load());
            break;
        }
        case WHvRunVpExitReasonMemoryAccess: {
            WHV_EMULATOR_STATUS emuS;
            //Slow as balls but will not be called frequently hopefully
            HRESULT hr = WHvEmulatorTryMmioEmulation(emuH, this, &exit_ctx.VpContext, &exit_ctx.MemoryAccess, &emuS);
            if (emuS.EmulationSuccessful) {

            }
            break;
        }
        case WHvRunVpExitReasonX64MsrAccess: {
            WHV_X64_MSR_ACCESS_CONTEXT msr = exit_ctx.MsrAccess;
            if (msr.AccessInfo.IsWrite) {
                hook_wrmsr(msr.MsrNumber,msr.Rax,msr.Rdx);
            }
            else {
                uint32_t rax=0;
                uint32_t rdx=0;
                hook_rdmsr(msr.MsrNumber,&rax,&rdx);
                WHV_REGISTER_NAME names[2] = { WHvX64RegisterRax ,WHvX64RegisterRdx };
                WHV_REGISTER_VALUE vals[2] = {0};
                vals[0].Reg32 = rax;
                vals[1].Reg32 = rdx;
                ok(WHvSetVirtualProcessorRegisters(partition, cpuctx->vpindex, names, 2, vals));
            }
            BumpRIP(&exit_ctx);
            break;
        }
        default:
            WHV_REGISTER_NAME names = WHvX64RegisterDeliverabilityNotifications;
            WHV_REGISTER_VALUE vals;
            WHvGetVirtualProcessorRegisters(partition, 0, &names, 1, &vals);
            PRINT_HEX(exit_ctx.VpContext.Cs.Base);
            PRINT_HEX(exit_ctx.VpContext.Rip);
            std::cout << "WHPX Exit: " << exit_ctx.ExitReason << std::endl;
            break;
        }
    }
}
void WHPX::BumpRIP(WHV_RUN_VP_EXIT_CONTEXT* exit_ctx) {
    WHV_REGISTER_NAME names = WHvX64RegisterRip;
    WHV_REGISTER_VALUE vals = {};
    vals.Reg64 = exit_ctx->VpContext.Rip + exit_ctx->VpContext.InstructionLength;
    ok(WHvSetVirtualProcessorRegisters(partition, 0, &names, 1, &vals));
}
void WHPX::ThunkGVAtoGPA(void* ctx, WHV_GUEST_VIRTUAL_ADDRESS va, WHV_GUEST_PHYSICAL_ADDRESS* pa) {
    WHPX* c = (WHPX*)ctx;
    c->GVAtoGPA(va, pa);
}
void WHPX::ThunkRaiseInterrupt(P_INTERRUPT_TYPE interr) {
    if (WHPX::currentCtx == nullptr)
        throw "Undefined";
    WHPX* c = (WHPX*)WHPX::currentCtx;
    c->RaiseInterrupt(interr);
}

void WHPX::StopVP() {
    WHvCancelRunVirtualProcessor(partition, cpuctx->vpindex, 0);
}
WHPX::~WHPX() {
    delete cpuctx;
    WHvUnmapGpaRange(partition, 0, RAM_SIZE);
    WHvEmulatorDestroyEmulator(emuH);
    WHvDeletePartition(partition);
}
void WHPX::GVAtoGPA(WHV_GUEST_VIRTUAL_ADDRESS va, WHV_GUEST_PHYSICAL_ADDRESS* pa) {
    WHV_TRANSLATE_GVA_FLAGS flags = WHvTranslateGvaFlagValidateRead;
    WHV_TRANSLATE_GVA_RESULT res;
    WHvTranslateGva(partition, cpuctx->vpindex, va, flags, &res, pa);
    if (res.ResultCode != WHvTranslateGvaResultSuccess) {
        throw res.ResultCode;
        pa = nullptr;
    }
}
void WHPX::EnableStepMode() {
    WHV_REGISTER_NAME n = WHvX64RegisterRflags;
    WHV_REGISTER_VALUE v = {};
    WHvGetVirtualProcessorRegisters(partition, 0, &n, 1, &v);
    v.Reg64 |= (1 << 8);
    WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v);
}
WHPX::WHPX(void* user_data_in_out) {
    InterruptLock.store(true);
    ok(WHvCreatePartition(&partition));
    WHV_PARTITION_PROPERTY property{};

    property.ProcessorCount = 1;
    WHV_PARTITION_PROPERTY_CODE code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeProcessorCount;
    ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));

    property = {};
    property.ApicRemoteRead = true;
    code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeApicRemoteReadSupport;
    //ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));

    property = {};
    property.LocalApicEmulationMode = WHV_X64_LOCAL_APIC_EMULATION_MODE::WHvX64LocalApicEmulationModeXApic;
    code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeLocalApicEmulationMode;
    ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));
    
    property = {};
    WHV_X64_MSR_EXIT_BITMAP msr_b = {};
    msr_b.UnhandledMsrs = 1;
    msr_b.TscMsrWrite = 0;
    msr_b.TscMsrRead = 0;
    msr_b.ApicBaseMsrWrite = 0;
    property.X64MsrExitBitmap = msr_b;
    code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeX64MsrExitBitmap;
    ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));

    property = {};
    //property.ExtendedVmExits.ExceptionExit = 1;
    property.ExtendedVmExits.X64MsrExit = 1;
    code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeExtendedVmExits;
    ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));

    
    WHV_PROCESSOR_FEATURES_BANKS processor_features = {};
    processor_features.BanksCount = 2;
    uint32_t written;
    ok(WHvGetCapability(WHvCapabilityCodeProcessorFeaturesBanks,&processor_features,sizeof(WHV_PROCESSOR_FEATURES_BANKS),&written));

    //ok(WHvSetPartitionProperty(partition,WHvPartitionPropertyCodeProcessorFeaturesBanks,&processor_features,sizeof(WHV_PROCESSOR_FEATURES_BANKS)));

    property = {};
    property.ExceptionExitBitmap = 0;
    //property.ExceptionExitBitmap =
    //    //(1ull << WHvX64ExceptionTypeDebugTrapOrFault) |
    //    //(1ull << WHvX64ExceptionTypeBreakpointTrap) |
    //    //(1ull << WHvX64ExceptionTypeInvalidOpcodeFault) |
    //    //(1ull << WHvX64ExceptionTypeGeneralProtectionFault) |
    //    //(1ull << WHvX64ExceptionTypePageFault);
    ok(WHvSetPartitionProperty(partition, WHvPartitionPropertyCodeExceptionExitBitmap, &property, sizeof(property)));



    ok(WHvSetupPartition(partition));

    WHV_MAP_GPA_RANGE_FLAGS flags = WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagExecute | WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagRead | WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagWrite;
    ok(WHvMapGpaRange(partition, RAM, 0, RAM_SIZE, flags));
    //wtf
    flags = WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagRead | WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagWrite;
    ok(WHvMapGpaRange(partition, SVGA, SVGA_BASE, SVGA_SIZE,flags));
    flags = WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagExecute | WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagRead | WHV_MAP_GPA_RANGE_FLAGS::WHvMapGpaRangeFlagWrite;
    ok(WHvMapGpaRange(partition, VGA_ROM, VGABIOS_BASE, VGABIOS_SIZE, flags));
    cpuctx = new whCPUctx(partition);

    UD* ux = (UD*)user_data_in_out;
    ud = user_data_in_out;
    currentCtx = this;

    //Hypervisiors api after this
    WHV_EMULATOR_CALLBACKS callbacks = {};
    callbacks.Size = sizeof(callbacks);
    callbacks.WHvEmulatorGetVirtualProcessorRegisters = ThunkGetVPReg;
    callbacks.WHvEmulatorSetVirtualProcessorRegisters = ThunkSetVPReg;
    callbacks.WHvEmulatorMemoryCallback = ThunkMemory;
    callbacks.WHvEmulatorIoPortCallback = ThunkIOport;
    callbacks.WHvEmulatorTranslateGvaPage = ThunkVAtoPA;
    ok(WHvEmulatorCreateEmulator(&callbacks, &emuH));
}

HRESULT WHPX::ThunkGetVPReg(_In_ VOID* Context, _In_reads_(RegisterCount) const WHV_REGISTER_NAME* RegisterNames, _In_ UINT32 RegisterCount, _Out_writes_(RegisterCount) WHV_REGISTER_VALUE* RegisterValues)
{
    WHPX* localctx = (WHPX*)Context;
    ok(WHvGetVirtualProcessorRegisters(localctx->partition, localctx->cpuctx->vpindex, RegisterNames, RegisterCount, RegisterValues));
    return S_OK;
}
HRESULT WHPX::ThunkSetVPReg(_In_ VOID* Context, _In_reads_(RegisterCount) const WHV_REGISTER_NAME* RegisterNames, _In_ UINT32 RegisterCount, _In_reads_(RegisterCount) const WHV_REGISTER_VALUE* RegisterValues)
{
    WHPX* localctx = (WHPX*)Context;
    ok(WHvSetVirtualProcessorRegisters(localctx->partition, localctx->cpuctx->vpindex, RegisterNames, RegisterCount, RegisterValues));
    return S_OK;
}
HRESULT WHPX::ThunkVAtoPA(_In_ VOID* Context, _In_ WHV_GUEST_VIRTUAL_ADDRESS GvaPage, _In_ WHV_TRANSLATE_GVA_FLAGS TranslateFlags, _Out_ WHV_TRANSLATE_GVA_RESULT_CODE* TranslationResult, _Out_ WHV_GUEST_PHYSICAL_ADDRESS* GpaPage)
{
    WHPX* localctx = (WHPX*)Context;
    WHV_TRANSLATE_GVA_RESULT res;

    ok(WHvTranslateGva(localctx->partition, localctx->cpuctx->vpindex, GvaPage, TranslateFlags, &res, GpaPage));
    if (res.ResultCode != WHvTranslateGvaResultSuccess) {
        throw res.ResultCode;
    }
    *TranslationResult = res.ResultCode;
    return S_OK;
}
HRESULT WHPX::ThunkIOport(_In_ VOID* Context, _Inout_ WHV_EMULATOR_IO_ACCESS_INFO* IoAccess)
{
    WHPX* localctx = (WHPX*)Context;
    if (IoAccess->Direction) {
        hook_out(IoAccess->Port, IoAccess->AccessSize, IoAccess->Data, localctx->ud);
    }
    else {
        IoAccess->Data = hook_in(IoAccess->Port,IoAccess->AccessSize,localctx->ud);
    }
    return S_OK;
}
HRESULT WHPX::ThunkMemory(_In_ VOID* Context, _Inout_ WHV_EMULATOR_MEMORY_ACCESS_INFO* MemoryAccess)
{
    WHPX* localctx = (WHPX*)Context;
    if (MemoryAccess->Direction) {
        hook_mmio_out(MemoryAccess->GpaAddress, MemoryAccess->Data, MemoryAccess->AccessSize, &localctx->cpuctx->cpu32->lapic);
    }
    else {
        hook_mmio_in(MemoryAccess->GpaAddress, MemoryAccess->Data, MemoryAccess->AccessSize, &localctx->cpuctx->cpu32->lapic);
    }
    return S_OK;
}
#include "Whpx.h"
short whCPUctx::VPcount = 0;
WHV_PARTITION_HANDLE whCPUctx::Partition;
void* WHPX::currentCtx = nullptr;

static int testN=-1;
void WHPX::RaiseInterrupt(int intN) {
    WHV_REGISTER_NAME n = WHvX64RegisterDeliverabilityNotifications;
    WHV_REGISTER_VALUE v = {};
    v.DeliverabilityNotifications.InterruptNotification = 1;
    WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v);
    testN = intN;
}

void WHPX::UnitTest(int intN) {
    WHV_REGISTER_NAME n = WHvRegisterPendingEvent;
    WHV_REGISTER_VALUE v = {};
    HRESULT hx=0;
 
    
    //hx = WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v);
    WHV_REGISTER_NAME names = WHvX64RegisterRflags;
    WHV_REGISTER_VALUE vals;
    WHvGetVirtualProcessorRegisters(partition, 0, &names, 1, &vals);
    uint64_t rflags = vals.Reg64;
    bool IF = (rflags & (1 << 9)) != 0;

    // Final decision
    bool interruptible = IF;

    if (!interruptible)
        return;
    n = WHvRegisterPendingEvent;
    v = {};
    v.ExtIntEvent.EventPending = 1;
    v.ExtIntEvent.EventType = WHvX64PendingEventExtInt;
    v.ExtIntEvent.Vector = intN;
    
    ok(WHvSetVirtualProcessorRegisters(partition, 0, &n, 1, &v));
    //EnableStepMode();
}
void WHPX::RunVP() {
    WHV_RUN_VP_EXIT_CONTEXT exit_ctx = {};
    while (running) {
        exit_ctx = {};
        ok(WHvRunVirtualProcessor(partition, cpuctx->vpindex, &exit_ctx, sizeof(exit_ctx)));
        
        switch (exit_ctx.ExitReason)
        {
        case WHV_RUN_VP_EXIT_REASON::WHvRunVpExitReasonX64IoPortAccess: {
            //TODO: shift this to WhvEmulator
            WHV_EMULATOR_STATUS emuS;
            ok(WHvEmulatorTryIoEmulation(emuH, this, &exit_ctx.VpContext, &exit_ctx.IoPortAccess,&emuS));
            if (!emuS.EmulationSuccessful) {
                throw 0;
            }
            break;
        }
        case WHvRunVpExitReasonX64InterruptWindow: {
            UnitTest(testN);
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
        default:
            PRINT_HEX(exit_ctx.VpContext.Cs.Base);
            PRINT_HEX(exit_ctx.VpContext.Rip);
            std::cout << "WHPX Exit: " << exit_ctx.ExitReason << std::endl;
            break;
        }
    }
}
void WHPX::ThunkGVAtoGPA(void* ctx, WHV_GUEST_VIRTUAL_ADDRESS va, WHV_GUEST_PHYSICAL_ADDRESS* pa) {
    WHPX* c = (WHPX*)ctx;
    c->GVAtoGPA(va, pa);
}
void WHPX::ThunkRaiseInterrupt(int intN) {
    if (WHPX::currentCtx == nullptr)
        throw "Undefined";
    WHPX* c = (WHPX*)WHPX::currentCtx;
    c->RaiseInterrupt(intN);
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
    WHV_X64_MSR_EXIT_BITMAP msr_b;
    msr_b.UnhandledMsrs = 1;
    msr_b.TscMsrWrite = 1;
    msr_b.TscMsrRead = 1;
    msr_b.ApicBaseMsrWrite = 1;
    //property.X64MsrExitBitmap = msr_b;
    //code = WHV_PARTITION_PROPERTY_CODE::WHvPartitionPropertyCodeX64MsrExitBitmap;
    //ok(WHvSetPartitionProperty(partition, code, &property, sizeof(property)));

    property = {};
    property.ExtendedVmExits.ExceptionExit = 1;
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
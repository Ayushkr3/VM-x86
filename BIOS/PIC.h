#pragma once
#include <chrono>
#include <cstdint>
#include <atomic>
#include "APIC.h"

struct P_INTERRUPT_TYPE {
    short irqN = -1;
    bool isIOAPIC = false;
    short APIC_ID = -1;
    //short TypeMode;
};
typedef void (*RaiseIRQ_f)(P_INTERRUPT_TYPE intT);
class PICState {
    static RaiseIRQ_f raiseInt_fwd;
public:
    //Callback to raise interrupt
    uint8_t master_offset = 0x8;
    uint8_t slave_offset = 0x70;

    uint8_t master_icw_state = 0;
    uint8_t slave_icw_state = 0;

    std::atomic<uint8_t> master_mask = 0xff;
    std::atomic<uint8_t> slave_mask = 0xff;

    std::atomic<uint32_t> fire_rate{ 50 };
    std::atomic<uint32_t> divisor{ 1 };
    std::atomic<uint64_t> irq0_tick_count{ 0 };
    std::atomic<bool> reset_timing{ false };

    std::atomic<bool> TimerLock = false;
    void RaiseIRQ(int irqN);
    void ReleaseTimerLock();
    int GetVectorFromIRQ(int irqN);
    static void Init(RaiseIRQ_f rfwd);
    static std::atomic<int> nonTimerInt;
    static std::atomic<bool> rtc_irq_pending;
};
extern PICState pic;
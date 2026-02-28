#pragma once
#include <chrono>
#include "Global.h"
//Forwarded declaration from PIC.h and APIC.h
extern RaiseIRQ RaiseIRQ_fwd;
struct PICState {
    uint8_t master_offset = 0x14;
    uint8_t slave_offset = 0x1c;

    uint8_t master_icw_state = 0;
    uint8_t slave_icw_state = 0;

    std::atomic<uint8_t> master_mask = 0xff;
    std::atomic<uint8_t> slave_mask = 0xff;

    std::atomic<uint32_t> fire_rate{ 50 };
    std::atomic<uint32_t> divisor{ 5000 };
    std::atomic<uint64_t> irq0_tick_count{ 0 };
    std::atomic<bool> reset_timing{ false };
    bool rtc_period_enabled = true;
    uint32_t rtc_period = 1;

};
void PITThread(PICState* pic);
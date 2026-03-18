#pragma once
#include <chrono>
#include <cstdint>
#include <atomic>
extern bool test;
typedef void (*RaiseIRQ)(int vector);
class PICState {
public:
    //Callback to raise interrupt
    static RaiseIRQ raiseInt_fwd;
    uint8_t master_offset = 0x8;
    uint8_t slave_offset = 0x70;

    uint8_t master_icw_state = 0;
    uint8_t slave_icw_state = 0;

    std::atomic<uint8_t> master_mask = 0xff;
    std::atomic<uint8_t> slave_mask = 0xff;

    std::atomic<uint32_t> fire_rate{ 50 };
    std::atomic<uint32_t> divisor{ 5000 };
    std::atomic<uint64_t> irq0_tick_count{ 0 };
    std::atomic<bool> reset_timing{ false };
    bool rtc_period_enabled = true;
    uint32_t rtc_period = 50;

    bool IRQRaised=false;
    void RaiseIRQ(int irqN);
};
extern PICState pic;
void PITThread(PICState* pic);
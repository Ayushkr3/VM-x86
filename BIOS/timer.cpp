//This file will contain all VM timer related feature
//It will contain
//IOAPIC redirection
//PIC redirections
//CMOS timers
//HPET timer if needed
//PM timer if needed


#include "timer.h"
#include <chrono>
#include <thread>
#define PIT_CLOCK 1193182 

using namespace std::chrono;

bool Timers::rtc_period_enabled = false;
uint32_t Timers::rtc_period = 50;

uint8_t Timers::PICsmask[3] = { 0xff};

PICState* Timers::picS = nullptr;
IOAPICState* Timers::ioapicS= nullptr;
std::chrono::steady_clock::time_point start_time;
double now() {
    static const auto start = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
}

void Timers::Init() {
	picS = &pic;
	ioapicS = &ioapic;
    start_time = std::chrono::high_resolution_clock::now();
}
void Timers::TimerThread() {
    double pit_next = now();
    double rtc_next = now();
    while (running.load()) {
        double current = now();
        if (current >= pit_next)
        {
            uint16_t div = picS->divisor;
            if (div == 0) div = 65536;

            double pit_period = div / PIT_CLOCK;
            SelectIRQandRaiseInterrupt(0x0);
            if (pit_next < current - pit_period * 2) {
                pit_next = current + pit_period; 
            }
        }
        if (rtc_period_enabled && current >= rtc_next)
        {
            SelectIRQandRaiseInterrupt(0x8);
            if (rtc_next < current - rtc_period * 2) {
                rtc_next = current + rtc_period;
            }
        }
        double next_wake = pit_next;
        if (rtc_period_enabled)
            next_wake = min(next_wake, rtc_next);

        double sleep_sec = next_wake - now();
        Sleep(64);
        if (sleep_sec > 0) {
            auto sleep_us = std::chrono::duration<double>(sleep_sec);
            std::this_thread::sleep_for(sleep_us);
        }
    }
}
void Timers::SelectIRQandRaiseInterrupt(int irqN) {
    picS->RaiseIRQ(irqN);
}
void Timers::UpdateMasks(int mask, TIMER_MASK_TYPE type) {
    PICsmask[type] = mask;
}
uint32_t Timers::GetCMOSregA() {
    static bool UIP = true;
    uint32_t return_val=0;
    //We switch between UIP bits
    if (UIP) {
        return_val = 0xa6;
        UIP = false;
    }
    else {
        return_val = 0x26;
        UIP = true;
    }
    return return_val;
}
uint32_t Timers::GetCMOSregB() {
    return 0x02;
}
uint32_t Timers::GetCMOSregC(){
    if (picS->rtc_irq_pending.load()) {
        picS->rtc_irq_pending.store(false);  // EOI
        //std::cout << "EOI" << std::endl;
        return 0xC0;  // IRQF + PF set
    }
    return 0x00;
}
uint32_t Timers::GetCMOSregD(){
    return 0x80;
}

uint32_t Timers::GetPMTimer() {
    // Get current time in nanoseconds
    auto now = std::chrono::high_resolution_clock::now();
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>
        (now - start_time).count();

    uint64_t ticks = (uint64_t)ns * 3579545 / 1000000000;
    return (uint32_t)(ticks & 0x00FFFFFF);
}
uint32_t Timers::ReturnCMOSData(int index) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    tm tm_info = {};
    localtime_s(&tm_info,&t);

    auto toBCD = [](int val) -> uint8_t {
        return ((val / 10) << 4) | (val % 10);
    };
    switch (index) {
    case 0x00: return toBCD(tm_info.tm_sec);   // seconds
    case 0x02: return toBCD(tm_info.tm_min);   // minutes
    case 0x04: return toBCD(tm_info.tm_hour);  // hours
    case 0x06: return toBCD(tm_info.tm_wday + 1); // day of week (1-7)
    case 0x07: return toBCD(tm_info.tm_mday);  // day of month
    case 0x08: return toBCD(tm_info.tm_mon + 1); // month
    case 0x09: return toBCD(tm_info.tm_year % 100); // year (2 digits)
    case 0x32: return toBCD((tm_info.tm_year + 1900) / 100); // century
    }
    return 0;
}
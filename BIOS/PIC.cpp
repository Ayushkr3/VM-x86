#include"PIC.h"
#include <chrono>
#include <thread>
#include"memory.h"
#define PIT_CLOCK 1193182 
//Forwarded declaration from PIC.h and APIC.h
RaiseIRQ RaiseIRQ_fwd;
using namespace std::chrono;
double now() {
    static const auto start = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - start).count();
}
void PITThread(PICState* pic) {
    double pit_next = now();
    double rtc_next = now();
	while (running.load()) {
        double current = now();
        if (current >= pit_next)
        {
            uint16_t div = pic->divisor;
            if (div == 0) div = 65536;

            double pit_period = div / PIT_CLOCK;
            bool masked = pic->master_mask & 1;
            if (!masked) {
                //RaiseIRQ_fwd(pic->master_offset);
            }
            pit_next += pit_period;
        }
        if (pic->rtc_period_enabled && current >= rtc_next)
        {
            bool masked = pic->slave_mask & 1;
            if (!masked) {
                RaiseIRQ_fwd(pic->slave_offset);
            }
            rtc_next += pic->rtc_period;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}
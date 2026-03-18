#include"PIC.h"
#include <chrono>
#include <thread>
#include"memory.h"
#define PIT_CLOCK 1193182 
PICState pic;
RaiseIRQ PICState::raiseInt_fwd;
using namespace std::chrono;
bool test = true;
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
                if(test)
                pic->RaiseIRQ(0x0);
            }
            pit_next += pit_period;
        }
        if (pic->rtc_period_enabled && current >= rtc_next)
        {
            bool masked = pic->slave_mask & 1;
            if (!masked) {
                //pic->RaiseIRQ(0x8);
            }
            rtc_next += pic->rtc_period;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
void PICState::RaiseIRQ(int irqN)
{
    bool masked;
    int vector=-1;

    if (irqN < 8)
    {
        masked = (master_mask >> irqN) & 1;
        if (masked)
            return;

        vector = master_offset + irqN;
    }
    else
    {
        int slave_irq = irqN - 8;

        masked = (slave_mask >> slave_irq) & 1;
        if (masked)
            return;

        // Slave interrupt also requires cascade through master IRQ2
        if ((master_mask >> 2) & 1)
            return;

        vector = slave_offset + slave_irq;
    }
    IRQRaised = true;
    raiseInt_fwd(vector);
}
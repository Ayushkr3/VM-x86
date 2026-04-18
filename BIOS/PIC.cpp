#include"PIC.h"
#include <chrono>
#include <thread>
#include"memory.h"
PICState pic;
RaiseIRQ_f PICState::raiseInt_fwd;
using namespace std::chrono;
std::atomic<int> PICState::nonTimerInt = -1;

std::atomic<bool> PICState::rtc_irq_pending;

void PICState::Init(RaiseIRQ_f rfwd) {
    raiseInt_fwd = rfwd;
}
void PICState::RaiseIRQ(int irqN)
{
    //if we get disk or any other irq we should priortize that instead of time
    P_INTERRUPT_TYPE intT;
    auto lock = [&]()->bool {
        if (irqN != 0 && irqN != 0x8) {
            //Non timer interrupts
            TimerLock.store(true);
            nonTimerInt.store(irqN);
            return false;
        }
        else {
            //Timer interrupts
            if (TimerLock.load()) {
                return true;
            }
            return false;
        }
    };
    IOREDEntry pin = ioapic.redir[irqN];
    bool mask = (pin.low & LVT_MASK_BIT);
    if (!mask) {
        intT.irqN = irqN;
        intT.isIOAPIC = true;
        intT.APIC_ID = (pin.high >> 24) & 0xFFu,
        raiseInt_fwd(intT);
    }
    if (irqN < 8)
    {
        mask = (master_mask >> irqN) & 1;
        if (!mask) {
            if (lock()) {
                return;
            }
            intT.irqN = irqN;
            intT.isIOAPIC = false;
            raiseInt_fwd(intT);
        }
    }
    else
    {
        int slave_irq = irqN - 8;
        mask = (slave_mask >> slave_irq) & 1;
        if (!(mask && (master_mask >> 2) & 1)) {
            if (lock()) {
                return;
            }
            intT.irqN = irqN;
            intT.isIOAPIC = false;
            raiseInt_fwd(intT);
        }
    }
    return;
}
void PICState::ReleaseTimerLock() {
    if (nonTimerInt != -1) {
        TimerLock.store(false);
        nonTimerInt = -1;
    }
}

int PICState::GetVectorFromIRQ(int irqN) {
    IOREDEntry pin = ioapic.redir[irqN];
    bool mask = (pin.low & LVT_MASK_BIT);
    if (!mask) {
        uint32_t vector = pin.low & 0xFF;
         if (irqN == 0x8) {
                if (rtc_irq_pending.load()&&TimerLock.load()) {
                    return -1;
                }
           rtc_irq_pending.store(true);
         }
         return vector;
    }
    if (irqN < 8)
    {
        mask = (master_mask >> irqN) & 1;
        if (!mask) {
            int vector = master_offset + irqN;
            return vector;
        }
        else {
            return -1;
        }
    }
    else
    {
        int slave_irq = irqN - 8;

        mask = (slave_mask >> slave_irq) & 1;
        if (!(mask && (master_mask >> 2) & 1)) {
            int vector = slave_offset + slave_irq;
            return vector;
        }
        else {
            return -1;
        }
    }

}
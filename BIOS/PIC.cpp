#include"PIC.h"
#include <chrono>
#include <thread>
#include"memory.h"
PICState pic;
RaiseIRQ_f PICState::raiseInt_fwd;
using namespace std::chrono;
void PICState::Init(RaiseIRQ_f rfwd) {
    raiseInt_fwd = rfwd;
}
static int nonTimerInt=-1;
void PICState::RaiseIRQ(int irqN)
{
    //if we get disk or any other irq we should priortize that instead of time
    auto lock = [&]()->bool {
        if (irqN != 0 && irqN != 0x8) {
            //Non timer interrupts
            TimerLock.store(true);
            nonTimerInt = irqN;
            return false;
        }
        else {
            //Timer interrupts
            if (TimerLock.load()) {
                return true;
            }
        }
    };
    IOREDEntry pin = ioapic.redir[irqN];
    bool mask = (pin.low & LVT_MASK_BIT);
    if (!mask) {
        //irq 0 and 8
        if (lock()) {
            return;
        }
        raiseInt_fwd(irqN);
    }
    if (irqN < 8)
    {
        mask = (master_mask >> irqN) & 1;
        if (!mask) {
            if (lock()) {
                return;
            }
            raiseInt_fwd(irqN);
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
            raiseInt_fwd(irqN);
        }
    }
    return;
}
void PICState::ReleaseTimerLock() {
    if (nonTimerInt !=-1&&nonTimerInt!=0&& nonTimerInt!=8) {
        TimerLock.store(false);
        nonTimerInt = -1;
    }
}
int PICState::GetVectorFromIRQ(int irqN) {
    IOREDEntry pin = ioapic.redir[irqN];
    bool mask = (pin.low & LVT_MASK_BIT);
    if (!mask) {
        //irq 0 and 8
        uint32_t vector = pin.low & 0xFFu;
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
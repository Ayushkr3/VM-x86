#include "PIC.h"
std::function<void(IRQs)> HWcallback; //interupt callback
constexpr double PIT_FREQ_HZ = 18.20648;
IRQs IRQ;

std::atomic_bool IRQs::stop_req = false;

IRQs::IRQs() {
    io_request_pending = false;
    InitializeCriticalSection(&cs);
}
IRQs::~IRQs() {
    DeleteCriticalSection(&cs);
}
void SetPICCallback() {
    
}
void PICThread() {
        using namespace std::chrono;
        auto now = system_clock::now();
        double accumulated_ticks = 0.0;
        while (running)
        {
            std::this_thread::sleep_until(now + duration<double>(1.0 / 18.20648)*2.0);
            auto curr = system_clock::now();
            duration<double> elapsed = curr -now;
            now = curr;
            double ticks_exact = elapsed.count() * PIT_FREQ_HZ;
            accumulated_ticks += ticks_exact;
            EnterCriticalSection(&IRQ.cs);
            IRQ.irq_pending[0] = true;
            IRQ.stop_req.store(true);
            LeaveCriticalSection(&IRQ.cs);
        }
}
void HIOThread() {
    while (running)
    {
        Sleep(1000);
        EnterCriticalSection(&IRQ.cs);
        IRQ.irq_pending[1] = true;
        LeaveCriticalSection(&IRQ.cs);
        
    }
}
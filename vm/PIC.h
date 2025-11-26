#pragma once
#include <chrono>
#include "Global.h"
#include <thread>
#include <functional>
enum IRQsToINT {
    IRQ0 = 0x0
};
struct IRQs {
    CRITICAL_SECTION cs;          
    bool irq_pending[16];     
    bool io_request_pending;
    static std::atomic_bool stop_req;
    IRQs();
    ~IRQs();
};
extern IRQs IRQ;
void SetPICCallback();
void PICThread();
void HIOThread();
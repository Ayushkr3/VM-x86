#include "kd.h"
HANDLE  KernelDebugger::pipe_in = INVALID_HANDLE_VALUE;
HANDLE  KernelDebugger::pipe_out = INVALID_HANDLE_VALUE;
uint8_t KernelDebugger::rx_buf = 0x00;
bool    KernelDebugger::rx_ready = false;

uint8_t KernelDebugger::ier=0;
uint8_t KernelDebugger::lcr=0;
uint8_t KernelDebugger::mcr=0;
uint8_t KernelDebugger::scratch=0;
uint8_t KernelDebugger::baud_lo=0;
uint8_t KernelDebugger::baud_hi=0;
std::function<void()> KernelDebugger::callbac;

void KernelDebugger::com1_init_pipe(std::function<void()> callback) {
    pipe_out = CreateNamedPipeA(
        "\\\\.\\pipe\\xpserial",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
        1, 4096, 4096, 0, NULL);
    pipe_in = pipe_out;
    ConnectNamedPipe(pipe_out, NULL);
    callbac = callback;
}

void KernelDebugger::pipe_poll_rx() {
    if (rx_ready) return;
    if (pipe_in == INVALID_HANDLE_VALUE) return;
    DWORD avail = 0;
    if (!PeekNamedPipe(pipe_in, NULL, 0, NULL, &avail, NULL)) return;
    if (avail > 0) {
        DWORD read = 0;
        ReadFile(pipe_in, &rx_buf, 1, &read, NULL);
        if (read == 1) { 
            rx_ready = true; 
            //callbac(0x4);
        }
    }
}
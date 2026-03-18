#pragma once
#include <cstdint>
#include <Windows.h>

class KernelDebugger {
public:
	static HANDLE  pipe_in;
	static HANDLE  pipe_out;
	static uint8_t rx_buf;
	static bool    rx_ready;
	static uint8_t ier;
	static uint8_t lcr;
	static uint8_t mcr;
	static uint8_t scratch;
	static uint8_t baud_lo;
	static uint8_t baud_hi;
	static void com1_init_pipe();
	static void pipe_poll_rx();
};
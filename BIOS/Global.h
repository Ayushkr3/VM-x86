#pragma once


//All the helper garbage goes here


#include "unicorn.h"
#include <iostream>
#include <atomic>
#include <unordered_map>
#define ok(err) if(err!=UC_ERR_OK) throw UC_ERR_ARG
#define PRINT_HEX(val)std::cout<<std::hex<<val<<std::endl;

extern std::atomic<bool> running;

const int RegOrder32[]{
UC_X86_REG_EAX,
UC_X86_REG_ECX,
UC_X86_REG_EDX,
UC_X86_REG_EBX,
UC_X86_REG_ESP,
UC_X86_REG_EBP,
UC_X86_REG_ESI,
UC_X86_REG_EDI,
UC_X86_REG_ES,
UC_X86_REG_CS,
UC_X86_REG_SS,
UC_X86_REG_DS,
UC_X86_REG_FS,
UC_X86_REG_GS,
UC_X86_REG_EIP,
UC_X86_REG_CR0,
UC_X86_REG_CR1,
UC_X86_REG_CR2,
UC_X86_REG_CR3,
UC_X86_REG_CR4,
UC_X86_REG_EFLAGS,
};
const int RegOrder16[]{
UC_X86_REG_EAX,
UC_X86_REG_ECX,
UC_X86_REG_EDX,
UC_X86_REG_EBX,
UC_X86_REG_ESP,
UC_X86_REG_EBP,
UC_X86_REG_ESI,
UC_X86_REG_EDI,
UC_X86_REG_ES,
UC_X86_REG_CS,
UC_X86_REG_SS,
UC_X86_REG_DS,
UC_X86_REG_FS,
UC_X86_REG_GS,
UC_X86_REG_EIP,
UC_X86_REG_CR0,
UC_X86_REG_CR1,
UC_X86_REG_CR2,
UC_X86_REG_CR3,
UC_X86_REG_CR4,
UC_X86_REG_EFLAGS,
};



struct HookData {
	uc_hook INS_IN;
	uc_hook INS_OUT;
	uc_hook INS_INT;
	uc_hook INS_WRMSR;
	uc_hook INS_RDMSR;
	uc_hook INS_CPUID;
	uc_hook ModeSwitch;
};
struct HookUserData {
	void* pic;
};
enum DISK_TYPE {
	DVD = 0xe0,
	HDD0 = 0x80
};
constexpr DISK_TYPE allIO[] = { DVD, HDD0 };
struct IO {
	uint8_t portsVal[256] = { 0 };
	std::unordered_map<int, std::ifstream*> IO_P_B; //io port and buffer to data
	IO() {
		for (auto e : allIO) {
			IO_P_B[e] = nullptr;
		}
	}
};
struct IRQ {
	CRITICAL_SECTION cs;
	
	//Sparse Set
	std::atomic<short> irqNum = 0;
	std::atomic<bool> irqRaised = false;
	IRQ();
	~IRQ();
	void SetIRQ(int irqN);
};
typedef void (*RaiseIRQ)(int);
extern IRQ irq;
uint64_t get_ms_since_midnight();
struct HostDatenTime {
	int hour;
	int minute;
	int second;
	int century;
	int year;
	int month;
	int day;
};
HostDatenTime get_host_time();
static inline uint8_t to_bcd(int value) {
	return (uint8_t)(((value / 10) << 4) | (value % 10));
}
#pragma once


//All the helper garbage goes here
#include <Windows.h>
#include <WinHvPlatform.h>
#include <WinHvEmulation.h>
#include <iostream>
#include <atomic>
#include <unordered_map>
//#define ok(err) if(err!=UC_ERR_OK) throw UC_ERR_ARG
#define ok(err) if(err!=S_OK) throw err
#define PRINT_HEX(val) std::cout<<std::hex<<val<<std::endl;

#pragma comment(lib,"WinHvPlatform.lib")
#pragma comment(lib,"WinHvEmulation.lib")
extern std::atomic<bool> running;

typedef void(*ThunkTranslate)(void* ctx, WHV_GUEST_VIRTUAL_ADDRESS va, WHV_GUEST_PHYSICAL_ADDRESS* pa);

struct HookUserData {
	void* pic;
};
//constexpr DISK_TYPE allIO[] = { DVD, HDD0 };
struct IO {
	uint8_t portsVal[256] = { 0 };
	std::unordered_map<int, std::ifstream*> IO_P_B; //io port and buffer to data
	IO() {
		/*for (auto e : allIO) {
			IO_P_B[e] = nullptr;
		}*/
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

#pragma once
#include "PIC.h"
#include <functional>
enum TIMER_MASK_TYPE {
	PIC1,
	PIC2,
	IOAPIC
};
class Timers {
	struct CallbackData {
		std::function<void()> fn;
		int TimeinMS=0;
		int current=0;
	};
	//Should contain PIC
	//Should contain IOAPIC
	static PICState* picS;
	static IOAPICState* ioapicS;
	//Stores PIC1,PIC2,IOAPIC timer masks
	static uint8_t PICsmask[3];
	static uint32_t RegisterB;
	static std::vector<CallbackData> callbacks;
public:
	static uint32_t rtc_period;
	static bool rtc_period_enabled;
	static void Init();
	static void TimerThread();
	//This raise
	static void SelectIRQandRaiseInterrupt(int irqN);
	static void UpdateMasks(int mask, TIMER_MASK_TYPE type);
	static uint32_t ReturnCMOSData(int index);
	static uint32_t  GetPMTimer();
	static uint32_t GetCMOSregA();
	static uint32_t GetCMOSregB();
	static void SetCMOSregB(uint32_t val);
	static uint32_t GetCMOSregC();
	static uint32_t GetCMOSregD();
	static void RegisterFixedIntervalCallback(std::function<void()>fn,int timeinMS);
};
#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <thread>
#include <queue>
#include <unordered_map>
#include "CPU.h"
#include <time.h>
#include "unic.h"
#include "PIC.h"
#include "Global.h"
extern std::mutex Int_m;
extern std::condition_variable cv;
extern std::string diskPath;

struct InteruptStruct {
	int num;
	CPU* pcpu = nullptr;
};
enum DISK_TYPE {
	DVD = 0xe0,
	HDD0 = 0x80
};
constexpr DISK_TYPE allIO[] = { DVD, HDD0 };
struct IO {
	std::unordered_map<int, std::ifstream*> IO_P_B; //io port and buffer to data
	IO() {
		for (auto e : allIO) {
			IO_P_B[e] = nullptr;
		}
	}
};
struct BasicReadWriteInfo {
	uint64_t address=0;
	int size=0;
	BasicReadWriteInfo() { }
	BasicReadWriteInfo(uint64_t address,
	int size) :address(address), size(size) {

	}
};

HostDatenTime get_host_time();
extern IO ports;
extern std::queue<InteruptStruct> jobQ;
extern bool shutdown_t;
char* ReadFile(std::string path);
void LoadDisk(std::string path, DISK_TYPE type);
void BootDisk();
void ReadHDD(uint16_t cyl, uint8_t head, uint16_t numberofSec,uint8_t sector,int index,UINT64 RAMloc, UnicornData* ud);

void RemoveDisk(int index);
void ReadIODisk(int index, long long DAP, UnicornData* ud);
void IOLookUp(InteruptStruct in, UnicornData* ud);
void HardwareIntHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data);
void HardwareInt(int nums,UnicornData* ud);
void InitIO(uc_engine** uc);
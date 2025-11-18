#pragma once
#include <Windows.h>
#include <iostream>
#include "xed_table.h"
#include <intrin.h>
#include "unicorn.h"
extern bool isIn32Mode;
union ModRM {
	uint8_t byte;
	struct {
		uint8_t rm : 3;
		uint8_t reg : 3;
		uint8_t mod : 2;
	};
};
union SIB {
	uint8_t byte;
	struct {
		uint8_t base : 3;
		uint8_t index : 3;
		uint8_t scale : 2;
	};
};
struct CPU {
	INT64 RAX = 0; //A
	INT64 RBX = 0; //B
	INT64 RCX = 0; //C
	INT64 RDX = 0; //D
	INT64 RBP = 0; //Base pointer
	INT64 RSP = 0; //Stack pointer
	INT64 RSI = 0; //
	INT64 RDI = 0;
	INT64 RIP = 0; //PC

	INT64 GS=0;
	INT64 FS=0;
	INT64 ES=0;
	INT64 DS=0;
	INT64 CS=0;
	INT64 SS=0;

	INT64 CR0 = 0;
	INT64 CR1 = 0;
	INT64 CR2 = 0;
	INT64 CR3 = 0;
	INT64 CR4 = 0;
	INT64 CR8 = 0;

	bool CF = false;
	bool PF = false;
	bool OF = false;
	bool AF = false;
	bool DF = false;
	bool ZF = false;
	bool SF = false;
	bool TF = false;
	bool IF = false;
	struct gdtr{
		uint32_t selector = 0;
		uint64_t base=0;
		uint32_t limit=0;
		uint32_t flags=0;
	};
	gdtr GDT;
	INT64 Eflag=0;
	INT64* Reg[21];
	CPU() {
		Reg[0] = &RAX;
		Reg[1] = &RCX;
		Reg[2] = &RDX;
		Reg[3] = &RBX;
		Reg[4] = &RSP;
		Reg[5] = &RBP;
		Reg[6] = &RSI;
		Reg[7] = &RDI;
		Reg[14] = &RIP;
		//Segment
		Reg[8] = &ES;
		Reg[9] = &CS;
		Reg[10] = &SS;
		Reg[11] = &DS;
		Reg[12] = &FS;
		Reg[13] = &GS;
		//Control
		Reg[15] = &CR0;
		Reg[16] = &CR1;
		Reg[17] = &CR2;
		Reg[18] = &CR3;
		Reg[19] = &CR4;
		Reg[20] = &Eflag;
	}
	virtual void SetRegOrder(const int* _RegOrder)=0;
	virtual const int* GetRegOrder() = 0;
	virtual const int  GetNumberofRegister() = 0;
	const int* RegOrder;
	virtual uint64_t GetFlatMemoryIP() = 0;
};
struct CPU32:public CPU {
	void SetRegOrder(const int* _RegOrder) {
		RegOrder = _RegOrder;
	}
	const int* GetRegOrder() {
		return RegOrder;
	}
	const int  GetNumberofRegister() {
		return 21;
	}
	uint64_t GetFlatMemoryIP() {
		return  RIP ;
	}
};
struct CPU16:public CPU {
	void SetRegOrder(const int* _RegOrder) {
		RegOrder = _RegOrder;
	}
	const int* GetRegOrder() {
		return RegOrder;
	};
	const int  GetNumberofRegister() {
		return 21;
	}
	uint64_t GetFlatMemoryIP() {
		return  RIP + CS * 16;
	}
};
struct AllCPU {
	uc_engine* uc16;
	uc_engine* uc32;
	CPU* cpu16;
	CPU* cpu32;
	AllCPU() {
		cpu16 = new CPU16;
		cpu32 = new CPU32;
	}
	~AllCPU() {
		delete cpu16;
		delete cpu32;
	}
};

//////////////////////////////////////////////////////////////////////////////
extern char* RAM;



//////////////////////////////////////////////////////////////////////////////
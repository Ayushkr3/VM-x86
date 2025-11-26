#include "table.h"
#include "xed_table.h"
#include <Windows.h>
using namespace std;
bool deltaMode = false; //32 bit requested
//void LookUp(CPU* cpu,xed_decoded_inst_t* Pins, INT64* ripOffset) {
//	//----------------------------addressing varaibles----------------------------------//
//	vector<int64_t> rel32;
//	vector<int64_t> imm32;
//	vector<xed_reg_enum_t> src_reg;
//	vector<xed_reg_enum_t> dst_reg;
//	int32_t width = 0;
//	int64_t src_mem = -1;
//	int64_t dst_mem = -1;
//	int64_t far_ptr = -1;
//	INT64* operand1_val = nullptr;
//	INT64* operand2_val = nullptr;
//	INT64* operand3_val = nullptr;
//	INT64* DS_SS_select = nullptr;
//	bool higher8_s = false;
//	bool higher8_d = false;
//	InteruptStruct is;
//	//----------------------------------------------------------------------------------//
//	xed_iclass_enum_t iclass = xed_decoded_inst_get_iclass(Pins);
//	int NO = xed_decoded_inst_noperands(Pins);//number of operands
//	for (int i = 0; i <NO; i++) {
//		const xed_operand_t* opr = xed_inst_operand(xed_decoded_inst_inst(Pins),i);
//		xed_operand_enum_t opr_enum = xed_operand_name(opr);
//		xed_operand_type_enum_t type = xed_operand_type(opr);
//		if (opr_enum == XED_OPERAND_RELBR) {
//			xed_int64_t rel32_v = xed_decoded_inst_get_branch_displacement(Pins);
//			rel32.push_back(rel32_v);
//		}
//		else if(xed_operand_is_register(opr_enum)){
//			if (xed_operand_written(opr)) {
//				xed_reg_enum_t dest_reg = xed_decoded_inst_get_reg(Pins, opr_enum);
//				if (dest_reg >= XED_REG_ES && dest_reg <= XED_REG_GS) {
//					dst_reg.push_back((xed_reg_enum_t)(dest_reg + XED_E_OFF - 221));
//					continue;
//				}
//				else if (dest_reg >= XED_REG_AH && dest_reg <= XED_REG_DH) {
//					higher8_d = true;
//				}
//				dst_reg.push_back((xed_reg_enum_t)(xed_get_largest_enclosing_register32(dest_reg)));
//			}
//			else if (xed_operand_read(opr)) {
//				xed_reg_enum_t s_reg = xed_decoded_inst_get_reg(Pins, opr_enum);
//				if (s_reg >= XED_REG_ES && s_reg <= XED_REG_GS) {
//					src_reg.push_back((xed_reg_enum_t)(s_reg + XED_E_OFF - 221));
//					continue;
//				}
//				else if (s_reg >= XED_REG_AH && s_reg <= XED_REG_DH) {
//					higher8_s = true;
//				}
//				src_reg.push_back((xed_reg_enum_t)(xed_get_largest_enclosing_register32(s_reg)));
//			}
//		}
//		else if (opr_enum==XED_OPERAND_MEM0|| opr_enum == XED_OPERAND_MEM1) {
//			MemoryAccess mem_acc;
//			unsigned int mem_op_index = (opr_enum == XED_OPERAND_MEM1);
//			mem_acc.base = (xed_reg_enum_t)(xed_get_largest_enclosing_register32(xed_decoded_inst_get_base_reg(Pins, mem_op_index)));
//			mem_acc.index = xed_decoded_inst_get_index_reg(Pins, mem_op_index);
//			mem_acc.scale = xed_decoded_inst_get_scale(Pins, mem_op_index);
//			mem_acc.displacement = xed_decoded_inst_get_memory_displacement(Pins, mem_op_index);
//			int64_t final_address = 0;
//			DS_SS_select = cpu->Reg[xed_decoded_inst_get_seg_reg(Pins, mem_op_index) - 221];
//			if (mem_acc.base != XED_REG_INVALID) {
//				final_address += *cpu->Reg[mem_acc.base-XED_E_OFF];
//				auto test = (xed_decoded_inst_get_seg_reg(Pins, mem_op_index))-221;
//				//DS_SS_select = (mem_acc.base == XED_REG_EBP) ? &cpu->SS : &cpu->DS;
//			}
//			if (mem_acc.index != XED_REG_INVALID) {
//				final_address += ((*cpu->Reg[mem_acc.index -XED_E_OFF])* mem_acc.scale);
//			}
//			final_address += mem_acc.displacement;
//			if (xed_operand_written(opr)) {
//				dst_mem = final_address;
//				if(iclass !=XED_ICLASS_PUSH && iclass != XED_ICLASS_POP&& iclass != XED_ICLASS_CALL_NEAR&&iclass != XED_ICLASS_RET_NEAR)
//					ZeroMemory((RAM + (cpu->DS * 16) + dst_mem),xed_decoded_inst_get_operand_width(Pins)/8);
//			}
//			else if (xed_operand_read(opr)) {
//				src_mem = final_address;
//			}
//		}
//		else if (opr_enum== XED_OPERAND_IMM0|| opr_enum == XED_OPERAND_IMM1) {
//			
//			int64_t immd = xed_decoded_inst_get_signed_immediate(Pins);
//			imm32.push_back(immd);
//		}
//		else if (opr_enum == XED_OPERAND_PTR) {
//			far_ptr = xed_decoded_inst_get_branch_displacement(Pins);
//		}
//		
//	}
//	switch (iclass)
//	{
//	case XED_ICLASS_MOV: {
//		INT64 opr0 = 0;
//		INT64 opr1 = 0;
//		operand1_val = &opr0; //dst
//		operand2_val = &opr1; //src
//		int mem0;
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!src_reg.empty()) {
//			operand2_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[src_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[src_reg[0] - XED_E_OFF]);
//		}
//		else if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		else if (src_mem != -1) {
//			memcpy((char*)operand2_val, (char*)(RAM + ((*DS_SS_select) * 16 + src_mem)), width);
//		}
//		if (!dst_reg.empty()) {
//			operand1_val = (higher8_d) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[dst_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[dst_reg[0] - XED_E_OFF]);
//
//		}
//		else if (dst_mem != -1) {
//			mem0 = (*DS_SS_select) * 16 + dst_mem;
//			operand1_val = (INT64*)(RAM + (*DS_SS_select) * 16 + dst_mem);
//			//memcpy(operand1_val, (RAM + ((*DS_SS_select) * 16) + dst_mem), width);
//		}
//		if (operand1_val != nullptr && operand2_val != nullptr) {
//			memcpy((char*)operand1_val, (char*)operand2_val, width);
//		}
//		else {
//			throw "Error";
//		}
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_JNLE: {
//		int64_t off = 0;
//		if (cpu->ZF == 0 && cpu->SF == cpu->OF) {
//			off = rel32[0];
//		}
//		*ripOffset = off;
//		break;
//	}
//	case XED_ICLASS_JZ: {
//		int64_t off = 0;
//		if (cpu->ZF) {
//			off = rel32[0];
//		}
//		*ripOffset = off;
//		break;
//	}
//	case XED_ICLASS_JNZ: {
//		int64_t off = 0;
//		if (!cpu->ZF) {
//			off = rel32[0];
//		}
//		*ripOffset = off;
//		break;
//	}
//	case XED_ICLASS_JB: {
//		if (cpu->CF) {
//			*ripOffset = rel32[0];
//		}
//		break;
//	}
//	case XED_ICLASS_JMP: {
//		if (!rel32.empty()) {
//			*ripOffset = rel32[0];
//		}
//		else {
//			*ripOffset = *cpu->Reg[src_reg[0] - XED_E_OFF];
//		}
//		break;
//	}
//	case XED_ICLASS_JMP_FAR: {
//		if (!imm32.empty()) {
//			cpu->CS = imm32[0];
//		}
//		if (far_ptr != -1) {
//			cpu->RIP = far_ptr;
//		}
//		*ripOffset = -(INT64)xed_decoded_inst_get_length(Pins);
//		break;
//	}
//	case XED_ICLASS_CLD: {
//		cpu->DF = false;
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_PUSH: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		cpu->RSP = cpu->RSP - width;
//		*ripOffset = 0;
//		INT64* value = ripOffset;
//		if (!src_reg.empty()) {
//			value = (cpu->Reg[src_reg[0] - XED_E_OFF]);
//		}
//		else if (!imm32.empty()) {
//			value = &imm32[0];
//		}
//		else if (src_mem != -1) {
//			memcpy((char*)value, (RAM + ((*DS_SS_select) * 16) + src_mem), width);
//		}
//#ifdef DEBUG
//		stack_d.push_back(*value);
//#endif
//		if (value != nullptr)
//			memcpy(STACK, value, width);
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_CALL_NEAR: {
//		int64_t off = 0;
//		width = 0;
//		if (!rel32.empty()) {
//			off = rel32[0];
//			width = xed_decoded_inst_get_branch_displacement_width(Pins);
//		}
//		else if (!src_reg.empty()) {
//			off = *cpu->Reg[src_reg[0] - XED_E_OFF];
//			width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		}
//		cpu->RSP = cpu->RSP - width;
//		int64_t value = ((INT32)cpu->RIP + xed_decoded_inst_get_length(Pins));
//#ifdef DEBUG
//		stack_d.push_back(value);
//#endif 
//		memcpy(STACK, &value, width);
//		*ripOffset = off;
//		break;
//	}
//	case XED_ICLASS_RET_NEAR: {
//		int64_t value = 0;
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		memcpy(&value, STACK, width);
//		cpu->RSP = cpu->RSP + width;
//		cpu->RIP = value;
//		*ripOffset = -(INT64)xed_decoded_inst_get_length(Pins);
//		break;
//	}
//	case XED_ICLASS_CMP: {
//		INT64 opr0 = 0;
//		INT64 opr1 = 0;
//		operand1_val = &opr0;
//		operand2_val = &opr1;
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!src_reg.empty()) {
//			operand1_val = cpu->Reg[src_reg[0] - XED_E_OFF];
//			src_reg.erase(src_reg.begin());
//		}
//		else if (src_mem != -1) {
//			//operand1_val = (INT64*)(RAM + ((*DS_SS_select) * 16) + src_mem);
//			memcpy((char*)operand1_val, (RAM + ((*DS_SS_select) * 16) + src_mem), width);
//		}
//		if (!src_reg.empty()) {
//			operand2_val = cpu->Reg[src_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand2_val = (INT64*)(RAM + ((*DS_SS_select) * 16) + dst_mem);
//		}
//		else if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		*operand1_val = MASK(*operand1_val, width * 8);
//		*operand2_val = MASK(*operand2_val, width * 8);
//		INT64 result = *operand1_val - *operand2_val;
//		cpu->ZF = (result == 0);
//		cpu->SF = (result < 0);
//		cpu->CF = ((INT64)*operand1_val < (INT64)*operand2_val);
//		cpu->OF = ((*operand1_val > 0 && *operand2_val < 0 && result < 0) ||
//			(*operand1_val < 0 && *operand2_val > 0 && result > 0)) ? 1 : 0;
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_SHRD: {
//		if (!dst_reg.empty()) {
//			operand1_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[dst_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[dst_reg[0] - XED_E_OFF]);
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + (*DS_SS_select) * 16 + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand3_val = &imm32[0];
//		}
//		else if (!src_reg.empty()) {
//			operand2_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[src_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[src_reg[0] - XED_E_OFF]);
//			src_reg.erase(src_reg.begin());
//			if (!src_reg.empty())
//				operand3_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[src_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[src_reg[0] - XED_E_OFF]);
//		}
//		uint64_t count = (*operand3_val) & ((width * 8) - 1);
//		if (count == 0)
//			return;
//		uint64_t mask = (1ULL << (width * 8)) - 1;
//		uint64_t dest_val = *operand1_val;
//		uint64_t src_val = *operand2_val;
//
//		uint64_t result = (dest_val >> count) | (src_val << ((width * 8) - count))&mask;
//		cpu->CF = (dest_val >> (count - 1)) & 1;
//		if (count == 1)
//			cpu->OF = ((result >> (width * 8 - 1)) ^ (dest_val >> (width * 8 - 1))) & 1;
//		cpu->SF = (result >> (width * 8 - 1)) & 1;
//		cpu->ZF = (result == 0);
//		cpu->PF = (__popcnt((result) & 0xFF) % 2 == 0);
//		*operand1_val = result;
//		memcpy(operand1_val, &result, width);
//		break;
//	}
//	case XED_ICLASS_SHR: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		int64_t orignal0 = -1;
//		int64_t orignal1 = -1;
//		if (!dst_reg.empty()) {
//			operand1_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[dst_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[dst_reg[0] - XED_E_OFF]);
//			orignal0 = *cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + (*DS_SS_select) * 16 + dst_mem);
//			orignal0 = (INT64)(RAM + (*DS_SS_select) * 16 + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//			orignal1 = *operand2_val;
//		}
//		else {
//			operand2_val = (higher8_s) ? (INT64*)((reinterpret_cast<uint8_t*>(cpu->Reg[src_reg[0] - XED_E_OFF]) + 1)) : (cpu->Reg[src_reg[0] - XED_E_OFF]);
//			orignal1 = *operand2_val;
//		}
//		*operand1_val = MASK(*operand1_val, width * 8);
//		*operand2_val = MASK(*operand2_val, width * 8);
//		int64_t res = *operand1_val >> *operand2_val;
//		memcpy(operand1_val, &res, width);
//		cpu->CF = ((orignal0 >> (*operand2_val - 1)) & 1) ? 1 : 0;
//		cpu->ZF = (*operand1_val == 0) ? 1 : 0;
//		cpu->SF = false;
//		if (*operand2_val == 1) {
//			int shift = *operand2_val & 0x3F; // mask to 6 bits (x86 behavior)
//			int opw = xed_decoded_inst_get_operand_width(Pins);
//			int msb = opw - 1;
//
//			uint64_t mask = (opw == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << opw) - 1);
//			uint64_t val = *operand1_val & mask;
//			cpu->OF = ((*operand1_val & ((opw == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << opw) - 1)) >> msb) & 1);
//		}
//		else
//			cpu->OF = false;
//		cpu->PF = (__popcnt((*operand1_val) & 0xFF) % 2 == 0);
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_CLI:
//		cpu->IF = false;
//		*ripOffset = 0;
//		break;
//	case XED_ICLASS_STI:
//		cpu->IF = true;
//		*ripOffset = 0;
//		break;
//	case XED_ICLASS_POP: {
//		char test[4];
//		memcpy(test, STACK, 4);
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!dst_reg.empty()) {
//			operand1_val = cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + dst_mem);
//		}
//		memcpy(operand1_val, STACK, width);
//#ifdef DEBUG
//		stack_d.erase(stack_d.end() - 1);
//#endif 
//		cpu->RSP = cpu->RSP + width;
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_SUB: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!dst_reg.empty()) {
//			operand1_val = cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		else if (!src_reg.empty()) {
//			operand2_val = cpu->Reg[XED_REG_ECX - XED_E_OFF];
//		}
//		else if (src_mem != -1) {
//			operand2_val = (INT64*)(RAM + src_mem);
//		}
//		INT32 res = *operand1_val - *operand2_val;
//		cpu->ZF = res == 0;
//		cpu->CF = (*operand1_val < *operand2_val);
//		cpu->OF = (((*operand1_val ^ *operand2_val) & (*operand1_val ^ res)) >> (width * 8 - 1)) & 1;
//		cpu->SF = ((res >> (width * 8 - 1)) & 1);
//		uint32_t v = res & 0xFF;
//		v ^= v >> 4;
//		v ^= v >> 2;
//		v ^= v >> 1;
//		cpu->PF = ((~v) & 1);
//		cpu->AF = ((*operand1_val ^ *operand2_val ^ res) & 0x10) != 0;
//		*operand1_val = res;
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_ADC: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!dst_reg.empty()) {
//			operand1_val = cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		else if (!src_reg.empty()) {
//			operand2_val = cpu->Reg[XED_REG_ECX - XED_E_OFF];
//		}
//		else if (src_mem != -1) {
//			operand2_val = (INT64*)(RAM + src_mem);
//		}
//		uint64_t mask = (width == 8) ? 0xFFFFFFFFFFFFFFFFULL :
//			(1ULL << (width * 8)) - 1;
//		INT64 res = ((*operand1_val & mask) + (*operand2_val & mask)+cpu->CF) & mask;
//		uint64_t full = *operand1_val + *operand2_val + (cpu->CF & 1);
//		cpu->CF = (full >> (width * 8)) & 1;
//		cpu->ZF = (res == 0);
//		cpu->SF = ((res >> ((width * 8) - 1)) & 1);
//		cpu->OF = (((~(*operand1_val ^ *operand2_val)) & (*operand1_val ^ res)) >> ((width * 8) - 1)) & 1;
//		cpu->AF = ((*operand1_val ^ *operand2_val ^ res) >> 4) & 1;
//		uint32_t v = res & 0xFF;
//		v ^= v >> 4;
//		v ^= v >> 2;
//		v ^= v >> 1;
//		cpu->PF = ((~v) & 1);
//		*operand1_val = res;
//		break;
//	}
//	case XED_ICLASS_ADD: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!dst_reg.empty()) {
//			operand1_val = cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		else if (!src_reg.empty()) {
//			operand2_val = cpu->Reg[XED_REG_ECX - XED_E_OFF];
//		}
//		else if (src_mem != -1) {
//			operand2_val = (INT64*)(RAM + src_mem);
//		}
//		uint64_t mask = (width == 8) ? 0xFFFFFFFFFFFFFFFFULL :
//			(1ULL << (width * 8)) - 1;
//		INT64 res = ((*operand1_val&mask) + (*operand2_val&mask)) & mask;
//		uint64_t full = *operand1_val + *operand2_val;
//		cpu->CF = (full >> (width * 8)) & 1;
//		cpu->ZF = (res == 0);
//		cpu->SF = ((res >> ((width * 8) - 1)) & 1);
//		cpu->OF = (((*operand1_val ^ res) & (*operand2_val ^ res)) >> ((width * 8) - 1)) & 1;
//		cpu->AF = ((*operand1_val ^ *operand2_val ^ res) >> 4) & 1;
//		uint32_t v = res & 0xFF;
//		v ^= v >> 4;
//		v ^= v >> 2;
//		v ^= v >> 1;
//		cpu->PF = ((~v) & 1);
//		*operand1_val = res;
//		*ripOffset = 0;
//		break;
//	}
//	case XED_ICLASS_LODSB: {
//		uint64_t addr = (cpu->DS * 16) + cpu->RSI;
//		memcpy(&cpu->RAX, RAM + addr, 1);
//		if (cpu->DF == 0)
//			cpu->RSI += 1;
//		else
//			cpu->RSI -= 1;
//		break;
//	}
//	case XED_ICLASS_INT: {
//		operand1_val = &imm32[0];
//		is.num = *operand1_val;
//		is.cpu = *cpu;
//		is.pcpu = cpu;
//		//IOLookUp(is,ud);
//		break;
//	}
//	case XED_ICLASS_XOR: {
//		width = xed_decoded_inst_get_operand_width(Pins) / 8;
//		if (!dst_reg.empty()) {
//			operand1_val = cpu->Reg[dst_reg[0] - XED_E_OFF];
//		}
//		else if (dst_mem != -1) {
//			operand1_val = (INT64*)(RAM + dst_mem);
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		else if (!src_reg.empty()) {
//			operand2_val = cpu->Reg[XED_REG_ECX - XED_E_OFF];
//		}
//		else if (src_mem != -1) {
//			operand2_val = (INT64*)(RAM + src_mem);
//		}
//		uint64_t mask = (width == 8) ? 0xFFFFFFFFFFFFFFFFULL :
//			(1ULL << (width * 8)) - 1;
//		INT64 res = ((*operand1_val & mask) ^ (*operand2_val & mask) + cpu->CF) & mask;
//		cpu->CF = false;
//		cpu->OF = false;
//		cpu->SF = ((res >> ((width * 8) - 1)) & 1);
//		//cpu->PF
//		cpu->ZF = res == 0;
//		//*operand1_val = res;
//		break;
//	}
//	case XED_ICLASS_TEST: {
//		width = xed_decoded_inst_get_operand_width(Pins)/8;
//		INT64 opr0 = 0;
//		operand1_val = &opr0;
//		if (src_mem != -1) {
//			memcpy(
//				(char*)operand1_val,
//				(char*)(RAM + (*DS_SS_select * 16 + src_mem)),
//				width
//			);
//		}
//		if (!src_reg.empty()) {
//			operand1_val = higher8_s? reinterpret_cast<INT64*>((uint8_t*)cpu->Reg[src_reg[0] - XED_E_OFF] + 1): cpu->Reg[src_reg[0] - XED_E_OFF];
//			if (src_reg.size() == 2)
//				operand2_val = higher8_s? reinterpret_cast<INT64*>((uint8_t*)cpu->Reg[src_reg[1] - XED_E_OFF] + 1): cpu->Reg[src_reg[1] - XED_E_OFF];
//		}
//		if (!imm32.empty()) {
//			operand2_val = &imm32[0];
//		}
//		int64_t res = *operand1_val & *operand2_val;
//		cpu->ZF = res == 0;
//		cpu->SF = (res >> (width * 8 - 1)) & 1;;
//		cpu->CF = 0;
//		cpu->OF = 0;
//		cpu->PF = (__popcnt((*operand1_val) & 0xFF) % 2 == 0);
//		break;
//	}
//	case XED_ICLASS_NOP: {
//		break;
//	}
//	default:
//		throw iclass;
//		*ripOffset = 0;
//		break;
//	}
//}
//void ReadInstruction(CPU* cpu, INT64* offset,xed_decoded_inst_t* Pins) {
//	xed_uint8_t* currentByte = (xed_uint8_t*)RAM +(cpu->CS*16+(cpu->RIP));
//	//xed_uint8_t currentByte[] = { 0x81, 0x3D, 0x04, 0xC0, 0x34, 0x0, 0x4E, 0xE6, 0x40, 0xBB };
//	xed_error_enum_t err =  xed_decode(Pins,(xed_uint8_t*)currentByte,15);
//	if (err == XED_ERROR_NONE) {
//		try {
//			LookUp(cpu, Pins, offset);
//		}
//		catch (int e) {
//			std::cout << e;
//		}
//	}
//}

void ProtectedModeSwitchHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	uint8_t* bytes = (uint8_t*)(RAM + address);
	if (*bytes == 0xCB || *bytes == 0xCA|| *bytes == 0xEA|| *bytes == 0x9a|| *bytes == 0xCF) {
		if (!ud->cpu->inProtectedMode)
			goto SixteenTo32;
		else
			goto ThirtyTwoTo16;
		
	}
	else {
		return;
	}
SixteenTo32: {
	ud->cpu->inProtectedMode = true;
	uc_hook_del(ud->uc, ud->ModeProtection);
	uc_ctl_flush_tb(uc);
	uc_err e = uc_emu_stop(ud->uc);
	ReadRegUni(ud);
	uc_emu_start(ud->uc, ud->cpu->CS * 0x10 + ud->cpu->RIP, 0, 0, 1);
	ReadRegUni(ud);
	auto index = (ud->cpu->CS >> 3) * 8;
	uint64_t* desc_ptr = (uint64_t*)(RAM + ud->cpu->GDT.base + index);
	uint64_t desc = *desc_ptr;
	uint32_t base = ((desc >> 16) & 0xFFFFFF) | ((desc >> 56) & 0xFF) << 24;
	uc_context* ctx;
	WriteRegUni(ud, UC_X86_REG_EIP, /*base +*/ ud->cpu->RIP);
	e = uc_context_alloc(ud->uc, &ctx);
	e = uc_context_save(ud->uc, ctx);
	ud->uc = ud->cpus->uc32;
	ud->cpu = ud->cpus->cpu32;
	e = uc_context_restore(ud->uc, ctx);
	ReadRegUni(ud);
	uc_context_free(ctx);
	uc_ctl_flush_tb(uc);
	uc_ctl_flush_tlb(uc);
	return;
	}
ThirtyTwoTo16: {
	ud->cpu->inProtectedMode = false;
	uc_hook_del(ud->uc, ud->ModeProtection);
	uc_ctl_flush_tb(uc);
	uc_err e = uc_emu_stop(ud->uc);
	ReadRegUni(ud);
	uc_emu_start(ud->uc,ud->cpu->RIP, 0, 0, 1);
	ReadRegUni(ud);
	uc_context* ctx;
	WriteRegUni(ud, UC_X86_REG_EIP, ud->cpu->RIP);
	e = uc_context_alloc(ud->uc, &ctx);
	e = uc_context_save(ud->uc, ctx);
	const int numofR = ud->cpu->GetNumberofRegister();
	CPU::mmrs gdtr;
	CPU::mmrs ldtr;
	CPU::mmrs idtr;
	gdtr = ud->cpu->GDT;
	ldtr = ud->cpu->LDT;
	idtr = ud->cpu->IDT;
	ud->uc = ud->cpus->uc16;
	ud->cpu = ud->cpus->cpu16;
	e = uc_context_restore(ud->uc, ctx);
	uc_context_free(ctx);
	uc_reg_write(ud->uc, UC_X86_REG_GDTR, &gdtr);
	uc_reg_write(ud->uc, UC_X86_REG_LDTR, &ldtr);
	uc_reg_write(ud->uc, UC_X86_REG_IDTR, &idtr);
	ReadRegUni(ud);
	WriteRegUni(ud, UC_X86_REG_CR0, 0x10);
	uc_ctl_flush_tb(uc);
	uc_ctl_flush_tlb(uc);
	return;
}
}

void PerBlockHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	ReadRegUni(ud);
	if (IRQ.stop_req.load()&&((ud->cpu->Eflag >> 9) & 1)) {
		IRQ.stop_req.store(false);
		uc_emu_stop(uc);
	}
	bool currPE = (ud->cpu->CR0 >> 0) & 1;
	if (currPE != deltaMode) {
		deltaMode = currPE;
		uc_hook_add(ud->uc, &ud->ModeProtection, UC_HOOK_CODE, ProtectedModeSwitchHook, ud, 0, RAM_SIZE);
	}
}
void PerLineHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	ReadRegUni(ud);
	//std::cout << std::hex << ud->cpu->RIP << std::endl;
	if (address == 0x00020742) {
		MemDump(RAM + 0x00020c53, 256);
		
	}
}
void LookUpUnicorn(uc_engine* uc, uint32_t address, void* user_data)
{
	UnicornData* ud = (UnicornData*)user_data;
	ReadRegUni(ud);
	InteruptStruct is;
	is.pcpu = ud->cpu;
	is.num = address;
	IOLookUp(is, ud);
}
void InvalidInsHook(uc_engine* uc, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	ReadRegUni(ud);
	char* at = (RAM + (ud->cpu->CS * 16) + ud->cpu->RIP);
	MemDump(at, 15);
}
void LookUpSpecialIns(uc_engine* uc, uint32_t address, void* user_data) {
	std::cout << "Special INS";
}








void MemR(uc_engine* uc, uc_mem_type type,
	uint64_t address, int size, int64_t value, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	std::cout << std::hex <<address<<std::endl;
	//ReadRegUni(ud);
}
void MemW(uc_engine* uc, uc_mem_type type,
	uint64_t address, int size, int64_t value, void* user_data) {
	UnicornData* ud = (UnicornData*)user_data;
	std::cout << std::hex << address << std::endl;
	//ReadRegUni(ud);
}
void MemDump(char* at, int size) {
	char* buff = (char*)malloc(size);
	memcpy(buff, at, size);
	std::string s(buff, size);
	std::cout << std::hex << buff;
	free(buff);
}
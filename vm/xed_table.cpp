#include "xed_table.h"

//void EmulationLoop(xed_state_t* xed_state) {
//    xed_decoded_inst_t ins;
//    cpu.RIP = 0x7c00;
//    INT64 ripOffsetAfterIns = 0;
//    int counter = 0;
//    xed_decoded_inst_zero_set_mode(&ins, xed_state);
//    char buff[2048];
//    err= uc_mem_read(uc, cpu.RIP,buff , 2048);
//    err = uc_hook_add(uc, &hook, UC_HOOK_INTR, LookUpUnicorn, nullptr, 0, 0);
//    err = uc_reg_write(uc, UC_X86_REG_EIP, &cpu.RIP);
//    err = uc_emu_start(uc, cpu.RIP, 0,0, 0);
//
//   //// while (counter < 300) {
//   // #ifdef DEBUG
//   //         std::cout << std::hex << cpu.RIP << std::endl;
//   // #endif
//   //     //ReadInstruction(&cpu, &ripOffsetAfterIns,&ins);
//   //    /* cpu.RIP += xed_decoded_inst_get_length(&ins) + ripOffsetAfterIns;
//   //     ripOffsetAfterIns = 0;
//   //     xed_decoded_inst_zero_keep_mode(&ins);
//   //     counter++;
//
//   // }*/
//}
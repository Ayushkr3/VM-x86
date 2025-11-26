#include "io.h"

using namespace std;
IO ports;
uc_engine** ucd;
uc_context* ctx;
////////////////////////////////////////////////////////////////////////////////////
static inline uint8_t to_bcd(int value) {
    return (uint8_t)(((value / 10) << 4) | (value % 10));
}

const int INTOrder[3] = {
    UC_X86_REG_EFLAGS,
    UC_X86_REG_CS,
    UC_X86_REG_EIP,
};
////////////////////////////////////////////////////////////////////////////////////
void HardwareIntHook(uc_engine* uc, uint64_t address, uint32_t size, void* user_data) {
    UnicornData* ud = (UnicornData*)user_data;
    uint8_t* bytes = (uint8_t*)(RAM + address);
#ifdef DEBUG
    ReadRegUni(ud);
#endif
    if (*bytes == 0xCF) {
        uc_hook_del(ud->uc, ud->HIntHook);
        uc_emu_stop(ud->uc);
    }

}
void HardwareInt(int nums, UnicornData* ud) {
    switch (nums)
    {
        //Clock INT 0x8
    case IRQ0: {
        char* segIP=RAM+0x70;
        if (!(uint32_t)*segIP ==0x0) {
            /*
            * cli
            * push ip
            * push cs
            * push eflag
            * call (INT 1ah)
            * pop eflag
            * pop cs
            * pop ip
            * sti
            */
            ReadRegUni(ud);
            int16_t IPCSEF[3] = { ud->cpu->RIP,ud->cpu->CS,ud->cpu->Eflag};
            //Add 12 bytes to stack 
            //INT 0x8 6 byte
            //Nested INT 0x1a 6 byte
            WriteRegUni(ud, UC_X86_REG_ESP, ud->cpu->RSP-12);
            uint32_t stackpos = ((ud->cpu->SS * 16) + (uint16_t)(ud->cpu->RSP));
            //Write twice or some garbage
            uc_mem_write(ud->uc, stackpos-12, IPCSEF, 3*sizeof(int16_t));
            uc_mem_write(ud->uc, stackpos-6, IPCSEF, 3 * sizeof(int16_t));
            ReadRegUni(ud);
            uint16_t* newIP = (uint16_t*)(RAM + 0x70);
            uint16_t* newCS = (uint16_t*)(RAM + 0x70+0x2); 
            uc_hook_add(ud->uc,&ud->HIntHook , UC_HOOK_CODE, HardwareIntHook, ud, 0, RAM_SIZE);
            //ISR routine
            uc_emu_start(ud->uc, *newCS * 16 + *newIP, 0, 0, 0);
            //Perform the final IRET Nested instruction
            //uc_emu_start(ud->uc, ud->cpu->CS * 16 + ud->cpu->RIP, 0, 0, 1);
            //uc_err e = uc_emu_start(ud->uc, 0x20c87, 0, 0, 1);
            uc_mem_read(ud->uc, stackpos-12, IPCSEF, 3 * sizeof(int16_t));
            WriteRegUni(ud, UC_X86_REG_ESP, ud->cpu->RSP + 6);
            ReadRegUni(ud);
            //Read the INT 0x8 IP,CS....
            uc_mem_read(ud->uc, stackpos-6, IPCSEF, 3 * sizeof(int16_t));
            WriteRegUni(ud, UC_X86_REG_ESP, ud->cpu->RSP +6);
            WriteRegUni(ud, UC_X86_REG_IP, IPCSEF[0]);
            WriteRegUni(ud, UC_X86_REG_CS, IPCSEF[1]);
            WriteRegUni(ud, UC_X86_REG_EFLAGS, IPCSEF[2]);
            ReadRegUni(ud);
        }
        break;
    }
    default:
        break;
    }
}
void InitIO(uc_engine** uc) {
    ucd = uc;
}
char* ReadFile(std::string path) {
    std::ifstream stream;
    stream.open(path, std::ios::binary);
    stream.seekg(0, std::ios::end);
    long long sizeOfFile = stream.tellg();
    stream.seekg(0);

    char* FileInMemory = (char*)malloc(sizeOfFile);

    stream.read(FileInMemory, sizeOfFile);
    stream.close();
    return FileInMemory;
}
void LoadDisk(string path, DISK_TYPE type) {
    std::ifstream* iso = new std::ifstream;
    iso->open(path, std::ios::binary);
    ports.IO_P_B[type]=iso;
}
void ReadIODisk(int index,long long DAP, UnicornData* ud) {
    std::ifstream* disk = ports.IO_P_B[index];
    char daps[16];
    memcpy(daps, RAM + DAP, 16);
    uint8_t  size = daps[0];
    uint8_t  reserved = daps[1];
    uint16_t sectors = *(uint16_t*)&daps[2];
    uint32_t buffer = *(uint32_t*)&daps[4];
    uint64_t lba = *(uint64_t*)&daps[8];
    int sectorSize = 0;
    uint16_t off = *(uint16_t*)&daps[4];
    uint16_t segment = *(uint16_t*)&daps[6];
    if (index == DISK_TYPE::DVD) {
        sectorSize = 2048;
    }
    char* data = (char*)malloc(sectors * sectorSize);
    disk->seekg(lba * sectorSize, std::ios::beg);
    disk->read(data, sectorSize * sectors);
    uint64_t addr = (segment * 16) + (off);
    //memcpy((RAM + (segment * 16) + (off)), data, sectorSize * sectors);
#ifdef UNICORN
    uc_err err = uc_mem_write(ud->uc,(segment * 16) + (off), data, sectorSize * sectors);
#endif 
   uc_ctl_remove_cache(ud->uc, (segment * 16) + (off), ((segment * 16) + (off))+sectorSize * sectors);
    free(data);
}

void BootDisk() {
    std::ifstream* iso = ports.IO_P_B[0xe0];
    if (iso == nullptr) {
        return;
    }
    char sector[2048];
    iso->seekg(2048 * 0x11, std::ios::beg);
    iso->read(sector, 2048);
    uint32_t bootCatalogLBA = *reinterpret_cast<uint32_t*>(sector + 0x47);
    iso->seekg(bootCatalogLBA * 2048, std::ios::beg);
    iso->read(sector, 2048);
    uint32_t lba = *(uint32_t*)&sector[0x28];
    iso->seekg(lba * 2048, std::ios::beg);
    iso->read(sector, 2048);
    memcpy(RAM + 0x7c00, sector, 2048);
}
void RemoveDisk(int index) {
    if (ports.IO_P_B[index] != nullptr) {
        ports.IO_P_B[index]->close();
        delete ports.IO_P_B[index];
        ports.IO_P_B[index] = nullptr;
    }
}
void ReadHDD(uint16_t cyl, uint8_t head, uint16_t numberofSec,uint8_t sector,int index, UINT64 RAMloc, UnicornData* ud) {
    if (ports.IO_P_B[index] == nullptr)return;
    uint64_t location = ((cyl * 16 + head) * 63) + (sector - 1);
    auto ptr = ports.IO_P_B[index];
    const int sectorSize = 512;
    char* data = (char*)malloc(numberofSec * sectorSize);
    ptr->seekg(location * sectorSize, std::ios::beg);
    ptr->read(data, numberofSec * sectorSize);
    //memcpy(RAM + RAMloc, data, numberofSec * sectorSize);
#ifdef UNICORN
    uc_err err = uc_mem_write(ud->uc, RAMloc, data, numberofSec * sectorSize);
#endif 
    uc_ctl_remove_cache(ud->uc, RAMloc, RAMloc + numberofSec * sectorSize);
    free(data);
}
void IOLookUp(InteruptStruct in, UnicornData* ud) {
    uint8_t* ah = reinterpret_cast<uint8_t*>(&in.pcpu->RAX) + 1; //type of access
    uint8_t* al = reinterpret_cast<uint8_t*>(&in.pcpu->RAX); //Number of sectors to read (1–127)
    uint8_t* ch = reinterpret_cast<uint8_t*>(&in.pcpu->RCX) + 1; //Cylinder number(low 8 bits)
    uint8_t* cl = reinterpret_cast<uint8_t*>(&in.pcpu->RCX); //Sector number
    uint8_t* dl = reinterpret_cast<uint8_t*>(&in.pcpu->RDX); // drive address
    uint8_t* dh = reinterpret_cast<uint8_t*>(&in.pcpu->RDX) + 1; //Head number
    switch (in.num) {
    case 0x13: {
        UINT64 loc = in.pcpu->ES * 16 + in.pcpu->RBX;
        switch (*ah)
        {
        case 0x42: {
            long long dap = (long long)((in.pcpu->DS * 16) + in.pcpu->RSI);
            ReadIODisk(*dl, dap,ud);
            in.pcpu->CF = false;
            ah = 0;
            in.pcpu->Eflag &= ~(1 << 0);
            WriteRegUni(ud, UC_X86_REG_AH, 0);
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag &= ~(1 << 0));
            break;
        }
        case 0x2:
        {
            ReadHDD(*ch, *dh,*al, *cl,*dl, loc,ud);
            *ah = 0x101;
            WriteRegUni(ud, UC_X86_REG_AH, *ah);
            in.pcpu->Eflag |= 1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        case 0x8: {
            uint16_t cylinders = 1040;
            uint8_t heads = 16;
            uint8_t sectors = 63;

            uint8_t chv = cylinders & 0xFF;
            uint8_t clv = ((cylinders >> 2) & 0xC0) | (sectors & 0x3F);
            uint8_t dhv = heads - 1;

            WriteRegUni(ud, UC_X86_REG_CH, chv);
            WriteRegUni(ud, UC_X86_REG_CL, clv);
            WriteRegUni(ud, UC_X86_REG_DH, dhv);
            WriteRegUni(ud, UC_X86_REG_DL, 1);
            WriteRegUni(ud, UC_X86_REG_AH, 0x00); 
            in.pcpu->Eflag &= ~0x1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        case 0x4b: {
            WriteRegUni(ud, UC_X86_REG_AH, 0);
            WriteRegUni(ud, UC_X86_REG_AL, 0);
            in.pcpu->Eflag &= ~1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        case 0x41: {
            WriteRegUni(ud, UC_X86_REG_CX, 0x0007);
            WriteRegUni(ud, UC_X86_REG_AH, 0x00);
            in.pcpu->Eflag &= ~0x1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            
            break;
        }
        case 0xd:
            WriteRegUni(ud, UC_X86_REG_AH, 0);
            in.pcpu->Eflag &= ~1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        default:
            std::cout << "unhandled interupt " << in.num <<"read  " <<ah << std::endl;
            break;
        }
        break;
    }
    case 0x15:{
        switch (*ah)
        {
        case 0xE8: {
            if (*al == 0x1) {
                uint16_t below16mb_kb = 0x3C00;  // 15 MB (1–16 MB range)
                uint16_t above16mb_blocks = 0x7F90;  // ~2 GB - 16 MB, in 64 KB blocks
                WriteRegUni(ud, UC_X86_REG_EAX, below16mb_kb);
                WriteRegUni(ud, UC_X86_REG_EBX, above16mb_blocks);
                WriteRegUni(ud, UC_X86_REG_ECX, below16mb_kb);
                WriteRegUni(ud, UC_X86_REG_EDX, above16mb_blocks);
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                break;
            }
            uint16_t cont;
            const uint64_t loc = (ud->cpu->ES * 16) + ud->cpu->RDI;
            #pragma pack(push,1)
            struct SMAPEntry
            {
                uint64_t base;
                uint64_t length;
                uint32_t type;
            };
            #pragma pack(pop)
            SMAPEntry entry;
            switch (ud->cpu->RBX)
            {
            case 0x0: {
                entry.base = 0x00000000;
                entry.length = 0x0009FC00;
                entry.type = 1;
                cont = 1;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
            case 0x1: {
                entry.base = 0x0009FC00;
                entry.length = 0x00000400;
                entry.type = 2;
                cont = 2;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
            case 0x2: {
                entry.base = 0x000F0000;
                entry.length = 0x000010000;
                entry.type = 2;
                cont = 3;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;  
            }
            case 0x3:
            {
                entry.base = 0x00100000;
                entry.length = 0x7BEE0000;
                entry.type = 1;
                cont = 4;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
            case 0x4:
            {
                entry.base = 0x7BEE0000;
                entry.length = 0x00020000;
                entry.type = 3;
                cont = 5;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
            case 0x5:
            {
                entry.base = 0x7BF00000;
                entry.length = 0x00100000;
                entry.type = 4;
                cont = 0;
                in.pcpu->Eflag &= ~0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
            case 0x6:
            {
                entry.base = 0x7C000000;
                entry.length = 0x040000000;
                entry.type = 2;
                cont = 0;
                in.pcpu->Eflag |= 0x1;
                WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
                goto COPY;
            }
        COPY:
            uint16_t s = sizeof(SMAPEntry);
            memcpy(RAM + loc, &entry, sizeof(SMAPEntry));
            uc_ctl_flush_tb(ud->uc);
            WriteRegUni(ud, UC_X86_REG_EAX, 0x534d4150);
            WriteRegUni(ud, UC_X86_REG_EBX, cont);
            WriteRegUni(ud, UC_X86_REG_ECX, sizeof(SMAPEntry));
            }
            break;
        }
        case 0x88: {
            uint32_t ext_kb = (2ULL * 1024ULL * 1024ULL) - 1024;
            ext_kb /= 1024;
            if (ext_kb > 0xFFFF) ext_kb = 0xFFFF;  
            WriteRegUni(ud, UC_X86_REG_EAX, ext_kb);
            in.pcpu->Eflag &= ~0x1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        default:
            std::cout << "unhandled SMAP " << in.num << std::endl;
            break;
        }
        break;
    }
    case 0x12: {
        WriteRegUni(ud, UC_X86_REG_EAX, 0x27F);
        in.pcpu->Eflag &= ~0x1;
        WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
        break;
    }
    case 0x1a: {
        switch (*ah) {
        case 0x00: { // Read system clock counter
            uint64_t ms_since_boot = get_ms_since_midnight();
            uint32_t ticks = (uint32_t)(ms_since_boot / 54.9254);

            WriteRegUni(ud, UC_X86_REG_CX, (ticks >> 16) & 0xFFFF);
            WriteRegUni(ud, UC_X86_REG_DX, ticks & 0xFFFF);
            WriteRegUni(ud, UC_X86_REG_AL, (ticks >= 1573040) ? 1 : 0);
            WriteRegUni(ud, UC_X86_REG_AH, 0);
            in.pcpu->Eflag &= ~0x1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        case 0x2: {
            HostDatenTime tim = get_host_time();
            WriteRegUni(ud, UC_X86_REG_CH, to_bcd(tim.hour));
            WriteRegUni(ud, UC_X86_REG_CL, to_bcd(tim.minute));
            WriteRegUni(ud, UC_X86_REG_DH, to_bcd(tim.second));
            WriteRegUni(ud, UC_X86_REG_AH, 0);
            in.pcpu->Eflag &= ~0x1;
            WriteRegUni(ud, UC_X86_REG_EFLAGS, in.pcpu->Eflag);
            break;
        }
        default:
            std::cout << "unhandled interupt " << in.num << std::endl;
            break;
        }
        break;
    }
    default:
        if (in.num == 0x10)break;
        std::cout << "unhandled interupt " << in.num << std::endl;
        break;
    }
}

#pragma once
#include "Global.h"


// =============================================================================
//  LAPIC register offsets  (Intel SDM Vol 3A Table 10-1)
// =============================================================================
#define LAPIC_REG_ID            0x020   // Local APIC ID
#define LAPIC_REG_VER           0x030   // Local APIC Version
#define LAPIC_REG_TPR           0x080   // Task Priority Register
#define LAPIC_REG_APR           0x090   // Arbitration Priority Register
#define LAPIC_REG_PPR           0x0A0   // Processor Priority Register
#define LAPIC_REG_EOI           0x0B0   // End-Of-Interrupt
#define LAPIC_REG_RRD           0x0C0   // Remote Read Register
#define LAPIC_REG_LDR           0x0D0   // Logical Destination Register
#define LAPIC_REG_DFR           0x0E0   // Destination Format Register
#define LAPIC_REG_SVR           0x0F0   // Spurious Interrupt Vector Register

#define LAPIC_REG_ISR0          0x100   // In-Service Register [0..7] @ +0x00..+0x70
#define LAPIC_REG_TMR0          0x180   // Trigger Mode Register
#define LAPIC_REG_IRR0          0x200   // Interrupt Request Register

#define LAPIC_REG_ESR           0x280   // Error Status Register
#define LAPIC_REG_CMCI          0x2F0   // LVT CMCI
#define LAPIC_REG_ICR_LO        0x300   // Interrupt Command Register [31:0]
#define LAPIC_REG_ICR_HI        0x310   // Interrupt Command Register [63:32]

#define LAPIC_REG_LVT_TIMER     0x320   // LVT Timer
#define LAPIC_REG_LVT_THERM     0x330   // LVT Thermal Sensor
#define LAPIC_REG_LVT_PERF      0x340   // LVT Performance Counter
#define LAPIC_REG_LVT_LINT0     0x350   // LVT Local Interrupt 0
#define LAPIC_REG_LVT_LINT1     0x360   // LVT Local Interrupt 1
#define LAPIC_REG_LVT_ERROR     0x370   // LVT Error

#define LAPIC_REG_TIMER_ICR     0x380   // Timer Initial Count
#define LAPIC_REG_TIMER_CCR     0x390   // Timer Current Count  (read-only to guest)
#define LAPIC_REG_TIMER_DCR     0x3E0   // Timer Divide Configuration

// Aliases used for MMIO writeback (same values, cleaner intent)
#define APIC_OFFSET_TMICT       LAPIC_REG_TIMER_ICR
#define APIC_OFFSET_TMCCT       LAPIC_REG_TIMER_CCR
#define APIC_OFFSET_LVT_TIMER   LAPIC_REG_LVT_TIMER
#define APIC_OFFSET_TDCR        LAPIC_REG_TIMER_DCR


#define LVT_VECTOR_MASK         0x000000FFu
#define LVT_DELIVERY_SHIFT      8
#define LVT_DELIVERY_MASK       (0x7u << LVT_DELIVERY_SHIFT)
#define LVT_STATUS_BIT          (1u << 12)  // delivery status (send pending)
#define LVT_POLARITY_BIT        (1u << 13)  // LINT pin polarity
#define LVT_REMOTE_IRR_BIT      (1u << 14)
#define LVT_TRIGGER_BIT         (1u << 15)  // trigger mode: 0=edge 1=level
#define LVT_MASK_BIT            (1u << 16)  // 1=masked
#define LVT_TIMER_MODE_SHIFT    17
#define LVT_TIMER_MODE_MASK     (0x3u << LVT_TIMER_MODE_SHIFT)


#define TIMER_ONE_SHOT          0u
#define TIMER_PERIODIC          1u
#define TIMER_TSC_DEADLINE      2u


#define DM_FIXED                0u
#define DM_LOWEST               1u
#define DM_SMI                  2u
#define DM_RESERVED             3u
#define DM_NMI                  4u
#define DM_INIT                 5u
#define DM_SIPI                 6u
#define DM_EXTINT               7u


#define ICR_DST_NONE            0u   // use destination field
#define ICR_DST_SELF            1u   // self only
#define ICR_DST_ALL_INCL        2u   // all including self
#define ICR_DST_ALL_EXCL        3u   // all excluding self

#define ICR_DEST_MODE_LOGICAL   (1u << 11)  // 0=physical 1=logical
#define ICR_LEVEL_ASSERT        (1u << 14)  // 1=assert 0=deassert (INIT only)
#define ICR_TRIGGER_LEVEL       (1u << 15)  // 1=level 0=edge

#define SVR_APIC_ENABLE         (1u << 8)
#define SVR_FOCUS_DISABLE       (1u << 9)
#define SVR_EOI_SUPPRESS        (1u << 12)
#define ESR_SEND_CHECKSUM       (1u << 0)
#define ESR_RECV_CHECKSUM       (1u << 1)
#define ESR_SEND_ACCEPT         (1u << 2)
#define ESR_RECV_ACCEPT         (1u << 3)
#define ESR_REDIRECTABLE_IPI    (1u << 4)
#define ESR_SEND_ILLEGAL_VEC    (1u << 5)
#define ESR_RECV_ILLEGAL_VEC    (1u << 6)
#define ESR_ILLEGAL_REG_ADDR    (1u << 7)

#define APIC_VERSION_ID         0x14u       // xAPIC
#define APIC_LVT_ENTRIES        6u          // LVT entries (timer,therm,perf,lint0,lint1,err)
#define APIC_VERSION_REG        (APIC_VERSION_ID | ((APIC_LVT_ENTRIES - 1) << 16))
#define IOAPIC_REG_ID           0x00u
#define IOAPIC_REG_VER          0x01u
#define IOAPIC_REG_ARB          0x02u
#define IOAPIC_REDIR_BASE       0x10u
#define IOAPIC_NUM_PINS         24u
#define IOAPIC_VERSION_REG      0x00170020u  // ver=0x20, max_redir=23
typedef struct {
    uint32_t id;
    uint32_t version;       // APIC version register value
    uint32_t tpr;           // Task Priority Register
    uint32_t ldr;           // Logical Destination Register  [31:24]
    uint32_t dfr;           // Destination Format Register   [31:28]
    uint32_t svr;           // Spurious Vector Register
    uint32_t lvt_timer;
    uint32_t lvt_thermal;
    uint32_t lvt_perf;
    uint32_t lvt_lint0;
    uint32_t lvt_lint1;
    uint32_t lvt_error;
    uint32_t lvt_cmci;
    uint32_t icr_lo;
    uint32_t icr_hi;
    uint32_t esr;           // committed ESR (read by guest)
    uint32_t esr_pending;   // internal error accumulator
    uint32_t irr[8];        // Interrupt Request Register
    uint32_t isr[8];        // In-Service Register
    uint32_t tmr[8];        // Trigger Mode Register
    uint32_t timer_icr;     // initial count  (guest write)
    uint32_t timer_ccr;     // current count  (updated by APICThread)
    uint32_t timer_dcr;     // divide config

    bool     enabled;       // true when SVR[8] set
    bool     focus_check;   // true when focus processor checking enabled
} LAPICState;
typedef struct {
    uint32_t low;    // vector[7:0], delmode[10:8], destmode[11], status[12],
                     // polarity[13], remirr[14], trigger[15], mask[16]
    uint32_t high;   // dest[31:24] (physical or logical)
    uint32_t pin_nums;
} IOREDEntry;
typedef struct {
    uint8_t    id;
    uint8_t    regsel;          // selected internal register (written to IOREGSEL)
    uint8_t    arb_id;
    IOREDEntry redir[IOAPIC_NUM_PINS];
    uint8_t    pin_state[IOAPIC_NUM_PINS];   // current level for each pin
} IOAPICState;
struct LIOAPIC {
	void* CPU_DATA;
    LAPICState state;
    //IOAPICState ioapic;
};
extern IOAPICState ioapic;
uint64_t lapic_mmio_read(uint64_t offset, void* user_data);
void lapic_mmio_write(uint64_t offset, uint64_t val, void* user_data);
uint64_t IOapic_mmio_read(uint64_t offset, void* user_data);
void IOapic_mmio_write(uint64_t offset, uint64_t val, void* user_data);
void Init_APIC(LIOAPIC* apicCtx);

#include "APIC.h"

static void lapic_init(LAPICState* s, uint8_t id)
{
    s->id = id;
    s->version = APIC_VERSION_REG;
    s->tpr = 0;
    s->ldr = 0;
    s->dfr = 0xFFFFFFFFu;   // flat model
    s->svr = 0x000000FFu;   // disabled, spurious=0xFF
    s->esr = 0;
    s->esr_pending = 0;
    s->icr_lo = 0x300;
    s->icr_hi = 0x310;

    s->timer_icr = 0;
    s->timer_ccr = 0;
    s->timer_dcr = 0xb;

    s->enabled = false;
    s->focus_check = true;

    // All LVT entries start masked (bit 16 set), vector 0
    s->lvt_timer = LVT_MASK_BIT;
    s->lvt_thermal = LVT_MASK_BIT;
    s->lvt_perf = LVT_MASK_BIT;
    s->lvt_lint0 = LVT_MASK_BIT;
    s->lvt_lint1 = LVT_MASK_BIT;
    s->lvt_error = LVT_MASK_BIT;
    s->lvt_cmci = LVT_MASK_BIT;

    memset(s->irr, 0, sizeof(s->irr));
    memset(s->isr, 0, sizeof(s->isr));
    memset(s->tmr, 0, sizeof(s->tmr));

    printf("[LAPIC] Init id=%u version=0x%08x\n", id, s->version);
}
static void ioapic_init(IOAPICState* s)
{
    memset(s, 0, sizeof(IOAPICState));
    s->id = 0;
    s->regsel = 0;
    s->arb_id = 0;

    for (int i = 0; i < (int)IOAPIC_NUM_PINS; i++) {
        // All pins: masked, edge, physical, fixed, vector=0xFF
        s->redir[i].low = LVT_MASK_BIT;
        s->redir[i].high = 0;
        s->pin_state[i] = 0;
    }
    printf("[IOAPIC] Init %u pins\n", IOAPIC_NUM_PINS);
}

static inline int bmp_high(const uint32_t* b)
{
    for (int i = 7; i >= 0; i--) {
        if (b[i]) {
#ifdef _MSC_VER
            unsigned long idx;
            _BitScanReverse(&idx, b[i]);
            return i * 32 + (int)idx;
#else
            return i * 32 + 31 - __builtin_clz(b[i]);
#endif
        }
    }
    return -1;
}
static int lapic_get_isrv(const LAPICState* s)
{
    return bmp_high(s->isr);   
}
static int lapic_get_ppr(const LAPICState* s)
{
    // PPR = max(TPR, ISR highest class)
    // Class = vector & 0xF0
    int  isrv = lapic_get_isrv(s);
    int  tpr_cls = (int)(s->tpr & 0xF0u);
    int  isr_cls = (isrv >= 0) ? (isrv & 0xF0) : 0;
    return (tpr_cls > isr_cls) ? tpr_cls : isr_cls;
}
static uint32_t ioapic_read_reg(IOAPICState* s, uint8_t reg)
{
    if (reg == IOAPIC_REG_ID)  return (uint32_t)s->id << 24;
    if (reg == IOAPIC_REG_VER) return IOAPIC_VERSION_REG;
    if (reg == IOAPIC_REG_ARB) return (uint32_t)s->arb_id << 24;

    if (reg >= IOAPIC_REDIR_BASE) {
        uint8_t idx = reg - IOAPIC_REDIR_BASE;
        uint8_t pin = idx / 2;
        bool    hi = (idx & 1) != 0;
        if (pin < IOAPIC_NUM_PINS)
            return hi ? s->redir[pin].high : s->redir[pin].low;
    }

    printf("[IOAPIC] Read unknown reg=0x%02x\n", reg);
    return 0xFFFFFFFFu;
}
static void ioapic_write_reg(IOAPICState* s, uint8_t reg, uint32_t val,LAPICState* lapic)
{
    if (reg == IOAPIC_REG_ID) {
        s->id = (uint8_t)((val >> 24) & 0xFu);
        printf("[IOAPIC] ID=0x%02x\n", s->id);
        return;
    }

    if (reg == IOAPIC_REG_VER || reg == IOAPIC_REG_ARB)
        return;   // read-only

    if (reg >= IOAPIC_REDIR_BASE) {
        uint8_t idx = reg - IOAPIC_REDIR_BASE;
        uint8_t pin = idx / 2;
        bool    hi = (idx & 1) != 0;
        if (pin >= IOAPIC_NUM_PINS) {
            printf("[IOAPIC] Redir write out of range pin=%u\n", pin);
            return;
        }

        if (hi) {
            s->redir[pin].high = val & 0xFF000000u;
        }
        else {
            // Low word: preserve read-only remote IRR and delivery status bits.
            uint32_t ro_bits = s->redir[pin].low & (LVT_REMOTE_IRR_BIT | LVT_STATUS_BIT);
            s->redir[pin].low = (val & ~(LVT_REMOTE_IRR_BIT | LVT_STATUS_BIT)) | ro_bits;
        }

        // Log when low word is written (entry is now complete/updated).
        if (!hi) {
            printf("[IOAPIC] pin=%2u vec=0x%02x dest=0x%02x delmode=%u "
                "trigger=%s polarity=%s destmode=%s %s\n",
                pin,
                s->redir[pin].low & 0xFFu,
                (s->redir[pin].high >> 24) & 0xFFu,
                (s->redir[pin].low >> 8) & 0x7u,
                (s->redir[pin].low & LVT_TRIGGER_BIT) ? "level" : "edge",
                (s->redir[pin].low & LVT_POLARITY_BIT) ? "low" : "high",
                (s->redir[pin].low & ICR_DEST_MODE_LOGICAL) ? "logical" : "physical",
                (s->redir[pin].low & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        }
        return;
    }

    printf("[IOAPIC] Write unknown reg=0x%02x val=0x%08x\n", reg, val);
}
void lapic_mmio_write(uint64_t offset, uint64_t val, void* user_data) {
    LIOAPIC* lapic = (LIOAPIC*)user_data;
    LAPICState* s = &lapic->state;
    switch (offset) {

    case LAPIC_REG_ID:
        printf("[LAPIC] ID=0x%02x\n", s->id);
        break;

        // ── TPR ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_TPR:
        printf("[LAPIC] TPR=0x%02x ppr=%d\n", s->tpr);
        // Raising TPR may now unblock a pending interrupt
        break;

        // ── EOI ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_EOI:
        break;

        // ── Logical destination / format ────────────────────────────────────────
    case LAPIC_REG_LDR:
        s->ldr = val & 0xFF000000u;
        printf("[LAPIC] LDR=0x%08x\n", s->ldr);
        break;

    case LAPIC_REG_DFR:
        // Low 28 bits read as 1; only top nibble is writable.
        s->dfr = val | 0x0FFFFFFFu;
        printf("[LAPIC] DFR=0x%08x (%s)\n", s->dfr,
            ((s->dfr >> 28) == 0xF) ? "flat" : "cluster");
        break;

        // ── SVR ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_SVR: {
        bool was_enabled = s->enabled;
        s->svr = val;
        s->enabled = (val & SVR_APIC_ENABLE) != 0;
        s->focus_check = !(val & SVR_FOCUS_DISABLE);

        printf("[LAPIC] SVR=0x%08x %s spurious=0x%02x\n",
            val, s->enabled ? "ENABLED" : "DISABLED", val & 0xFF);

        if (!s->enabled && was_enabled) {
            // Software-disable: mask all LVT entries (Bochs behaviour)
            s->lvt_timer |= LVT_MASK_BIT;
            s->lvt_thermal |= LVT_MASK_BIT;
            s->lvt_perf |= LVT_MASK_BIT;
            s->lvt_lint0 |= LVT_MASK_BIT;
            s->lvt_lint1 |= LVT_MASK_BIT;
            s->lvt_error |= LVT_MASK_BIT;
            s->lvt_cmci |= LVT_MASK_BIT;
        }
        break;
    }

                      // ── ESR: write to commit pending, cleared by next read ──────────────────
    case LAPIC_REG_ESR:
        s->esr = s->esr_pending;
        s->esr_pending = 0;
        break;

        // ── ICR ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_ICR_HI:
        s->icr_hi = val & 0xFF000000u;
        break;

    case LAPIC_REG_ICR_LO:
        // Writing ICR_LO triggers the IPI.
        s->icr_lo = val & ~LVT_STATUS_BIT;   // clear delivery status
        // Delivery status: clear immediately (we model synchronous delivery)
        s->icr_lo &= ~LVT_STATUS_BIT;
        break;

        // ── LVT Timer ───────────────────────────────────────────────────────────
    case LAPIC_REG_LVT_TIMER:
        s->lvt_timer = val;
        printf("[LAPIC] LVT Timer vec=0x%02x mode=%u %s\n",
            val & 0xFF, (val >> LVT_TIMER_MODE_SHIFT) & 0x3u,
            (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        // Wake APICThread to recalculate sleep
        break;

        // ── LVT Thermal ─────────────────────────────────────────────────────────
    case LAPIC_REG_LVT_THERM:
        s->lvt_thermal = val;
        printf("[LAPIC] LVT Thermal vec=0x%02x %s\n",
            val & 0xFF, (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

        // ── LVT Perf ────────────────────────────────────────────────────────────
    case LAPIC_REG_LVT_PERF:
        s->lvt_perf = val;
        printf("[LAPIC] LVT Perf vec=0x%02x %s\n",
            val & 0xFF, (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

        // ── LVT LINT0/LINT1 ─────────────────────────────────────────────────────
    case LAPIC_REG_LVT_LINT0:
        s->lvt_lint0 = val;
        printf("[LAPIC] LVT LINT0 vec=0x%02x delmode=%u trigger=%s %s\n",
            val & 0xFF, (val >> 8) & 0x7u,
            (val & LVT_TRIGGER_BIT) ? "level" : "edge",
            (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

    case LAPIC_REG_LVT_LINT1:
        s->lvt_lint1 = val;
        printf("[LAPIC] LVT LINT1 vec=0x%02x delmode=%u trigger=%s %s\n",
            val & 0xFF, (val >> 8) & 0x7u,
            (val & LVT_TRIGGER_BIT) ? "level" : "edge",
            (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

        // ── LVT Error ───────────────────────────────────────────────────────────
    case LAPIC_REG_LVT_ERROR:
        s->lvt_error = val;
        printf("[LAPIC] LVT Error vec=0x%02x %s\n",
            val & 0xFF, (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

        // ── LVT CMCI ────────────────────────────────────────────────────────────
    case LAPIC_REG_CMCI:
        s->lvt_cmci = val;
        printf("[LAPIC] LVT CMCI vec=0x%02x %s\n",
            val & 0xFF, (val & LVT_MASK_BIT) ? "MASKED" : "UNMASKED");
        break;

        // ── Timer ICR ───────────────────────────────────────────────────────────
    case LAPIC_REG_TIMER_ICR:
        s->timer_icr = val;
        s->timer_ccr = val;   // reload CCR immediately (Bochs behaviour)
        printf("[LAPIC] Timer ICR=0x%08x (CCR reloaded)\n", val);
        // lapic_mmio_writeback is a no-op under uc_mmio_map; reads are live.
        // Wake APICThread to restart sleep calculation
        break;

        // ── Timer CCR (read-only to guest) ──────────────────────────────────────
    case LAPIC_REG_TIMER_CCR:
        printf("[LAPIC] Timer CCR write ignored (read-only)\n");
        
        break;

        // ── Timer DCR ───────────────────────────────────────────────────────────
    case LAPIC_REG_TIMER_DCR:
        s->timer_dcr = val & 0xBu;   // only bits [3,1:0] are valid
        break;
    case LAPIC_REG_RRD:
        break;   // ignore writes
    case LAPIC_REG_APR:
    case LAPIC_REG_PPR:
        break;   // ignore writes

    default:
        printf("[LAPIC] Write reserved/unknown offset=0x%03x val=0x%08x\n", offset, val);
        break;
    }
}
uint64_t lapic_mmio_read(uint64_t offset, void* user_data) {
    LIOAPIC* lapic = (LIOAPIC*)user_data;
    LAPICState* s = &lapic->state;
    switch (offset) {

        // ── Identity ────────────────────────────────────────────────────────────
    case LAPIC_REG_ID:
        return (s->id & 0xFFu) << 24;

    case LAPIC_REG_VER:
        return s->version;

        // ── Priority ────────────────────────────────────────────────────────────
    case LAPIC_REG_TPR:
        return s->tpr & 0xFFu;

    case LAPIC_REG_APR:
        // Arbitration Priority = max(TPR, ISR highest class)
        return (uint32_t)lapic_get_ppr(s) & 0xFFu;

    case LAPIC_REG_PPR:
        return (uint32_t)lapic_get_ppr(s) & 0xFFu;

        // ── EOI (write-only; read returns 0) ────────────────────────────────────
    case LAPIC_REG_EOI:
        return 0;

        // ── Logical / format ────────────────────────────────────────────────────
    case LAPIC_REG_LDR:
        return s->ldr;

    case LAPIC_REG_DFR:
        return s->dfr;

    case LAPIC_REG_SVR:
        return s->svr;

        // ── ESR: commit pending errors on read ──────────────────────────────────
    case LAPIC_REG_ESR:
        s->esr = s->esr_pending;
        s->esr_pending = 0;
        return s->esr;

        // ── ICR ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_ICR_LO:  return s->icr_lo;
    case LAPIC_REG_ICR_HI:  return s->icr_hi;

        // ── LVT ─────────────────────────────────────────────────────────────────
    case LAPIC_REG_LVT_TIMER:  return s->lvt_timer;
    case LAPIC_REG_LVT_THERM:  return s->lvt_thermal;
    case LAPIC_REG_LVT_PERF:   return s->lvt_perf;
    case LAPIC_REG_LVT_LINT0:  return s->lvt_lint0;
    case LAPIC_REG_LVT_LINT1:  return s->lvt_lint1;
    case LAPIC_REG_LVT_ERROR:  return s->lvt_error;
    case LAPIC_REG_CMCI:        return s->lvt_cmci;

        // ── Timer ───────────────────────────────────────────────────────────────
    case LAPIC_REG_TIMER_ICR:  return s->timer_icr;
    case LAPIC_REG_TIMER_CCR:  return s->timer_ccr;   // APICThread keeps this live
    case LAPIC_REG_TIMER_DCR:  return s->timer_dcr;

        // ── ISR 0x100..0x170 ────────────────────────────────────────────────────
    case LAPIC_REG_ISR0 + 0x00: return s->isr[0];
    case LAPIC_REG_ISR0 + 0x10: return s->isr[1];
    case LAPIC_REG_ISR0 + 0x20: return s->isr[2];
    case LAPIC_REG_ISR0 + 0x30: return s->isr[3];
    case LAPIC_REG_ISR0 + 0x40: return s->isr[4];
    case LAPIC_REG_ISR0 + 0x50: return s->isr[5];
    case LAPIC_REG_ISR0 + 0x60: return s->isr[6];
    case LAPIC_REG_ISR0 + 0x70: return s->isr[7];

        // ── TMR 0x180..0x1F0 ────────────────────────────────────────────────────
    case LAPIC_REG_TMR0 + 0x00: return s->tmr[0];
    case LAPIC_REG_TMR0 + 0x10: return s->tmr[1];
    case LAPIC_REG_TMR0 + 0x20: return s->tmr[2];
    case LAPIC_REG_TMR0 + 0x30: return s->tmr[3];
    case LAPIC_REG_TMR0 + 0x40: return s->tmr[4];
    case LAPIC_REG_TMR0 + 0x50: return s->tmr[5];
    case LAPIC_REG_TMR0 + 0x60: return s->tmr[6];
    case LAPIC_REG_TMR0 + 0x70: return s->tmr[7];

        // ── IRR 0x200..0x270 ────────────────────────────────────────────────────
    case LAPIC_REG_IRR0 + 0x00: return s->irr[0];
    case LAPIC_REG_IRR0 + 0x10: return s->irr[1];
    case LAPIC_REG_IRR0 + 0x20: return s->irr[2];
    case LAPIC_REG_IRR0 + 0x30: return s->irr[3];
    case LAPIC_REG_IRR0 + 0x40: return s->irr[4];
    case LAPIC_REG_IRR0 + 0x50: return s->irr[5];
    case LAPIC_REG_IRR0 + 0x60: return s->irr[6];
    case LAPIC_REG_IRR0 + 0x70: return s->irr[7];

    default:
        printf("[LAPIC] Read reserved/unknown offset=0x%03x\n", offset);
        //lapic_set_esr(s, ESR_ILLEGAL_REG_ADDR);
        return 0xFFFFFFFFu;
    }
}
uint64_t IOapic_mmio_read(uint64_t offset, void* user_data) {
    LIOAPIC* ctx = (LIOAPIC*)user_data;

    // Only the window register at +0x10 is readable.
    if (offset == 0x10) {
        uint32_t val = ioapic_read_reg(&ctx->ioapic, ctx->ioapic.regsel);
        printf("[IOAPIC Read]  reg=0x%02x = 0x%08x\n", ctx->ioapic.regsel, val);
        return (uint64_t)val;
    }

    // IOREGSEL (+0x00) reads back the last value written to it.
    if (offset == 0x00)
        return (uint64_t)ctx->ioapic.regsel;

    printf("[IOAPIC Read]  unknown offset=0x%02x\n", (uint32_t)offset);
    return 0xFFFFFFFFu;
}
void IOapic_mmio_write(uint64_t offset,uint64_t value, void* user_data) {
    LIOAPIC* ctx = (LIOAPIC*)user_data;
    uint32_t val = (uint32_t)value;

    if (offset == 0x00) {
        // IOREGSEL: select the indirect register to access.
        ctx->ioapic.regsel = (uint8_t)(val & 0xFFu);
        printf("[IOAPIC] REGSEL=0x%02x\n", ctx->ioapic.regsel);
    }
    else if (offset == 0x10) {
        // IOWIN: write through to the currently selected register.
        printf("[IOAPIC Write] reg=0x%02x val=0x%08x\n", ctx->ioapic.regsel, val);
        ioapic_write_reg(&ctx->ioapic, ctx->ioapic.regsel, val, &ctx->state);
    }
    else {
        printf("[IOAPIC Write] unknown offset=0x%02x val=0x%08x\n",
            (uint32_t)offset, val);
    }
}

void Init_APIC(LIOAPIC* apicCtx) {
    lapic_init(&apicCtx->state,0);
    ioapic_init(&apicCtx->ioapic);
}
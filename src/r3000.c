#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "psx.h"
#include "r3000.h"

#define R3000_RESET_VECTOR      0xbfc00000
#define R3000_EXCEPTION_VECTOR0 0x80000080
#define R3000_EXCEPTION_VECTOR1 0xbfc00180

#define R3000_COP0_SR_IEC       0x00000001
#define R3000_COP0_SR_KUC       0x00000002
#define R3000_COP0_SR_ISC       0x00010000
#define R3000_COP0_SR_TS        0x00200000
#define R3000_COP0_SR_BEV       0x00400000

#define R3000_COP0_SR_EX_BLK    0x0000003f

#define R3000_COP0_CAUSE_EX     0x0000007c
#define R3000_COP0_CAUSE_IP_W   0x00000300
#define R3000_COP0_CAUSE_BD     0x80000000

static const char *R3000_REGISTERS[R3000_NR_REGISTERS] = {
    "$zr",
    "$at",
    "$v0", "$v1",
    "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9",
    "$k0", "$k1",
    "$gp",
    "$sp",
    "$fp",
    "$ra",
    "$hi", "$lo"
};

static const char *R3000_COP0_REGISTERS[R3000_COP0_NR_REGISTERS] = {
    "$err", "$err", "$err",
    "$bpc",
    "$err",
    "$bda",
    "$jumpdest",
    "$dcic",
    "$badvaddr",
    "$bdam",
    "$err",
    "$bpcm",
    "$sr",
    "$cause",
    "$epc",
    "$prid",
    "$err", "$err", "$err", "$err", "$err", "$err", "$err", "$err",
    "$err", "$err", "$err", "$err", "$err", "$err", "$err", "$err"
};

static const uint32_t R3000_VIRTADDR_MASKS[8] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,     /* KUSEG */
    0x7fffffff,                                         /* KSEG0 */
    0x1fffffff,                                         /* KSEG1 */
    0xffffffff, 0xffffffff                              /* KSEG2 */
};

struct r3000 {
    uint32_t pc, current_pc, next_pc;
    uint32_t gpr[R3000_NR_REGISTERS];

    bool branch, branch_delay;

    struct {
        uint32_t sr;
        uint32_t cause;
        uint32_t epc;
    } cop0;
};

static struct r3000 r3000;

static uint32_t
r3000_cop0_sr_read(void)
{
    return r3000.cop0.sr;
}

static void
r3000_cop0_sr_write(uint32_t value)
{
    printf("cop0: info: writing 0x%08x to sr\n", value);

    r3000.cop0.sr = value;
}

static bool
r3000_cop0_sr_isc(void)
{
    return r3000.cop0.sr & R3000_COP0_SR_ISC;
}

static uint32_t
r3000_cop0_cause_read(void)
{
    return r3000.cop0.cause;
}

static void
r3000_cop0_cause_write(uint32_t value)
{
    printf("cop0: info: writing 0x%08x to cause\n", value);

    r3000.cop0.cause &= ~R3000_COP0_CAUSE_IP_W;
    r3000.cop0.cause |= (value & R3000_COP0_CAUSE_IP_W);
}

static uint32_t
r3000_cop0_epc_read(void)
{
    return r3000.cop0.epc;
}

static void
r3000_cop0_soft_reset(uint32_t epc)
{
    r3000.cop0.sr |= R3000_COP0_SR_BEV;         /* Set 'Boot Exception Vectors' flag */
    r3000.cop0.sr |= R3000_COP0_SR_TS;          /* Set 'TLB Shutdown' flag */
    r3000.cop0.sr &= ~R3000_COP0_SR_KUC;        /* Set processor to kernel mode */
    r3000.cop0.sr &= ~R3000_COP0_SR_IEC;        /* Disable interrupts */

    r3000.cop0.epc = epc;
}

static void
r3000_cop0_hard_reset(void)
{
    r3000.cop0.sr = 0;
    r3000.cop0.cause = 0;
    r3000.cop0.epc = 0;
}

static uint32_t
r3000_cop0_exception(enum R3000Exception e, bool branch_delay, uint32_t epc)
{
    uint32_t prev_ex_blk;

    prev_ex_blk = r3000.cop0.sr & R3000_COP0_SR_EX_BLK;         /* Backup previous exception block */

    r3000.cop0.sr &= ~R3000_COP0_SR_EX_BLK;                     /* Clear exception block */
    r3000.cop0.sr |= (prev_ex_blk << 2) & R3000_COP0_SR_EX_BLK; /* Shift exception block */

    r3000.cop0.cause &= ~R3000_COP0_CAUSE_BD;                   /* Clear BD bit */
    r3000.cop0.cause |= branch_delay ? R3000_COP0_CAUSE_BD : 0; /* Set new BD bit */

    r3000.cop0.cause &= ~R3000_COP0_CAUSE_EX;                   /* Clear excode */
    r3000.cop0.cause |= e << 2;                                 /* Set new excode */

    r3000.cop0.epc = epc;

    return (r3000.cop0.sr & R3000_COP0_SR_BEV) ? R3000_EXCEPTION_VECTOR1
                                                 : R3000_EXCEPTION_VECTOR0;
}

static void
r3000_cop0_exit_exception(void)
{
    uint32_t prev_ex_blk;

    prev_ex_blk = r3000.cop0.sr & R3000_COP0_SR_EX_BLK;         /* Backup previous exception block */

    r3000.cop0.sr &= ~R3000_COP0_SR_EX_BLK;                     /* Clear exception block */
    r3000.cop0.sr |= (prev_ex_blk >> 2) & R3000_COP0_SR_EX_BLK; /* Shift exception block */
}

const char *
r3000_register_name(unsigned int reg)
{
    assert(reg < R3000_NR_REGISTERS);

    return R3000_REGISTERS[reg];
}

const char *
r3000_cop0_register_name(unsigned int reg)
{
    assert(reg < R3000_NR_REGISTERS);

    return R3000_COP0_REGISTERS[reg];
}

void
r3000_setup(void)
{
    r3000_hard_reset();
}

void
r3000_soft_reset(void)
{
    r3000_cop0_soft_reset(r3000.current_pc);

    r3000.pc = r3000.current_pc = R3000_RESET_VECTOR;
    r3000.next_pc = r3000.pc + 4;

    r3000.branch = r3000.branch_delay = false;
}

void
r3000_hard_reset(void)
{
    r3000_cop0_hard_reset();

    memset(r3000.gpr, 0, sizeof(r3000.gpr));

    r3000_soft_reset();
}

void
r3000_exception(enum R3000Exception e)
{
    uint32_t epc, vector;

    printf("r3000: info: entering exception 0x%x\n", e);

    epc = r3000.branch_delay ? r3000.current_pc - 4 : r3000.current_pc;
    vector = r3000_cop0_exception(e, r3000.branch_delay, epc);

    r3000.pc = vector;
    r3000.next_pc = r3000.pc + 4;
}

void
r3000_exit_exception(void)
{
    r3000_cop0_exit_exception();
}

uint32_t
r3000_read_pc(void)
{
    return r3000.pc;
}

uint32_t
r3000_read_next_pc(void)
{
    return r3000.next_pc;
}

void
r3000_jump(uint32_t address)
{
    r3000.branch = true;
    r3000.next_pc = address;
}

void
r3000_branch(uint32_t offset)
{
    r3000.branch = true;
    r3000.next_pc = r3000.pc + offset;
}

uint32_t
r3000_read_reg(unsigned int reg)
{
    assert(reg < R3000_NR_REGISTERS);

    return r3000.gpr[reg];
}

void
r3000_write_reg(unsigned int reg, uint32_t value)
{
    assert(reg < R3000_NR_REGISTERS);

    if (reg == 0) {
        return;
    }

    r3000.gpr[reg] = value;
}

uint32_t
r3000_cop0_read(unsigned int reg)
{
    assert(reg < R3000_NR_REGISTERS);

    switch (reg) {
    case 12:
        return r3000_cop0_sr_read();
    case 13:
        return r3000_cop0_cause_read();
    case 14:
        return r3000_cop0_epc_read();
    default:
        PANIC;
        break;
    }

    return 0;
}

void
r3000_cop0_write(unsigned int reg, uint32_t value)
{
    assert(reg < R3000_NR_REGISTERS);

    switch (reg) {
    case 3:
        break;
    case 5:
        break;
    case 6:
        break;
    case 7:
        break;
    case 9:
        break;
    case 11:
        break;
    case 12:
        r3000_cop0_sr_write(value);
        break;
    case 13:
        r3000_cop0_cause_write(value);
        break;
    default:
        PANIC;
        break;
    }
}

static uint32_t
r3000_translate_virtaddr(uint32_t address)
{
    uint32_t mask;

    mask = R3000_VIRTADDR_MASKS[address >> 29];
    return address & mask;
}

uint32_t
r3000_read_code(void)
{
    uint32_t result;

    if (r3000.pc & 0x3) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_LOAD);
        return 0;
    }

    result = psx_read_memory32(r3000_translate_virtaddr(r3000.pc));

    r3000.current_pc = r3000.pc;
    r3000.pc = r3000.next_pc;
    r3000.next_pc += 4;

    r3000.branch_delay = r3000.branch;
    r3000.branch = false;

    return result;
}

uint8_t
r3000_read_memory8(uint32_t address)
{
    if (r3000_cop0_sr_isc()) {
        return 0;
    }

    return psx_read_memory8(r3000_translate_virtaddr(address));
}

uint16_t
r3000_read_memory16(uint32_t address)
{
    assert(!(address & 0x1));

    if (r3000_cop0_sr_isc()) {
        return 0;
    }

    return psx_read_memory16(r3000_translate_virtaddr(address));
}

uint32_t
r3000_read_memory32(uint32_t address)
{
    assert(!(address & 0x3));

    if (r3000_cop0_sr_isc()) {
        return 0;
    }

    return psx_read_memory32(r3000_translate_virtaddr(address));
}

void
r3000_write_memory8(uint32_t address, uint8_t value)
{
    if (r3000_cop0_sr_isc()) {
        return;
    }

    psx_write_memory8(r3000_translate_virtaddr(address), value);
}

void
r3000_write_memory16(uint32_t address, uint16_t value)
{
    assert(!(address & 0x1));

    if (r3000_cop0_sr_isc()) {
        return;
    }

    psx_write_memory16(r3000_translate_virtaddr(address), value);
}

void
r3000_write_memory32(uint32_t address, uint32_t value)
{
    assert(!(address & 0x3));

    if (r3000_cop0_sr_isc()) {
        return;
    }

    psx_write_memory32(r3000_translate_virtaddr(address), value);
}

uint32_t
r3000_debug_read_memory32(uint32_t address)
{
    return psx_debug_read_memory32(r3000_translate_virtaddr(address));
}

void
r3000_debug_write_memory32(uint32_t address, uint32_t value)
{
    psx_debug_write_memory32(r3000_translate_virtaddr(address), value);
}

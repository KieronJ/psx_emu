#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "psx.h"
#include "r3000.h"

#define R3000_NR_REGISTERS      32
#define R3000_RESET_VECTOR      0xbfc00000


#define R3000_COP0_SR_IEC       0x00000001
#define R3000_COP0_SR_KUC       0x00000002
#define R3000_COP0_SR_ISC       0x00010000
#define R3000_COP0_SR_TS        0x00200000
#define R3000_COP0_SR_BEV       0x00400000

const char *R3000_REGISTERS[R3000_NR_REGISTERS] = {
    "$zero",
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
    "$ra"
};

const char *R3000_COP0_REGISTERS[R3000_NR_REGISTERS] = {
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

struct r3000 {
    uint32_t pc, next_pc;
    uint32_t gpr[R3000_NR_REGISTERS];
    uint32_t lo, hi;

    struct {
        uint32_t sr;
        uint32_t cause;
        uint32_t epc;
    } cop0;
};

static struct r3000 r3000;

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

static void
r3000_cop0_reset(uint32_t pc)
{
    r3000.cop0.sr |= R3000_COP0_SR_BEV;         /* Set 'Boot Exception Vectors' flag */
    r3000.cop0.sr |= R3000_COP0_SR_TS;          /* Set 'TLB Shutdown' flag */
    r3000.cop0.sr &= ~R3000_COP0_SR_KUC;        /* Set processor to kernel mode */
    r3000.cop0.sr &= ~R3000_COP0_SR_IEC;        /* Disable interrupts */

    r3000.cop0.epc = pc;
}

void
r3000_setup(void)
{
    r3000_reset();
    r3000_cop0_reset(r3000.pc);

    memset(r3000.gpr, 0, sizeof(r3000.gpr));

    r3000.lo = 0;
    r3000.hi = 0;
}

void
r3000_reset(void)
{
    r3000.pc = R3000_RESET_VECTOR;
    r3000.next_pc = r3000.pc + 4;
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

uint32_t
r3000_read_pc(void)
{
    return r3000.pc;
}

void
r3000_jump(uint32_t address)
{
    r3000.next_pc = address;
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

    r3000.gpr[reg] = value;
}

void
r3000_cop0_write(unsigned int reg, uint32_t value)
{
    assert(reg < R3000_NR_REGISTERS);

    switch (reg) {
    case 12:
        r3000_cop0_sr_write(value);
        break;
    default:
        PANIC;
        break;
    }
}

uint32_t
r3000_read_code(void)
{
    uint32_t result;

    result = psx_read_memory32(r3000.pc);

    r3000.pc = r3000.next_pc;
    r3000.next_pc += 4;

    return result;
}

uint32_t
r3000_read_memory32(uint32_t address)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated read\n");
        return 0;
    }

    return psx_read_memory32(address);
}

void
r3000_write_memory32(uint32_t address, uint32_t value)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated write\n");
        return;
    }

    psx_write_memory32(address, value);
}

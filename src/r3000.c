#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "macros.h"
#include "psx.h"
#include "r3000.h"

#define R3000_RESET_VECTOR      0xbfc00000

#define R3000_COP0_SR_IEC       0x00000001
#define R3000_COP0_SR_KUC       0x00000002
#define R3000_COP0_SR_ISC       0x00010000
#define R3000_COP0_SR_TS        0x00200000
#define R3000_COP0_SR_BEV       0x00400000

#define R3000_COP0_CAUSE_IP_W   0x00000300

const char *R3000_REGISTERS[R3000_NR_REGISTERS] = {
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

const uint32_t R3000_VIRTADDR_MASKS[8] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,     /* KUSEG */
    0x7fffffff,                                         /* KSEG0 */
    0x1fffffff,                                         /* KSEG1 */
    0xffffffff, 0xffffffff                              /* KSEG2 */
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

static void
r3000_cop0_cause_write(uint32_t value)
{
    printf("cop0: info: writing 0x%08x to cause\n", value);

    r3000.cop0.cause &= ~R3000_COP0_CAUSE_IP_W;
    r3000.cop0.cause |= (value & R3000_COP0_CAUSE_IP_W);
}

static void
r3000_cop0_soft_reset(uint32_t pc)
{
    r3000.cop0.sr |= R3000_COP0_SR_BEV;         /* Set 'Boot Exception Vectors' flag */
    r3000.cop0.sr |= R3000_COP0_SR_TS;          /* Set 'TLB Shutdown' flag */
    r3000.cop0.sr &= ~R3000_COP0_SR_KUC;        /* Set processor to kernel mode */
    r3000.cop0.sr &= ~R3000_COP0_SR_IEC;        /* Disable interrupts */

    r3000.cop0.epc = pc;
}

static void
r3000_cop0_hard_reset(void)
{
    r3000.cop0.sr = 0;
    r3000.cop0.cause = 0;
    r3000.cop0.epc = 0;
}

void
r3000_setup(void)
{
    r3000_hard_reset();
}

void
r3000_soft_reset(void)
{
    r3000_cop0_soft_reset(r3000.pc);

    r3000.pc = R3000_RESET_VECTOR;
    r3000.next_pc = r3000.pc + 4;
}

void
r3000_hard_reset(void)
{
    r3000_cop0_hard_reset();

    memset(r3000.gpr, 0, sizeof(r3000.gpr));
    r3000.lo = 0;
    r3000.hi = 0;

    r3000_soft_reset();
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

uint32_t
r3000_read_next_pc(void)
{
    return r3000.next_pc;
}

void
r3000_jump(uint32_t address)
{
    r3000.next_pc = address;
}

void
r3000_branch(uint32_t offset)
{
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
r3000_read_hi(void)
{
    return r3000.hi;
}

uint32_t
r3000_read_lo(void)
{
    return r3000.lo;
}

uint32_t
r3000_cop0_read(unsigned int reg)
{
    assert(reg < R3000_NR_REGISTERS);

    switch (reg) {
    case 12:
        return r3000_cop0_sr_read();
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

    result = psx_read_memory32(r3000_translate_virtaddr(r3000.pc));

    r3000.pc = r3000.next_pc;
    r3000.next_pc += 4;

    return result;
}

uint8_t
r3000_read_memory8(uint32_t address)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated read\n");
        return 0;
    }

    return psx_read_memory8(r3000_translate_virtaddr(address));
}

uint32_t
r3000_read_memory32(uint32_t address)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated read\n");
        return 0;
    }

    return psx_read_memory32(r3000_translate_virtaddr(address));
}

void
r3000_write_memory8(uint32_t address, uint8_t value)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated write\n");
        return;
    }

    psx_write_memory8(r3000_translate_virtaddr(address), value);
}

void
r3000_write_memory16(uint32_t address, uint16_t value)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated write\n");
        return;
    }

    psx_write_memory16(r3000_translate_virtaddr(address), value);
}

void
r3000_write_memory32(uint32_t address, uint32_t value)
{
    if (r3000_cop0_sr_isc()) {
        printf("r3000: info: cache isolated write\n");
        return;
    }

    psx_write_memory32(r3000_translate_virtaddr(address), value);
}

uint32_t
r3000_debug_read_memory32(uint32_t address)
{
    return psx_debug_read_memory32(r3000_translate_virtaddr(address));
}

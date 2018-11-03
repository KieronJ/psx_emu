#include <stdint.h>
#include <stdio.h>

#include "macros.h"
#include "r3000.h"
#include "r3000_disassembler.h"
#include "r3000_interpreter.h"
#include "util.h"

static void
r3000_interpreter_bcond(uint32_t instruction)
{
    unsigned int rs, rt;
    uint32_t offset;
    int32_t s;
    bool branch, link;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);
    offset = R3000_IMM_SE(instruction);

    s = r3000_read_reg(rs);

    branch = (s ^ ((int32_t)(rt << 31))) < 0;
    link = (rt & 0x1e) == 0x10;

    if (link) {
        r3000_write_reg(31, r3000_read_next_pc());
    }

    if (branch) {
        r3000_branch(offset << 2);
    }
}

static void
r3000_interpreter_j(uint32_t instruction, uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    r3000_jump((address & 0xf0000000) | (target << 2));
}

static void
r3000_interpreter_jal(uint32_t instruction, uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    r3000_write_reg(31, r3000_read_next_pc());
    r3000_jump((address & 0xf0000000) | (target << 2));
}

static void
r3000_interpreter_beq(uint32_t instruction)
{
    unsigned int rs, rt;
    uint32_t offset;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);
    offset = R3000_IMM_SE(instruction);

    if (r3000_read_reg(rs) == r3000_read_reg(rt)) {
        r3000_branch(offset << 2);
    }
}

static void
r3000_interpreter_bne(uint32_t instruction)
{
    unsigned int rs, rt;
    uint32_t offset;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);
    offset = R3000_IMM_SE(instruction);

    if (r3000_read_reg(rs) != r3000_read_reg(rt)) {
        r3000_branch(offset << 2);
    }
}

static void
r3000_interpreter_blez(uint32_t instruction)
{
    unsigned int rs;
    uint32_t offset;

    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    if (((int32_t)r3000_read_reg(rs)) <= 0) {
        r3000_branch(offset << 2);
    }
}

static void
r3000_interpreter_bgtz(uint32_t instruction)
{
    unsigned int rs;
    uint32_t offset;

    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    if (((int32_t)r3000_read_reg(rs)) > 0) {
        r3000_branch(offset << 2);
    }
}

static void
r3000_interpreter_addi(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t s, imm, result;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);

    s = r3000_read_reg(rs);
    imm = R3000_IMM_SE(instruction);

    result = s + imm;

    if (overflow32(s, imm, result)) {
        r3000_exception(R3000_EXCEPTION_OVERFLOW);
    }

    r3000_write_reg(rt, result);
}

static void
r3000_interpreter_addiu(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM_SE(instruction);

    r3000_write_reg(rt, r3000_read_reg(rs) + imm);
}

static void
r3000_interpreter_slti(uint32_t instruction)
{
    unsigned int rt, rs;
    int32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM_SE(instruction);

    r3000_write_reg(rt, ((int32_t)r3000_read_reg(rs)) < imm);
}

static void
r3000_interpreter_sltiu(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM_SE(instruction);

    r3000_write_reg(rt, r3000_read_reg(rs) < imm);
}

static void
r3000_interpreter_andi(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM(instruction);

    r3000_write_reg(rt, r3000_read_reg(rs) & imm);
}

static void
r3000_interpreter_ori(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM(instruction);

    r3000_write_reg(rt, r3000_read_reg(rs) | imm);
}

static void
r3000_interpreter_xori(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t imm;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    imm = R3000_IMM(instruction);

    r3000_write_reg(rt, r3000_read_reg(rs) ^ imm);
}

static void
r3000_interpreter_lui(uint32_t instruction)
{
    unsigned int rt;
    uint32_t imm;

    rt = R3000_RT(instruction);
    imm = R3000_IMM(instruction);

    r3000_write_reg(rt, imm << 16);
}

static void
r3000_interpreter_lb(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    value = (int8_t)r3000_read_memory8(r3000_read_reg(rs) + offset);

    r3000_write_reg(rt, value);
}

static void
r3000_interpreter_lh(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    if (address & 0x1) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_LOAD);
        return;
    }

    value = (int16_t)r3000_read_memory16(address);

    r3000_write_reg(rt, value);
}

static void
r3000_interpreter_lwl(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address, current, aligned, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    current = r3000_read_reg(rt);
    aligned = r3000_read_memory32(address & ~0x3);

    switch (address & 0x3) {
    case 0x0:
        value = (current & 0xffffff) | (aligned << 24);
        break;
    case 0x1:
        value = (current & 0xffff) | (aligned << 16);
        break;
    case 0x2:
        value = (current & 0xff) | (aligned << 8);
        break;
    case 0x3:
        value = aligned;
        break;
    }

    r3000_write_reg(rt, value);
}

static void
r3000_interpreter_lw(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset; 

    if (address & 0x3) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_LOAD);
        return;
    }

    r3000_write_reg(rt, r3000_read_memory32(address));
}

static void
r3000_interpreter_lbu(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    r3000_write_reg(rt, r3000_read_memory8(r3000_read_reg(rs) + offset));
}

static void
r3000_interpreter_lhu(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    if (address & 0x1) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_LOAD);
    }

    r3000_write_reg(rt, r3000_read_memory16(address));
}

static void
r3000_interpreter_lwr(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address, current, aligned, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    current = r3000_read_reg(rt);
    aligned = r3000_read_memory32(address & ~0x3);

    switch (address & 0x3) {
    case 0x0:
        value = aligned;
        break;
    case 0x1:
        value = (current & 0xff000000) | (aligned >> 8);
        break;
    case 0x2:
        value = (current & 0xffff0000) | (aligned >> 16);
        break;
    case 0x3:
        value = (current & 0xffffff00) | (aligned >> 24);
        break;
    }

    r3000_write_reg(rt, value);
}

static void
r3000_interpreter_sb(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    r3000_write_memory8(r3000_read_reg(rs) + offset, r3000_read_reg(rt));
}

static void
r3000_interpreter_sh(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    if (address & 0x1) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_STORE);
        return;
    }

    r3000_write_memory16(address, r3000_read_reg(rt));
}

static void
r3000_interpreter_swl(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address, current, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    current = r3000_read_memory32(address & ~0x3);
    value = r3000_read_reg(rt);
   
    switch (address & 0x3) {
    case 0x0:
        value = (current & 0xffffff00) | (value >> 24);
        break;
    case 0x1:
        value = (current & 0xffff0000) | (value >> 16);
        break;
    case 0x2:
        value = (current & 0xff000000) | (value >> 8);
        break;
    }

    r3000_write_memory32(address & ~0x3, value);
}

static void
r3000_interpreter_sw(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    if (address & 0x3) {
        r3000_exception(R3000_EXCEPTION_ADDRESS_STORE);
        return;
    }

    r3000_write_memory32(address, r3000_read_reg(rt));
}

static void
r3000_interpreter_swr(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset, address, current, value;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    address = r3000_read_reg(rs) + offset;

    current = r3000_read_memory32(address & ~0x3);
    value = r3000_read_reg(rt);
   
    switch (address & 0x3) {
    case 0x1:
        value = (current & 0xff) | (value << 8);
        break;
    case 0x2:
        value = (current & 0xffff) | (value << 16);
        break;
    case 0x3:
        value = (current & 0xffffff) | (value << 24);
        break;
    }

    r3000_write_memory32(address & ~0x3, value);
}

static void
r3000_interpreter_sll(uint32_t instruction)
{
    unsigned int rd, rt, shift;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    shift = R3000_SHIFT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rt) << shift);
}

static void
r3000_interpreter_srl(uint32_t instruction)
{
    unsigned int rd, rt, shift;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    shift = R3000_SHIFT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rt) >> shift);
}

static void
r3000_interpreter_sra(uint32_t instruction)
{
    unsigned int rd, rt, shift;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    shift = R3000_SHIFT(instruction);

    r3000_write_reg(rd, (int32_t)r3000_read_reg(rt) >> shift);
}

static void
r3000_interpreter_sllv(uint32_t instruction)
{
    unsigned int rd, rt, rs;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);

    r3000_write_reg(rd, r3000_read_reg(rt) << r3000_read_reg(rs));
}

static void
r3000_interpreter_srlv(uint32_t instruction)
{
    unsigned int rd, rt, rs;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);

    r3000_write_reg(rd, r3000_read_reg(rt) >> r3000_read_reg(rs));
}

static void
r3000_interpreter_srav(uint32_t instruction)
{
    unsigned int rd, rt, rs;

    rd = R3000_RD(instruction);
    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);

    r3000_write_reg(rd, (int32_t)r3000_read_reg(rt) >> r3000_read_reg(rs));
}

static void
r3000_interpreter_jr(uint32_t instruction)
{
    unsigned int rs;

    rs = R3000_RS(instruction);

    r3000_jump(r3000_read_reg(rs));
}

static void
r3000_interpreter_jalr(uint32_t instruction)
{
    unsigned int rd, rs;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);

    r3000_write_reg(rd, r3000_read_next_pc());

    r3000_jump(r3000_read_reg(rs));
}

static void
r3000_interpreter_syscall(uint32_t instruction)
{
    (void)instruction;

    r3000_exception(R3000_EXCEPTION_SYSCALL);
}

static void
r3000_interpreter_break(uint32_t instruction)
{
    (void)instruction;

    r3000_exception(R3000_EXCEPTION_BREAKPOINT);
}

static void
r3000_interpreter_mfhi(uint32_t instruction)
{
    unsigned int rd;

    rd = R3000_RD(instruction);

    r3000_write_reg(rd, r3000_read_reg(R3000_REGISTER_HI));
}

static void
r3000_interpreter_mthi(uint32_t instruction)
{
    unsigned int rs;

    rs = R3000_RS(instruction);

    r3000_write_reg(R3000_REGISTER_HI, r3000_read_reg(rs));
}

static void
r3000_interpreter_mflo(uint32_t instruction)
{
    unsigned int rd;

    rd = R3000_RD(instruction);

    r3000_write_reg(rd, r3000_read_reg(R3000_REGISTER_LO));
}

static void
r3000_interpreter_mtlo(uint32_t instruction)
{
    unsigned int rs;

    rs = R3000_RS(instruction);

    r3000_write_reg(R3000_REGISTER_LO, r3000_read_reg(rs));
}

static void
r3000_interpreter_mult(uint32_t instruction)
{
    unsigned int rs, rt;
    int64_t a, b;
    uint64_t result;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    a = (int32_t)r3000_read_reg(rs);
    b = (int32_t)r3000_read_reg(rt);

    result = a * b;

    r3000_write_reg(R3000_REGISTER_HI, result >> 32);
    r3000_write_reg(R3000_REGISTER_LO, result);
}

static void
r3000_interpreter_multu(uint32_t instruction)
{
    unsigned int rs, rt;
    uint64_t result;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    result = r3000_read_reg(rs) * r3000_read_reg(rt);

    r3000_write_reg(R3000_REGISTER_HI, result >> 32);
    r3000_write_reg(R3000_REGISTER_LO, result);
}

static void
r3000_interpreter_div(uint32_t instruction)
{
    unsigned int rs, rt;
    int32_t n, d;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    n = r3000_read_reg(rs);
    d = r3000_read_reg(rt);

    if (d == 0) {
        r3000_write_reg(R3000_REGISTER_HI, n);
        r3000_write_reg(R3000_REGISTER_LO, n >= 0 ? -1 : 1);
    } else if (n == INT32_MIN && d == -1) {
        r3000_write_reg(R3000_REGISTER_HI, 0);
        r3000_write_reg(R3000_REGISTER_LO, INT32_MIN);
    } else {
        r3000_write_reg(R3000_REGISTER_HI, n % d);
        r3000_write_reg(R3000_REGISTER_LO, n / d);
    }
}

static void
r3000_interpreter_divu(uint32_t instruction)
{
    unsigned int rs, rt;
    uint32_t n, d;

    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    n = r3000_read_reg(rs);
    d = r3000_read_reg(rt);

    if (d == 0) {
        r3000_write_reg(R3000_REGISTER_HI, n);
        r3000_write_reg(R3000_REGISTER_LO, -1);
    } else {
        r3000_write_reg(R3000_REGISTER_HI, n % d);
        r3000_write_reg(R3000_REGISTER_LO, n / d);
    }
}

static void
r3000_interpreter_add(uint32_t instruction)
{
    unsigned int rd, rs, rt;
    uint32_t s, t, result;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    s = r3000_read_reg(rs);
    t = r3000_read_reg(rt);

    result = s + t;

    if (overflow32(s, t, result)) {
        r3000_exception(R3000_EXCEPTION_OVERFLOW);
    }

    r3000_write_reg(rd, result);
}

static void
r3000_interpreter_addu(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) + r3000_read_reg(rt));
}

static void
r3000_interpreter_sub(uint32_t instruction)
{
    unsigned int rd, rs, rt;
    uint32_t s, t, result;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    s = r3000_read_reg(rs);
    t = r3000_read_reg(rt);

    result = s - t;

    if (overflow32(s, t, result)) {
        r3000_exception(R3000_EXCEPTION_OVERFLOW);
    }

    r3000_write_reg(rd, result);
}

static void
r3000_interpreter_subu(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) - r3000_read_reg(rt));
}

static void
r3000_interpreter_and(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) & r3000_read_reg(rt));
}

static void
r3000_interpreter_or(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) | r3000_read_reg(rt));
}

static void
r3000_interpreter_xor(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) ^ r3000_read_reg(rt));
}

static void
r3000_interpreter_nor(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, ~(r3000_read_reg(rs) | r3000_read_reg(rt)));
}

static void
r3000_interpreter_slt(uint32_t instruction)
{
    unsigned int rd, rs, rt;
    uint32_t result;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    result = (int32_t)r3000_read_reg(rs) < (int32_t)r3000_read_reg(rt);

    r3000_write_reg(rd, result);
}

static void
r3000_interpreter_sltu(uint32_t instruction)
{
    unsigned int rd, rs, rt;

    rd = R3000_RD(instruction);
    rs = R3000_RS(instruction);
    rt = R3000_RT(instruction);

    r3000_write_reg(rd, r3000_read_reg(rs) < r3000_read_reg(rt));
}

static void
r3000_interpreter_special(uint32_t instruction)
{
    switch (R3000_FUNC(instruction)) {
    case 0x00:
        r3000_interpreter_sll(instruction);
        break;
    case 0x02:
        r3000_interpreter_srl(instruction);
        break;
    case 0x03:
        r3000_interpreter_sra(instruction);
        break;
    case 0x04:
        r3000_interpreter_sllv(instruction);
        break;
    case 0x06:
        r3000_interpreter_srlv(instruction);
        break;
    case 0x07:
        r3000_interpreter_srav(instruction);
        break;
    case 0x08:
        r3000_interpreter_jr(instruction);
        break;
    case 0x09:
        r3000_interpreter_jalr(instruction);
        break;
    case 0x0c:
        r3000_interpreter_syscall(instruction);
        break;
    case 0x0d:
        r3000_interpreter_break(instruction);
        break;
    case 0x10:
        r3000_interpreter_mfhi(instruction);
        break;
    case 0x11:
        r3000_interpreter_mthi(instruction);
        break;
    case 0x12:
        r3000_interpreter_mflo(instruction);
        break;
    case 0x13:
        r3000_interpreter_mtlo(instruction);
        break;
    case 0x18:
        r3000_interpreter_mult(instruction);
        break;
    case 0x19:
        r3000_interpreter_multu(instruction);
        break;
    case 0x1a:
        r3000_interpreter_div(instruction);
        break;
    case 0x1b:
        r3000_interpreter_divu(instruction);
        break;
    case 0x20:
        r3000_interpreter_add(instruction);
        break;
    case 0x21:
        r3000_interpreter_addu(instruction);
        break;
    case 0x22:
        r3000_interpreter_sub(instruction);
        break;
    case 0x23:
        r3000_interpreter_subu(instruction);
        break;
    case 0x24:
        r3000_interpreter_and(instruction);
        break;
    case 0x25:
        r3000_interpreter_or(instruction);
        break;
    case 0x26:
        r3000_interpreter_xor(instruction);
        break;
    case 0x27:
        r3000_interpreter_nor(instruction);
        break;
    case 0x2a:
        r3000_interpreter_slt(instruction);
        break;
    case 0x2b:
        r3000_interpreter_sltu(instruction);
        break;
    default:
        printf("r3000_interpreter: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

static void
r3000_interpreter_mfc0(uint32_t instruction)
{
    unsigned int rt, rd;

    rt = R3000_RT(instruction);
    rd = R3000_RD(instruction);

    r3000_write_reg(rt, r3000_cop0_read(rd));
}

static void
r3000_interpreter_mtc0(uint32_t instruction)
{
    unsigned int rt, rd;

    rt = R3000_RT(instruction);
    rd = R3000_RD(instruction);

    r3000_cop0_write(rd, r3000_read_reg(rt));
}

static void
r3000_interpreter_rfe(uint32_t instruction)
{
    (void)instruction;

    r3000_exit_exception();
}

static void
r3000_interpreter_cop0(uint32_t instruction)
{
    switch (R3000_RS(instruction)) {
    case 0x00:
        r3000_interpreter_mfc0(instruction);
        break;
    case 0x04:
        r3000_interpreter_mtc0(instruction);
        break;
    case 0x10:
        r3000_interpreter_rfe(instruction);
        break;
    default:
        printf("r3000_interpreter: error: unknown cop0 instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

void r3000_interpreter_execute(void)
{
    uint32_t pc;
    uint32_t instruction;

    //char disasm_buf[64];

    pc = r3000_read_pc();
    instruction = r3000_read_code();

    //r3000_disassembler_disassemble(disasm_buf, sizeof(disasm_buf),
    //                               instruction, pc);

    //printf("0x%08x: %s\n", pc, disasm_buf);

    if (instruction == 0) {
        return;
    }

    switch (R3000_OPCODE(instruction)) {
    case 0x00:
        r3000_interpreter_special(instruction);
        break;
    case 0x01:
        r3000_interpreter_bcond(instruction);
        break;
    case 0x02:
        r3000_interpreter_j(instruction, pc);
        break;
    case 0x03:
        r3000_interpreter_jal(instruction, pc);
        break;
    case 0x04:
        r3000_interpreter_beq(instruction);
        break;
    case 0x05:
        r3000_interpreter_bne(instruction);
        break;
    case 0x06:
        r3000_interpreter_blez(instruction);
        break;
    case 0x07:
        r3000_interpreter_bgtz(instruction);
        break;
    case 0x08:
        r3000_interpreter_addi(instruction);
        break;
    case 0x09:
        r3000_interpreter_addiu(instruction);
        break;
    case 0x0a:
        r3000_interpreter_slti(instruction);
        break;
    case 0x0b:
        r3000_interpreter_sltiu(instruction);
        break;
    case 0x0c:
        r3000_interpreter_andi(instruction);
        break;
    case 0x0d:
        r3000_interpreter_ori(instruction);
        break;
    case 0x0e:
        r3000_interpreter_xori(instruction);
        break;
    case 0x0f:
        r3000_interpreter_lui(instruction);
        break;
    case 0x10:
        r3000_interpreter_cop0(instruction);
        break;
    case 0x20:
        r3000_interpreter_lb(instruction);
        break;
    case 0x21:
        r3000_interpreter_lh(instruction);
        break;
    case 0x22:
        r3000_interpreter_lwl(instruction);
        break;
    case 0x23:
        r3000_interpreter_lw(instruction);
        break;
    case 0x24:
        r3000_interpreter_lbu(instruction);
        break;
    case 0x25:
        r3000_interpreter_lhu(instruction);
        break;
    case 0x26:
        r3000_interpreter_lwr(instruction);
        break;
    case 0x28:
        r3000_interpreter_sb(instruction);
        break;
    case 0x29:
        r3000_interpreter_sh(instruction);
        break;
    case 0x2a:
        r3000_interpreter_swl(instruction);
        break;
    case 0x2b:
        r3000_interpreter_sw(instruction);
        break;
    case 0x2e:
        r3000_interpreter_swr(instruction);
        break;
    default:
        printf("r3000_interpreter: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    };
}

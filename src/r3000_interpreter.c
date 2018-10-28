#include <stdint.h>
#include <stdio.h>

#include "macros.h"
#include "r3000.h"
#include "r3000_disassembler.h"
#include "r3000_interpreter.h"
#include "util.h"

static void
r3000_interpreter_j(uint32_t instruction, uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    /* TODO: Replace with next_pc */
    r3000_jump((address & 0xf0000000) | (target << 2));
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
        PANIC;
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
r3000_interpreter_lui(uint32_t instruction)
{
    unsigned int rt;
    uint32_t imm;

    rt = R3000_RT(instruction);
    imm = R3000_IMM(instruction);

    r3000_write_reg(rt, imm << 16);
}

static void
r3000_interpreter_lw(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    r3000_write_reg(rt, r3000_read_memory32(r3000_read_reg(rs) + offset));
}

static void
r3000_interpreter_sw(uint32_t instruction)
{
    unsigned int rt, rs;
    uint32_t offset;

    rt = R3000_RT(instruction);
    rs = R3000_RS(instruction);
    offset = R3000_IMM_SE(instruction);

    r3000_write_memory32(r3000_read_reg(rs) + offset, r3000_read_reg(rt));
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
r3000_interpreter_special(uint32_t instruction)
{
    switch (R3000_FUNC(instruction)) {
    case 0x25:
        r3000_interpreter_or(instruction);
        break;
    default:
        printf("r3000_interpreter: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
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
r3000_interpreter_cop0(uint32_t instruction)
{
    switch (R3000_RS(instruction)) {
    case 0x04:
        r3000_interpreter_mtc0(instruction);
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

    pc = r3000_read_pc(); 
    instruction = r3000_read_code();

    r3000_disassembler_disassemble(instruction, pc);

    if (instruction == 0) {
        return;
    }

    switch(R3000_OPCODE(instruction)) {
    case 0x00:
        r3000_interpreter_special(instruction);
        break;
    case 0x02:
        r3000_interpreter_j(instruction, pc);
        break;
    case 0x05:
        r3000_interpreter_bne(instruction);
        break;
    case 0x08:
        r3000_interpreter_addi(instruction);
        break;
    case 0x09:
        r3000_interpreter_addiu(instruction);
        break;
    case 0x0d:
        r3000_interpreter_ori(instruction);
        break;
    case 0x0f:
        r3000_interpreter_lui(instruction);
        break;
    case 0x10:
        r3000_interpreter_cop0(instruction);
        break;
    case 0x23:
        r3000_interpreter_lw(instruction);
        break;
    case 0x2b:
        r3000_interpreter_sw(instruction);
        break;
    default:
        printf("r3000_interpreter: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    };
}

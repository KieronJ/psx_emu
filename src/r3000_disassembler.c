#include <stdint.h>
#include <stdio.h>

#include "macros.h"
#include "r3000.h"
#include "r3000_disassembler.h"

static void
r3000_disassembler_j(uint32_t instruction, uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    printf("J 0x%08x\n", (address & 0xf0000000) | (target << 2));
}

static void
r3000_disassembler_addiu(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm, imm_abs;
    char *imm_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    imm_sign = HEX_SIGN_32(imm);
    imm_abs = HEX_ABS_32(imm);

    printf("ADDIU %s, %s, %s0x%x\n", rt, rs, imm_sign, imm_abs);
}

static void
r3000_disassembler_ori(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM(instruction);

    printf("ORI %s, %s, 0x%04x\n", rt, rs, imm);
}

static void
r3000_disassembler_lui(uint32_t instruction)
{
    const char *rt;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    imm = R3000_IMM(instruction);

    printf("LUI %s, 0x%04x\n", rt, imm);
}

static void
r3000_disassembler_sw(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    printf("SW %s, %s0x%x(%s)\n", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_or(uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    printf("OR %s, %s, %s\n", rd, rs, rt);
}

static void
r3000_disassembler_special(uint32_t instruction)
{
    switch (R3000_FUNC(instruction)) {
    case 0x25:
        r3000_disassembler_or(instruction);
        break;
    default:
        printf("r3000_disassembler: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

static void
r3000_disassembler_mtc0(uint32_t instruction)
{
    const char *rt, *rd;

    rt = r3000_register_name(R3000_RT(instruction));
    rd = r3000_cop0_register_name(R3000_RD(instruction));

    printf("MTC0 %s, %s\n", rt, rd);
}

static void
r3000_disassembler_cop0(uint32_t instruction)
{
    switch (R3000_RS(instruction)) {
    case 0x04:
        r3000_disassembler_mtc0(instruction);
        break;
    default:
        printf("r3000_disassembler: error: unknown cop0 instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

void
r3000_disassembler_disassemble(uint32_t instruction, uint32_t address)
{
    printf("0x%08x:\t", address);

    if (instruction == 0) {
        printf("NOP\n");
        return;
    } 

    switch (R3000_OPCODE(instruction)) {
    case 0x00:
        r3000_disassembler_special(instruction);
        break;
    case 0x02:
        r3000_disassembler_j(instruction, address);
        break;
    case 0x09:
        r3000_disassembler_addiu(instruction);
        break;
    case 0x0d:
        r3000_disassembler_ori(instruction);
        break;
    case 0x0f:
        r3000_disassembler_lui(instruction);
        break;
    case 0x10:
        r3000_disassembler_cop0(instruction);
        break;
    case 0x2b:
        r3000_disassembler_sw(instruction);
        break;
    default:
        printf("r3000_disassembler: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

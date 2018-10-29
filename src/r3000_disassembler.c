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
r3000_disassembler_jal(uint32_t instruction, uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    printf("JAL 0x%08x\n", (address & 0xf0000000) | (target << 2));
}

static void
r3000_disassembler_beq(uint32_t instruction, uint32_t address)
{
    const char *rs, *rt;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    offset = R3000_IMM_SE(instruction);

    printf("BEQ %s, %s, 0x%08x\n", rs, rt, address + (offset << 2) + 4);
}

static void
r3000_disassembler_bne(uint32_t instruction, uint32_t address)
{
    const char *rs, *rt;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    offset = R3000_IMM_SE(instruction);

    printf("BNE %s, %s, 0x%08x\n", rs, rt, address + (offset << 2) + 4);
}

static void
r3000_disassembler_bgtz(uint32_t instruction, uint32_t address)
{
    const char *rs;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    printf("BGTZ %s, 0x%08x\n", rs, address + (offset << 2) + 4);
}

static void
r3000_disassembler_addi(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm, imm_abs;
    char *imm_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    imm_sign = HEX_SIGN_32(imm);
    imm_abs = HEX_ABS_32(imm);

    printf("ADDI %s, %s, %s0x%x\n", rt, rs, imm_sign, imm_abs);
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
r3000_disassembler_andi(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM(instruction);

    printf("ANDI %s, %s, 0x%04x\n", rt, rs, imm);
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
r3000_disassembler_lb(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    printf("LB %s, %s0x%x(%s)\n", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_lw(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    printf("LW %s, %s0x%x(%s)\n", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sb(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    printf("SB %s, %s0x%x(%s)\n", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sh(uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    printf("SH %s, %s0x%x(%s)\n", rt, offset_sign, offset_abs, rs);
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
r3000_disassembler_sll(uint32_t instruction)
{
    const char *rd, *rt;
    unsigned int shift;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    shift = R3000_SHIFT(instruction);

    printf("SLL %s, %s, %d\n", rd, rt, shift);
}

static void
r3000_disassembler_jr(uint32_t instruction)
{
    const char *rs;

    rs = r3000_register_name(R3000_RS(instruction));

    printf("JR %s\n", rs);
}

static void
r3000_disassembler_add(uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    printf("ADD %s, %s, %s\n", rd, rs, rt);
}

static void
r3000_disassembler_addu(uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    printf("ADDU %s, %s, %s\n", rd, rs, rt);
}

static void
r3000_disassembler_and(uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    printf("AND %s, %s, %s\n", rd, rs, rt);
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
r3000_disassembler_sltu(uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    printf("SLTU %s, %s, %s\n", rd, rs, rt);
}

static void
r3000_disassembler_special(uint32_t instruction)
{
    switch (R3000_FUNC(instruction)) {
    case 0x00:
        r3000_disassembler_sll(instruction);
        break;
    case 0x08:
        r3000_disassembler_jr(instruction);
        break;
    case 0x20:
        r3000_disassembler_add(instruction);
        break;
    case 0x21:
        r3000_disassembler_addu(instruction);
        break;
    case 0x24:
        r3000_disassembler_and(instruction);
        break;
    case 0x25:
        r3000_disassembler_or(instruction);
        break;
    case 0x2b:
        r3000_disassembler_sltu(instruction);
        break;
    default:
        printf("r3000_disassembler: error: unknown instruction 0x%08x\n",
               instruction);
        PANIC;
        break;
    }
}

static void
r3000_disassembler_mfc0(uint32_t instruction)
{
    const char *rt, *rd;

    rt = r3000_register_name(R3000_RT(instruction));
    rd = r3000_cop0_register_name(R3000_RD(instruction));

    printf("MFC0 %s, %s\n", rt, rd);
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
    case 0x00:
        r3000_disassembler_mfc0(instruction);
        break;
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
    case 0x03:
        r3000_disassembler_jal(instruction, address);
        break;
    case 0x04:
        r3000_disassembler_beq(instruction, address);
        break;
    case 0x05:
        r3000_disassembler_bne(instruction, address);
        break;
    case 0x07:
        r3000_disassembler_bgtz(instruction, address);
        break;
    case 0x08:
        r3000_disassembler_addi(instruction);
        break;
    case 0x09:
        r3000_disassembler_addiu(instruction);
        break;
    case 0x0c:
        r3000_disassembler_andi(instruction);
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
    case 0x20:
        r3000_disassembler_lb(instruction);
        break;
    case 0x23:
        r3000_disassembler_lw(instruction);
        break;
    case 0x28:
        r3000_disassembler_sb(instruction);
        break;
    case 0x29:
        r3000_disassembler_sh(instruction);
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

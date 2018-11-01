#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "macros.h"
#include "r3000.h"
#include "r3000_disassembler.h"

static void
r3000_disassembler_j(char *buf, size_t n, uint32_t instruction,
                     uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    snprintf(buf, n, "J 0x%08x", (address & 0xf0000000) | (target << 2));
}

static void
r3000_disassembler_bcond(char *buf, size_t n, uint32_t instruction,
                         uint32_t address)
{
    const char *rs;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    switch(R3000_RT(instruction) & 0x11) {
    case 0x00:
        snprintf(buf, n, "BLTZ %s, 0x%08x",
                 rs, address + (offset << 2) + 4);
        break;
    case 0x01:
        snprintf(buf, n, "BGEZ %s, 0x%08x",
                 rs, address + (offset << 2) + 4);
        break;
    case 0x10:
        snprintf(buf, n, "BLTZAL %s, 0x%08x",
                 rs, address + (offset << 2) + 4);
        break;
    case 0x11:
        snprintf(buf, n, "BGEZAL %s, 0x%08x",
                 rs, address + (offset << 2) + 4);
        break;
    default:
        snprintf(buf, n, "UNKNOWN");
        break;
    }
}

static void
r3000_disassembler_jal(char *buf, size_t n, uint32_t instruction,
					   uint32_t address)
{
    uint32_t target;

    target = R3000_TARGET(instruction);

    snprintf(buf, n, "JAL 0x%08x", (address & 0xf0000000) | (target << 2));
}

static void
r3000_disassembler_beq(char *buf, size_t n, uint32_t instruction,
                       uint32_t address)
{
    const char *rs, *rt;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    offset = R3000_IMM_SE(instruction);

    snprintf(buf, n, "BEQ %s, %s, 0x%08x",
             rs, rt, address + (offset << 2) + 4);
}

static void
r3000_disassembler_bne(char *buf, size_t n, uint32_t instruction,
					   uint32_t address)
{
    const char *rs, *rt;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    offset = R3000_IMM_SE(instruction);

    snprintf(buf, n, "BNE %s, %s, 0x%08x",
             rs, rt, address + (offset << 2) + 4);
}

static void
r3000_disassembler_blez(char *buf, size_t n, uint32_t instruction,
						uint32_t address)
{
    const char *rs;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    snprintf(buf, n, "BLEZ %s, 0x%08x", rs, address + (offset << 2) + 4);
}

static void
r3000_disassembler_bgtz(char *buf, size_t n, uint32_t instruction,
						uint32_t address)
{
    const char *rs;
    uint32_t offset;

    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    snprintf(buf, n, "BGTZ %s, 0x%08x", rs, address + (offset << 2) + 4);
}

static void
r3000_disassembler_addi(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm, imm_abs;
    char *imm_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    imm_sign = HEX_SIGN_32(imm);
    imm_abs = HEX_ABS_32(imm);

    snprintf(buf, n, "ADDI %s, %s, %s0x%x", rt, rs, imm_sign, imm_abs);
}

static void
r3000_disassembler_addiu(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm, imm_abs;
    char *imm_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    imm_sign = HEX_SIGN_32(imm);
    imm_abs = HEX_ABS_32(imm);

    snprintf(buf, n, "ADDIU %s, %s, %s0x%x", rt, rs, imm_sign, imm_abs);
}

static void
r3000_disassembler_slti(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm, imm_abs;
    char *imm_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    imm_sign = HEX_SIGN_32(imm);
    imm_abs = HEX_ABS_32(imm);

    snprintf(buf, n, "SLTI %s, %s, %s0x%x", rt, rs, imm_sign, imm_abs);
}

static void
r3000_disassembler_sltiu(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM_SE(instruction);

    snprintf(buf, n, "SLTIU %s, %s, 0x08%x", rt, rs, imm);
}

static void
r3000_disassembler_andi(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM(instruction);

    snprintf(buf, n, "ANDI %s, %s, 0x%04x", rt, rs, imm);
}

static void
r3000_disassembler_ori(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    imm = R3000_IMM(instruction);

    snprintf(buf, n, "ORI %s, %s, 0x%04x", rt, rs, imm);
}

static void
r3000_disassembler_lui(char *buf, size_t n, uint32_t instruction)
{
    const char *rt;
    uint32_t imm;

    rt = r3000_register_name(R3000_RT(instruction));
    imm = R3000_IMM(instruction);

    snprintf(buf, n, "LUI %s, 0x%04x", rt, imm);
}

static void
r3000_disassembler_lb(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "LB %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_lw(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "LW %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_lbu(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "LBU %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_lhu(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "LHU %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sb(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "SB %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sh(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "SH %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sw(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rs;
    uint32_t offset, offset_abs;
    char *offset_sign;

    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    offset = R3000_IMM_SE(instruction);

    offset_sign = HEX_SIGN_32(offset);
    offset_abs = HEX_ABS_32(offset);

    snprintf(buf, n, "SW %s, %s0x%x(%s)", rt, offset_sign, offset_abs, rs);
}

static void
r3000_disassembler_sll(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt;
    unsigned int shift;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    shift = R3000_SHIFT(instruction);

    snprintf(buf, n, "SLL %s, %s, %d", rd, rt, shift);
}

static void
r3000_disassembler_srl(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt;
    unsigned int shift;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    shift = R3000_SHIFT(instruction);

    snprintf(buf, n, "SRL %s, %s, %d", rd, rt, shift);
}

static void
r3000_disassembler_sra(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt;
    unsigned int shift;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    shift = R3000_SHIFT(instruction);

    snprintf(buf, n, "SRA %s, %s, %d", rd, rt, shift);
}

static void
r3000_disassembler_sllv(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt, *rs;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "SLLV %s, %s, %s", rd, rt, rs);
}

static void
r3000_disassembler_srlv(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt, *rs;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "SRLV %s, %s, %s", rd, rt, rs);
}

static void
r3000_disassembler_srav(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rt, *rs;

    rd = r3000_register_name(R3000_RD(instruction));
    rt = r3000_register_name(R3000_RT(instruction));
    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "SRAV %s, %s, %s", rd, rt, rs);
}

static void
r3000_disassembler_jr(char *buf, size_t n, uint32_t instruction)
{
    const char *rs;

    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "JR %s", rs);
}

static void
r3000_disassembler_jalr(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "JALR %s, %s", rd, rs);
}

static void
r3000_disassembler_syscall(char *buf, size_t n, uint32_t instruction)
{
    (void)instruction;

    snprintf(buf, n, "SYSCALL");
}

static void
r3000_disassembler_break(char *buf, size_t n, uint32_t instruction)
{
    (void)instruction;

    snprintf(buf, n, "BREAK");
}

static void
r3000_disassembler_mfhi(char *buf, size_t n, uint32_t instruction)
{
    const char *rd;

    rd = r3000_register_name(R3000_RD(instruction));

    snprintf(buf, n, "MFHI %s", rd);
}

static void
r3000_disassembler_mthi(char *buf, size_t n, uint32_t instruction)
{
    const char *rs;

    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "MTHI %s", rs);
}

static void
r3000_disassembler_mflo(char *buf, size_t n, uint32_t instruction)
{
    const char *rd;

    rd = r3000_register_name(R3000_RD(instruction));

    snprintf(buf, n, "MFLO %s", rd);
}

static void
r3000_disassembler_mtlo(char *buf, size_t n, uint32_t instruction)
{
    const char *rs;

    rs = r3000_register_name(R3000_RS(instruction));

    snprintf(buf, n, "MTLO %s", rs);
}

static void
r3000_disassembler_mult(char *buf, size_t n, uint32_t instruction)
{
    const char *rs, *rt;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "MULT %s, %s", rs, rt);
}

static void
r3000_disassembler_multu(char *buf, size_t n, uint32_t instruction)
{
    const char *rs, *rt;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "MULTU %s, %s", rs, rt);
}


static void
r3000_disassembler_div(char *buf, size_t n, uint32_t instruction)
{
    const char *rs, *rt;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "DIV %s, %s", rs, rt);
}

static void
r3000_disassembler_divu(char *buf, size_t n, uint32_t instruction)
{
    const char *rs, *rt;

    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "DIVU %s, %s", rs, rt);
}

static void
r3000_disassembler_add(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "ADD %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_addu(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "ADDU %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_sub(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "SUB %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_subu(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "SUBU %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_and(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "AND %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_or(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "OR %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_xor(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "XOR %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_nor(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "NOR %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_slt(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "SLT %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_sltu(char *buf, size_t n, uint32_t instruction)
{
    const char *rd, *rs, *rt;

    rd = r3000_register_name(R3000_RD(instruction));
    rs = r3000_register_name(R3000_RS(instruction));
    rt = r3000_register_name(R3000_RT(instruction));

    snprintf(buf, n, "SLTU %s, %s, %s", rd, rs, rt);
}

static void
r3000_disassembler_special(char *buf, size_t n, uint32_t instruction)
{
    switch (R3000_FUNC(instruction)) {
    case 0x00:
        r3000_disassembler_sll(buf, n, instruction);
        break;
    case 0x02:
        r3000_disassembler_srl(buf, n, instruction);
        break;
    case 0x03:
        r3000_disassembler_sra(buf, n, instruction);
        break;
    case 0x04:
        r3000_disassembler_sllv(buf, n, instruction);
        break;
    case 0x06:
        r3000_disassembler_srlv(buf, n, instruction);
        break;
    case 0x07:
        r3000_disassembler_srav(buf, n, instruction);
        break;
    case 0x08:
        r3000_disassembler_jr(buf, n, instruction);
        break;
    case 0x09:
        r3000_disassembler_jalr(buf, n, instruction);
        break;
    case 0x0c:
        r3000_disassembler_syscall(buf, n, instruction);
        break;
    case 0x0d:
        r3000_disassembler_break(buf, n, instruction);
        break;
    case 0x10:
        r3000_disassembler_mfhi(buf, n, instruction);
        break;
    case 0x11:
        r3000_disassembler_mthi(buf, n, instruction);
        break;
    case 0x12:
        r3000_disassembler_mflo(buf, n, instruction);
        break;
    case 0x13:
        r3000_disassembler_mtlo(buf, n, instruction);
        break;
    case 0x18:
        r3000_disassembler_mult(buf, n, instruction);
        break;
    case 0x19:
        r3000_disassembler_multu(buf, n, instruction);
        break;
    case 0x1a:
        r3000_disassembler_div(buf, n, instruction);
        break;
    case 0x1b:
        r3000_disassembler_divu(buf, n, instruction);
        break;
    case 0x20:
        r3000_disassembler_add(buf, n, instruction);
        break;
    case 0x21:
        r3000_disassembler_addu(buf, n, instruction);
        break;
    case 0x22:
        r3000_disassembler_sub(buf, n, instruction);
        break;
    case 0x23:
        r3000_disassembler_subu(buf, n, instruction);
        break;
    case 0x24:
        r3000_disassembler_and(buf, n, instruction);
        break;
    case 0x25:
        r3000_disassembler_or(buf, n, instruction);
        break;
    case 0x26:
        r3000_disassembler_xor(buf, n, instruction);
        break;
    case 0x27:
        r3000_disassembler_nor(buf, n, instruction);
        break;
    case 0x2a:
        r3000_disassembler_slt(buf, n, instruction);
        break;
    case 0x2b:
        r3000_disassembler_sltu(buf, n, instruction);
        break;
    default:
        snprintf(buf, n, "UNKNOWN");
        break;
    }
}

static void
r3000_disassembler_mfc0(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rd;

    rt = r3000_register_name(R3000_RT(instruction));
    rd = r3000_cop0_register_name(R3000_RD(instruction));

    snprintf(buf, n, "MFC0 %s, %s", rt, rd);
}

static void
r3000_disassembler_mtc0(char *buf, size_t n, uint32_t instruction)
{
    const char *rt, *rd;

    rt = r3000_register_name(R3000_RT(instruction));
    rd = r3000_cop0_register_name(R3000_RD(instruction));

    snprintf(buf, n, "MTC0 %s, %s", rt, rd);
}

static void
r3000_disassembler_rfe(char *buf, size_t n, uint32_t instruction)
{
    (void)instruction;

    snprintf(buf, n, "RFE");
}

static void
r3000_disassembler_cop0(char *buf, size_t n, uint32_t instruction)
{
    switch (R3000_RS(instruction)) {
    case 0x00:
        r3000_disassembler_mfc0(buf, n, instruction);
        break;
    case 0x04:
        r3000_disassembler_mtc0(buf, n, instruction);
        break;
    case 0x10:
        r3000_disassembler_rfe(buf, n, instruction);
        break;
    default:
        snprintf(buf, n, "UNKNOWN");
        break;
    }
}

void
r3000_disassembler_disassemble(char *buf, size_t n, uint32_t instruction,
                               uint32_t address)
{
    if (instruction == 0) {
        snprintf(buf, n, "NOP");
        return;
    }

    switch (R3000_OPCODE(instruction)) {
    case 0x00:
        r3000_disassembler_special(buf, n, instruction);
        break;
    case 0x01:
        r3000_disassembler_bcond(buf, n, instruction, address);
        break;
    case 0x02:
        r3000_disassembler_j(buf, n, instruction, address);
        break;
    case 0x03:
        r3000_disassembler_jal(buf, n, instruction, address);
        break;
    case 0x04:
        r3000_disassembler_beq(buf, n, instruction, address);
        break;
    case 0x05:
        r3000_disassembler_bne(buf, n, instruction, address);
        break;
    case 0x06:
        r3000_disassembler_blez(buf, n, instruction, address);
        break;
    case 0x07:
        r3000_disassembler_bgtz(buf, n, instruction, address);
        break;
    case 0x08:
        r3000_disassembler_addi(buf, n, instruction);
        break;
    case 0x09:
        r3000_disassembler_addiu(buf, n, instruction);
        break;
    case 0x0a:
        r3000_disassembler_slti(buf, n, instruction);
        break;
    case 0x0b:
        r3000_disassembler_sltiu(buf, n, instruction);
        break;
    case 0x0c:
        r3000_disassembler_andi(buf, n, instruction);
        break;
    case 0x0d:
        r3000_disassembler_ori(buf, n, instruction);
        break;
    case 0x0f:
        r3000_disassembler_lui(buf, n, instruction);
        break;
    case 0x10:
        r3000_disassembler_cop0(buf, n, instruction);
        break;
    case 0x20:
        r3000_disassembler_lb(buf, n, instruction);
        break;
    case 0x23:
        r3000_disassembler_lw(buf, n, instruction);
        break;
    case 0x24:
        r3000_disassembler_lbu(buf, n, instruction);
        break;
    case 0x25:
        r3000_disassembler_lhu(buf, n, instruction);
        break;
    case 0x28:
        r3000_disassembler_sb(buf, n, instruction);
        break;
    case 0x29:
        r3000_disassembler_sh(buf, n, instruction);
        break;
    case 0x2b:
        r3000_disassembler_sw(buf, n, instruction);
        break;
    default:
        snprintf(buf, n, "UNKNOWN");
        break;
    }
}

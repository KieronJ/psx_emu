#ifndef R3000_H
#define R3000_H

#include <stdint.h>

#define R3000_OPCODE(x) ((x) >> 26)
#define R3000_RS(x)     (((x) >> 21) & 0x1f)
#define R3000_RT(x)     (((x) >> 16) & 0x1f)
#define R3000_RD(x)     (((x) >> 11) & 0x1f)
#define R3000_SHIFT(x)  (((x) >> 6) & 0x1f)
#define R3000_FUNC(x)   ((x) & 0x3f)
#define R3000_IMM(x)    ((x) & 0xffff)
#define R3000_IMM_SE(x) ((uint32_t)(int16_t)(x))
#define R3000_TARGET(x) ((x) & 0x3ffffff)

void r3000_setup(void);
void r3000_reset(void);

const char * r3000_register_name(unsigned int reg);
const char * r3000_cop0_register_name(unsigned int reg);

uint32_t r3000_read_pc(void);
uint32_t r3000_read_next_pc(void);
void r3000_jump(uint32_t address);
void r3000_branch(uint32_t offset);

uint32_t r3000_read_reg(unsigned int reg);
void r3000_write_reg(unsigned int reg, uint32_t value);

uint32_t r3000_cop0_read(unsigned int reg);
void r3000_cop0_write(unsigned int reg, uint32_t value);

uint32_t r3000_read_code(void);
uint8_t r3000_read_memory8(uint32_t address);
uint32_t r3000_read_memory32(uint32_t address);
void r3000_write_memory8(uint32_t address, uint8_t value);
void r3000_write_memory16(uint32_t address, uint16_t value);
void r3000_write_memory32(uint32_t address, uint32_t value);

#endif /* R3000_H */

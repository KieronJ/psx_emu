#ifndef R3000_DISASSEMBLER_H
#define R3000_DISASSEMBLER_H

#include <stdint.h>

void r3000_disassembler_disassemble(uint32_t instruction, uint32_t address);

#endif /* R3000_DISASSEMBLER_H */

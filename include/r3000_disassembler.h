#ifndef R3000_DISASSEMBLER_H
#define R3000_DISASSEMBLER_H

#include <stddef.h>
#include <stdint.h>

/* TODO: Refactor and clean up code */
void r3000_disassembler_disassemble(char *buf, size_t n, uint32_t instruction,
                                    uint32_t address);

#endif /* R3000_DISASSEMBLER_H */

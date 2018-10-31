#ifndef EXP2_H
#define EXP2_H

#include <stdint.h>

void exp2_setup(void);

uint8_t exp2_read8(uint32_t address);
void exp2_write8(uint32_t address, uint8_t value);

#endif /* EXP2_H */

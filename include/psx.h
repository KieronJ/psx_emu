#ifndef PSX_H
#define PSX_H

#include <stdint.h>

void psx_setup(const char *bios_path);
void psx_step(void);

uint8_t psx_read_memory8(uint32_t address);
uint32_t psx_read_memory32(uint32_t address);
void psx_write_memory8(uint32_t address, uint8_t value);
void psx_write_memory16(uint32_t address, uint16_t value);
void psx_write_memory32(uint32_t address, uint32_t value);

#endif /* PSX_H */

#ifndef PSX_H
#define PSX_H

#include <stdint.h>

void psx_setup(const char *bios_path);
void psx_step(void);

uint32_t psx_read_memory32(uint32_t address);
void psx_write_memory32(uint32_t address, uint32_t value);

#endif /* PSX_H */

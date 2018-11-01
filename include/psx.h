#ifndef PSX_H
#define PSX_H

#include <stdint.h>

#include "macros.h"

#define PSX_RAM_SIZE    MEGABYTES(2)
#define PSX_BIOS_SIZE   KILOBYTES(512)

void psx_setup(const char *bios_path);
void psx_shutdown(void);
void psx_soft_reset(void);
void psx_hard_reset(void);

void psx_step(void);
void psx_run_frame(void);

uint8_t psx_read_memory8(uint32_t address);
uint16_t psx_read_memory16(uint32_t address);
uint32_t psx_read_memory32(uint32_t address);
void psx_write_memory8(uint32_t address, uint8_t value);
void psx_write_memory16(uint32_t address, uint16_t value);
void psx_write_memory32(uint32_t address, uint32_t value);

uint8_t psx_debug_read_memory8(uint32_t address);
uint32_t psx_debug_read_memory32(uint32_t address);
void psx_debug_write_memory32(uint32_t address, uint32_t value);

uint8_t * psx_debug_ram(void);
uint8_t * psx_debug_bios(void);

#endif /* PSX_H */

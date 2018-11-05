#ifndef PSX_H
#define PSX_H

#include <stdint.h>

#include "macros.h"

#define PSX_RAM_SIZE    MEGABYTES(2)
#define PSX_BIOS_SIZE   KILOBYTES(512)

#define PSX_INTERRUPT_STATUS    0x1f801070
#define PSX_INTERRUPT_MASK      0x1f801074

enum psx_interrupt {
    PSX_INTERRUPT_VBLANK = 0x1,
    PSX_INTERRUPT_GPU = 0x2,
    PSX_INTERRUPT_CDROM = 0x4,
    PSX_INTERRUPT_DMA = 0x8,
    PSX_INTERRUPT_TMR0 = 0x10,
    PSX_INTERRUPT_TMR1 = 0x20,
    PSX_INTERRUPT_TMR2 = 0x40,
    PSX_INTERRUPT_INP = 0x80,
    PSX_INTERRUPT_SIO = 0x100,
    PSX_INTERRUPT_SPU = 0x200,
    PSX_INTERRUPT_PIO = 0x400,
};

void psx_setup(const char *bios_path);
void psx_shutdown(void);
void psx_soft_reset(void);
void psx_hard_reset(void);

void psx_step(void);
void psx_run_frame(void);

void psx_assert_irq(enum psx_interrupt i);

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

#ifndef SPU_H
#define SPU_H

#include <stdint.h>

#define SPU_NR_VOICES   24

#define SPU_RAM_SIZE    KILOBYTES(512)

void spu_setup(void);
void spu_shutdown(void);
void spu_hard_reset(void);

void spu_step(void);

uint16_t spu_read16(uint32_t address);
void spu_write16(uint32_t address, uint16_t value);

uint8_t * spu_debug_ram(void);

#endif /* SPU_H */

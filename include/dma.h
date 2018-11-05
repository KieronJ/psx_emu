#ifndef DMA_H
#define DMA_H

#include <stdint.h>

void dma_setup(void);
void dma_soft_reset(void);
void dma_hard_reset(void);

uint32_t dma_read32(uint32_t address);
void dma_write32(uint32_t address, uint32_t value);

#endif /* DMA_H */

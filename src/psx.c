#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exp2.h"
#include "macros.h"
#include "psx.h"
#include "r3000.h"
#include "r3000_interpreter.h"
#include "spu.h"
#include "util.h"

#define PSX_FORCE_TTY

#define PSX_REFRESH_RATE        60

#define PSX_EXP1_SIZE           MEGABYTES(8)
#define PSX_MEMCTRL_SIZE        0x24
#define PSX_DMA_SIZE            0x80
#define PSX_TIMER_SIZE          0x30
#define PSX_CDROM_SIZE          0x4
#define PSX_SPU_SIZE            KILOBYTES(1)
#define PSX_EXP2_SIZE           KILOBYTES(8)

#define PSX_RAM_START           0x00000000
#define PSX_RAM_END             PSX_RAM_START + PSX_RAM_SIZE

#define PSX_EXP1_START          0x1f000000
#define PSX_EXP1_END            PSX_EXP1_START + PSX_EXP1_SIZE

#define PSX_MEMCTRL_START       0x1f801000
#define PSX_MEMCTRL_END         PSX_MEMCTRL_START + PSX_MEMCTRL_SIZE

#define PSX_MEMCTRL2            0x1f801060

#define PSX_INTERRUPT_STATUS    0x1f801070
#define PSX_INTERRUPT_MASK      0x1f801074

#define PSX_DMA_START           0x1f801080
#define PSX_DMA_END             PSX_DMA_START + PSX_DMA_SIZE

#define PSX_TIMER_START         0x1f801100
#define PSX_TIMER_END           PSX_TIMER_START + PSX_TIMER_SIZE

#define PSX_CDROM_START         0x1f801800
#define PSX_CDROM_END           PSX_CDROM_START + PSX_CDROM_SIZE

#define PSX_GPUREAD             0x1f801810
#define PSX_GP0                 0x1f801810
#define PSX_GPUSTAT             0x1f801814
#define PSX_GP1                 0x1f801814

#define PSX_SPU_START           0x1f801c00
#define PSX_SPU_END             PSX_SPU_START + PSX_SPU_SIZE

#define PSX_EXP2_START          0x1f802000
#define PSX_EXP2_END            PSX_EXP2_START + PSX_EXP2_SIZE

#define PSX_BIOS_START          0x1fc00000
#define PSX_BIOS_END            PSX_BIOS_START + PSX_BIOS_SIZE

#define PSX_CACHECTRL           0xfffe0130

struct psx {
    void *bios;
    void *ram;

    struct {
        uint32_t status;
        uint32_t mask;
    } interrupt;
};

static struct psx psx;

static void
psx_load_bios(const char *bios_path)
{
    FILE *fp;
    size_t read_size;

    fp = fopen(bios_path, "rb");

    if (!fp) {
        perror("psx: error: unable to load bios");
        PANIC;
    }

    read_size = fread(psx.bios, 1, PSX_BIOS_SIZE, fp);

    if (read_size != PSX_BIOS_SIZE) {
        printf("psx: error: unexpected bios size %I64u bytes\n", read_size);
        PANIC;
    }

    fclose(fp);
}

static void
psx_reset_memory(void)
{
    memset(psx.ram, 0, PSX_RAM_SIZE);

    spu_reset_memory();

    psx.interrupt.status = 0;
    psx.interrupt.mask = 0;
}

void
psx_setup(const char *bios_path)
{
    exp2_setup();
    r3000_setup();
    spu_setup();

    psx.bios = malloc(PSX_BIOS_SIZE);
    psx.ram = malloc(PSX_RAM_SIZE);

    assert(psx.bios);
    assert(psx.ram);

    memset(psx.bios, 0, PSX_BIOS_SIZE);

    psx_reset_memory();
    psx_load_bios(bios_path);

#ifdef PSX_FORCE_TTY /* Patch BIOS to enable TTY output */
    ((uint32_t *)psx.bios)[0x1bc3] = 0x24010001; /* ADDIU $at, $zero, 0x1 */
    ((uint32_t *)psx.bios)[0x1bc5] = 0xaf81a9c0; /* SW $at, -0x5640($gp) */
#endif
}

void
psx_shutdown(void)
{
    assert(psx.bios);
    assert(psx.ram);

    free(psx.bios);
    free(psx.ram);

    spu_shutdown();
}

void
psx_soft_reset(void)
{
    r3000_soft_reset();
}

void
psx_hard_reset(void)
{
    r3000_hard_reset();
    psx_reset_memory();
}

void
psx_step(void)
{
    r3000_interpreter_execute();
}

void
psx_run_frame(void)
{
    for (int i = 0; i < R3000_IPS / PSX_REFRESH_RATE; ++i) {
        psx_step();
    }

    psx_assert_irq(PSX_INTERRUPT_VBLANK);
}

void
psx_assert_irq(enum psx_interrupt i)
{
    psx.interrupt.status |= i;
    r3000_assert_irq(psx.interrupt.status & psx.interrupt.mask);
}

uint8_t
psx_read_memory8(uint32_t address)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint8_t);
        return ((uint8_t *)psx.ram)[offset];
    }

    if (between(address, PSX_EXP1_START, PSX_EXP1_END)) {
        printf("psx: info: read from exp1 register at 0x%08x\n", address);
        return 0;
    }

    if (between(address, PSX_CDROM_START, PSX_CDROM_END)) {
        printf("psx: info: read from cdrom register at 0x%08x\n", address);
        return 0;
    }

    if (between(address, PSX_EXP2_START, PSX_EXP2_END)) {
        return exp2_read8(address);
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        offset = (address - PSX_BIOS_START) / sizeof(uint8_t);
        return ((uint8_t *)psx.bios)[offset];
    }

    printf("psx: error: unknown read address 0x%08x\n", address);
    PANIC;

    return 0;
}

uint16_t
psx_read_memory16(uint32_t address)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint16_t);
        return ((uint16_t *)psx.ram)[offset];
    }

    if (address == PSX_INTERRUPT_STATUS) {
        return psx.interrupt.status;
    }

    if (address == PSX_INTERRUPT_MASK) {
        return psx.interrupt.mask;
    }

    if (between(address, PSX_SPU_START, PSX_SPU_END)) {
        return spu_read16(address);
    }

    printf("psx: error: unknown read address 0x%08x\n", address);
    PANIC;

    return 0;
}

uint32_t
psx_read_memory32(uint32_t address)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint32_t);
        return ((uint32_t *)psx.ram)[offset];
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        offset = (address - PSX_BIOS_START) / sizeof(uint32_t);
        return ((uint32_t *)psx.bios)[offset];
    }

    if (address == PSX_INTERRUPT_STATUS) {
        return psx.interrupt.status;
    }

    if (address == PSX_INTERRUPT_MASK) {
        return psx.interrupt.mask;
    }

    if (between(address, PSX_DMA_START, PSX_DMA_END)) {
        printf("psx: info: read from dma register at 0x%08x\n", address);
        PANIC;
        return 0;
    }

    if (between(address, PSX_TIMER_START, PSX_TIMER_END)) {
        printf("psx: info: read from timer register at 0x%08x\n", address);
        return 0;
    }

    if (address == PSX_GPUREAD) {
        printf("psx: info: read from gpuread\n");
        return 0;
    }

    if (address == PSX_GPUSTAT) {
        //printf("psx: info: read from gpustat\n");
        return 0x1c000000;
    }

    printf("psx: error: unknown read address 0x%08x\n", address);
    PANIC;

    return 0;
}

void
psx_write_memory8(uint32_t address, uint8_t value)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint8_t);
        ((uint8_t *)psx.ram)[offset] = value;
        return;
    }

    if (between(address, PSX_CDROM_START, PSX_CDROM_END)) {
        printf("psx: info: write to cdrom register at 0x%08x\n", address);
        return;
    }

    if (between(address, PSX_EXP2_START, PSX_EXP2_END)) {
        exp2_write8(address, value);
        return;
    }

    printf("psx: error: unknown write address 0x%08x\n", address);
    PANIC;
}

void
psx_write_memory16(uint32_t address, uint16_t value)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint16_t);
        ((uint16_t *)psx.ram)[offset] = value;
        return;
    }

    if (address == PSX_INTERRUPT_STATUS) {
        psx.interrupt.status &= value;
        r3000_assert_irq(psx.interrupt.status & psx.interrupt.mask);
        return;
    }

    if (address == PSX_INTERRUPT_MASK) {
        psx.interrupt.mask |= value;
        r3000_assert_irq(psx.interrupt.status & psx.interrupt.mask);
        return;
    }

    if (between(address, PSX_TIMER_START, PSX_TIMER_END)) {
        printf("psx: info: write to timer register at 0x%08x\n", address);
        return;
    }

    if (between(address, PSX_SPU_START, PSX_SPU_END)) {
        spu_write16(address, value);
        return;
    }

    printf("psx: error: unknown write address 0x%08x\n", address);
    PANIC;
}

void
psx_write_memory32(uint32_t address, uint32_t value)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint32_t);
        ((uint32_t *)psx.ram)[offset] = value;
        return;
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        printf("psx: error: write to bios at 0x%08x\n", address);
        PANIC;
    }

    if (between(address, PSX_MEMCTRL_START, PSX_MEMCTRL_END)) {
        printf("psx: info: write to memctrl register at 0x%08x\n", address);
        return;
    }

    if (address == PSX_MEMCTRL2) {
        printf("psx: info: write to ram_size register\n");
        return;
    }

    if (address == PSX_INTERRUPT_STATUS) {
        psx.interrupt.status &= value;
        r3000_assert_irq(psx.interrupt.status & psx.interrupt.mask);
        return;
    }

    if (address == PSX_INTERRUPT_MASK) {
        psx.interrupt.mask = value;
        r3000_assert_irq(psx.interrupt.status & psx.interrupt.mask);
        return;
    }

    if (between(address, PSX_DMA_START, PSX_DMA_END)) {
        printf("psx: info: write to dma register at 0x%08x\n", address);
        PANIC;
        return;
    }

    if (between(address, PSX_TIMER_START, PSX_TIMER_END)) {
        printf("psx: info: write to timer register at 0x%08x\n", address);
        return;
    }

    if (address == PSX_GP0) {
        printf("psx: info: write to gp0\n");
        return;
    }

    if (address == PSX_GP1) {
        printf("psx: info: write to gp1\n");
        return;
    }

    if (address == PSX_CACHECTRL) {
        printf("psx: info: write to cachectrl register\n");
        return;
    }

    printf("psx: error: unknown write address 0x%08x\n", address);
    PANIC;
}

uint8_t
psx_debug_read_memory8(uint32_t address)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint8_t);
        return ((uint8_t *)psx.ram)[offset];
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        offset = (address - PSX_BIOS_START) / sizeof(uint8_t);
        return ((uint8_t *)psx.bios)[offset];
    }

    return 0;
}

uint32_t
psx_debug_read_memory32(uint32_t address)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint32_t);
        return ((uint32_t *)psx.ram)[offset];
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        offset = (address - PSX_BIOS_START) / sizeof(uint32_t);
        return ((uint32_t *)psx.bios)[offset];
    }

    return 0;
}

void
psx_debug_write_memory32(uint32_t address, uint32_t value)
{
    uint32_t offset;

    if (between(address, PSX_RAM_START, PSX_RAM_END)) {
        offset = (address - PSX_RAM_START) / sizeof(uint32_t);
        ((uint32_t *)psx.ram)[offset] = value;
        return;
    }

    if (between(address, PSX_BIOS_START, PSX_BIOS_END)) {
        offset = (address - PSX_BIOS_START) / sizeof(uint32_t);
        ((uint32_t *)psx.bios)[offset] = value;
        return;
    }
}

uint8_t *
psx_debug_ram(void)
{
    return psx.ram;
}

uint8_t *
psx_debug_bios(void)
{
    return psx.bios;
}

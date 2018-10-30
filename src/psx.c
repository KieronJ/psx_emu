#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "psx.h"
#include "r3000.h"
#include "r3000_interpreter.h"
#include "util.h"

#define PSX_REFRESH_RATE        60
#define PSX_CYCLES_PER_REFRESH  (R3000_FREQ / PSX_REFRESH_RATE)

#define PSX_EXP1_SIZE           MEGABYTES(8)
#define PSX_MEMCTRL_SIZE        0x24
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

    psx.interrupt.status = 0;
    psx.interrupt.mask = 0;
}

void
psx_setup(const char *bios_path)
{
    r3000_setup();

    psx.bios = malloc(PSX_BIOS_SIZE);
    psx.ram = malloc(PSX_RAM_SIZE);

    assert(psx.bios);
    assert(psx.ram);

    memset(psx.bios, 0, PSX_BIOS_SIZE);

    psx_reset_memory();
    psx_load_bios(bios_path);
}

void
psx_shutdown(void)
{
    assert(psx.bios);
    assert(psx.ram);

    free(psx.bios);
    free(psx.ram);
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
    for (int i = 0; i < 100; ++i) {
        //printf("%f\n", (float)i / (float)PSX_CYCLES_PER_REFRESH);
        psx_step();
    }
}

uint8_t
psx_read_memory8(uint32_t address)
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

    if (between(address, PSX_EXP1_START, PSX_EXP1_END)) {
        printf("psx: info: write to exp1 register at 0x%08x\n", address);
        return 0;
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

    if (between(address, PSX_EXP2_START, PSX_EXP2_END)) {
        printf("psx: info: write to exp2 register at 0x%08x\n", address);
        return;
    }

    printf("psx: error: unknown write address 0x%08x\n", address);
    PANIC;
}

void
psx_write_memory16(uint32_t address, uint16_t value)
{
    (void)value;

    if (between(address, PSX_SPU_START, PSX_SPU_END)) {
        printf("psx: info: write to spu register at 0x%08x\n", address);
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
        return;
    }

    if (address == PSX_INTERRUPT_MASK) {
        psx.interrupt.mask = value;
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

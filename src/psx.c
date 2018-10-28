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

#define PSX_BIOS_SIZE           KILOBYTES(512)
#define PSX_RAM_SIZE            MEGABYTES(2)
#define PSX_MEMCTRL_SIZE        0x24

#define PSX_BIOS_START          0x1fc00000
#define PSX_BIOS_END            PSX_BIOS_START + PSX_BIOS_SIZE

#define PSX_RAM_START           0x00000000
#define PSX_RAM_END             PSX_RAM_START + PSX_RAM_SIZE

#define PSX_MEMCTRL_START       0x1f801000
#define PSX_MEMCTRL_END         PSX_MEMCTRL_START + PSX_MEMCTRL_SIZE

#define PSX_MEMCTRL2            0x1f801060

#define PSX_CACHECTRL           0xfffe0130

struct psx {
    void *bios;
    void *ram;
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

void
psx_setup(const char *bios_path)
{
    r3000_setup();

    psx.bios = malloc(PSX_BIOS_SIZE);
    psx.ram = malloc(PSX_RAM_SIZE);

    assert(psx.bios);
    assert(psx.ram);

    memset(psx.bios, 0, PSX_BIOS_SIZE);
    memset(psx.ram, 0, PSX_RAM_SIZE);

    psx_load_bios(bios_path);
}

void
psx_step(void)
{
    r3000_interpreter_execute();
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
psx_write_memory32(uint32_t address, uint32_t value)
{
    (void)value;

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

    if (address == PSX_CACHECTRL) {
        printf("psx: info: write to cachectrl register\n");
        return;
    }

    printf("psx: error: unknown write address 0x%08x\n", address);
    PANIC;
}

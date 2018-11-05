#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "spu.h"

enum spu_transfer_mode {
    SPU_TRANSFER_MODE_STOP,
    SPU_TRANSFER_MODE_MANUAL,
    SPU_TRANSFER_MODE_DMAWRITE,
    SPU_TRANSFER_MODE_DMAREAD
};

struct spu_voice {
    union {
        struct {
            uint16_t left;
            uint16_t right;
        };
        uint32_t both;
    } volume;

    uint16_t sample_rate;
    uint16_t start_address;

    union {
        struct {
            uint16_t bottom;
            uint16_t top;
        };
        uint32_t both;
    } adsr;

    uint16_t adsr_current_volume;
};

static uint16_t
spu_voice_read16(const struct spu_voice *voice, uint32_t address)
{
    switch (address & 0xf) {
    case 0xc:
        return voice->adsr_current_volume;
    default:
        printf("spu: error: read from unknown register 0x%08x\n", address);
        PANIC;
    }

    return 0;
}

static void
spu_voice_write16(struct spu_voice *voice, uint32_t address, uint16_t value)
{
    switch (address & 0xf) {
    case 0x0:
        voice->volume.left = value;
        break;
    case 0x2:
        voice->volume.right = value;
        break;
    case 0x4:
        voice->sample_rate = value;
        break;
    case 0x6:
        voice->start_address = value;
        break;
    case 0x8:
        voice->adsr.bottom = value;
        break;
    case 0xa:
        voice->adsr.top = value;
        break;
    default:
        printf("spu: error: write to unknown register 0x%08x: 0x%04x\n",
               address, value);
        PANIC;
    }
}

struct spu {
    void *ram;

    struct spu_voice voice[24];

    union {
        struct {
            uint16_t left;
            uint16_t right;
        };
        uint32_t both;
    } main_volume, reverb_volume, cd_volume, extern_volume, msame,
      mcomb[4], dsame, mdiff, ddiff, mapf[2], vin;

    union {
        struct {
            uint16_t bottom;
            uint16_t top;
        };
        uint32_t both;
    } kon, koff, pmon, non, eon;

    struct {
        uint16_t data[32];
        size_t index;
    } fifo;

    struct {
        uint16_t address;
        uint16_t current_address;
        uint16_t control;
    } data_transfer;

    uint16_t mbase;
    uint16_t dapf[2];
    uint16_t viir;
    uint16_t vcomb[4];
    uint16_t vwall;
    uint16_t vapf[2];

    uint16_t control;
    uint16_t status;
};

static struct spu spu;

static void
spu_fifo_push(uint16_t value)
{
    if (spu.fifo.index > 31) {
        return;
    }

    spu.fifo.data[spu.fifo.index++] = value;
}

static struct spu_voice *
spu_get_voice(uint32_t address)
{
    return &spu.voice[(address & 0x1f0) >> 4];
}

enum spu_transfer_mode
spu_control_transfer_mode(void)
{
    return (spu.control & 0x30) >> 4;
}

static void
spu_memory_write16(uint32_t address, uint16_t value)
{
    assert(address < SPU_RAM_SIZE);

    ((uint16_t *)spu.ram)[address / 2] = value;
}

static void
spu_write_control(uint16_t value)
{
    spu.control = value;

    spu.status &= ~0x80;
    spu.status |= (value & 0x20) ? 0x80 : 0;

    spu.status &= ~0x3f;
    spu.status |= value & 0x3f;

    if (spu_control_transfer_mode() == SPU_TRANSFER_MODE_MANUAL) {
        for (size_t i = 0; i < spu.fifo.index; ++i) {
            uint32_t address;
            uint16_t value;

            address = spu.data_transfer.current_address;
            spu.data_transfer.current_address += 2;
            value = spu.fifo.data[i];

            spu_memory_write16(address, value);
        }

        spu.fifo.index = 0;
    }
}

void
spu_setup(void)
{
    spu.ram = malloc(SPU_RAM_SIZE);
    assert(spu.ram);
}

void
spu_shutdown(void)
{
    assert(spu.ram);
    free(spu.ram);
}

void
spu_hard_reset(void)
{
    assert(spu.ram);
    memset(spu.ram, 0, SPU_RAM_SIZE);
}

uint16_t
spu_read16(uint32_t address)
{
    switch (address) {
    case 0x1f801c00 ... 0x1f801d7e:
        return spu_voice_read16(spu_get_voice(address), address);
    case 0x1f801d88:
    case 0x1f801d8a:
        printf("spu: warning: read from write-only KON register\n");
        return 0;
    case 0x1f801d8c:
    case 0x1f801d8e:
        printf("spu: warning: read from write-only KOFF register\n");
        return 0;
    case 0x1f801daa:
        return spu.control;
    case 0x1f801dac:
        return spu.data_transfer.control;
    case 0x1f801dae:
        return spu.status;
    default:
        printf("spu: error: read from unknown register 0x%08x\n", address);
        PANIC;
    }

    return 0;
}

void
spu_write16(uint32_t address, uint16_t value)
{
    switch (address) {
    case 0x1f801c00 ... 0x1f801d7e:
        spu_voice_write16(spu_get_voice(address), address, value);
        break;
    case 0x1f801d80:
        spu.main_volume.left = value;
        break;
    case 0x1f801d82:
        spu.main_volume.right = value;
        break;
    case 0x1f801d84:
        spu.reverb_volume.left = value;
        break;
    case 0x1f801d86:
        spu.reverb_volume.right = value;
        break;
    case 0x1f801d88:
        spu.kon.bottom = value;
        break;
    case 0x1f801d8a:
        spu.kon.top = value;
        break;
    case 0x1f801d8c:
        spu.koff.bottom = value;
        break;
    case 0x1f801d8e:
        spu.koff.top = value;
        break;
    case 0x1f801d90:
        spu.pmon.bottom = value;
        break;
    case 0x1f801d92:
        spu.pmon.top = value;
        break;
    case 0x1f801d94:
        spu.non.bottom = value;
        break;
    case 0x1f801d96:
        spu.non.top = value;
        break;
    case 0x1f801d98:
        spu.eon.bottom = value;
        break;
    case 0x1f801d9a:
        spu.eon.top = value;
        break;
    case 0x1f801da2:
        spu.mbase = value;
        break;
    case 0x1f801da6:
        spu.data_transfer.address = value;
        spu.data_transfer.current_address = value * 8;
        break;
    case 0x1f801da8:
        spu_fifo_push(value);
        break;
    case 0x1f801daa:
        spu_write_control(value);
        break;
    case 0x1f801dac:
        spu.data_transfer.control = value;
        break;
    case 0x1f801db0:
        spu.cd_volume.left = value;
        break;
    case 0x1f801db2:
        spu.cd_volume.right = value;
        break;
    case 0x1f801db4:
        spu.extern_volume.left = value;
        break;
    case 0x1f801db6:
        spu.extern_volume.right = value;
        break;
    case 0x1f801dc0:
        spu.dapf[0] = value;
        break;
    case 0x1f801dc2:
        spu.dapf[1] = value;
        break;
    case 0x1f801dc4:
        spu.viir = value;
        break;
    case 0x1f801dc6:
        spu.vcomb[0] = value;
        break;
    case 0x1f801dc8:
        spu.vcomb[1] = value;
        break;
    case 0x1f801dca:
        spu.vcomb[2] = value;
        break;
    case 0x1f801dcc:
        spu.vcomb[3] = value;
        break;
    case 0x1f801dce:
        spu.vwall = value;
        break;
    case 0x1f801dd0:
        spu.vapf[0] = value;
        break;
    case 0x1f801dd2:
        spu.vapf[1] = value;
        break;
    case 0x1f801dd4:
        spu.msame.left = value;
        break;
    case 0x1f801dd6:
        spu.msame.right = value;
        break;
    case 0x1f801dd8:
        spu.mcomb[0].left = value;
        break;
    case 0x1f801dda:
        spu.mcomb[0].right = value;
        break;
    case 0x1f801ddc:
        spu.mcomb[1].left = value;
        break;
    case 0x1f801dde:
        spu.mcomb[1].right = value;
        break;
    case 0x1f801de0:
        spu.dsame.left = value;
        break;
    case 0x1f801de2:
        spu.dsame.right = value;
        break;
    case 0x1f801de4:
        spu.mdiff.left = value;
        break;
    case 0x1f801de6:
        spu.mdiff.right = value;
        break;
    case 0x1f801de8:
        spu.mcomb[2].left = value;
        break;
    case 0x1f801dea:
        spu.mcomb[2].right = value;
        break;
    case 0x1f801dec:
        spu.mcomb[3].left = value;
        break;
    case 0x1f801dee:
        spu.mcomb[3].right = value;
        break;
    case 0x1f801df0:
        spu.ddiff.left = value;
        break;
    case 0x1f801df2:
        spu.ddiff.right = value;
        break;
    case 0x1f801df4:
        spu.mapf[0].left = value;
        break;
    case 0x1f801df6:
        spu.mapf[0].right = value;
        break;
    case 0x1f801df8:
        spu.mapf[1].left = value;
        break;
    case 0x1f801dfa:
        spu.mapf[1].right = value;
        break;
    case 0x1f801dfc:
        spu.vin.left = value;
        break;
    case 0x1f801dfe:
        spu.vin.right = value;
        break;
    default:
        printf("spu: error: write to unknown register 0x%08x: 0x%04x\n",
               address, value);
        PANIC;
    }
}

uint8_t *
spu_debug_ram(void)
{
    return spu.ram;
}

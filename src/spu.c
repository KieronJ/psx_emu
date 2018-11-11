#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "spu.h"
#include "util.h"
#include "window.h"

#define SPU_CYCLES_PER_TICK             768

#define SPU_SAMPLE_BUFFER_SIZE          256
#define SPU_FIFO_SIZE                   32

#define SPU_CONTROL_TRANSFER_MODE       0x30
#define SPU_CONTROL_ENABLE              0x8000

#define SPU_STATUS_MODE                 0x3f
#define SPU_STATUS_DMA_REQUEST          0x80

#define SPU_VOICE_NR_SAMPLES            28

#define SPU_VOICE_SUSTAIN_LEVEL         0xf
#define SPU_VOICE_DECAY_SHIFT           0xf0
#define SPU_VOICE_ATTACK_STEP           0x300
#define SPU_VOICE_ATTACK_SHIFT          0x7c00
#define SPU_VOICE_ATTACK_MODE           0x8000
#define SPU_VOICE_RELEASE_SHIFT         0x1f0000
#define SPU_VOICE_RELEASE_MODE          0x200000
#define SPU_VOICE_SUSTAIN_STEP          0xc00000
#define SPU_VOICE_SUSTAIN_SHIFT         0x1f000000
#define SPU_VOICE_SUSTAIN_DIRECTION     0x40000000
#define SPU_VOICE_SUSTAIN_MODE          0x80000000

struct volume {
    union {
        struct {
            int16_t left;
            int16_t right;
        };
        uint32_t both;
    };
};

enum spu_voice_state {
    SPU_VOICE_STATE_DISABLED,
    SPU_VOICE_STATE_ATTACK,
    SPU_VOICE_STATE_DECAY,
    SPU_VOICE_STATE_SUSTAIN,
    SPU_VOICE_STATE_RELEASE
};

struct spu_voice {
    enum spu_voice_state state;
    uint32_t pitch_counter;

    struct volume volume;

    uint16_t sample_rate;

    uint32_t start_address;
    uint32_t repeat_address;
    uint32_t current_address;

    uint32_t adsr;
    int16_t adsr_current_volume;
    size_t adsr_cycles;

    int16_t sample_buffer[SPU_VOICE_NR_SAMPLES];
    int16_t prev_sample[2];

    bool reset;
};

enum spu_voice_adsr_mode {
    SPU_VOICE_ADSR_MODE_LINEAR,
    SPU_VOICE_ADSR_MODE_EXPONENTIAL,
};

enum spu_voice_adsr_direction {
    SPU_VOICE_ADSR_DIRECTION_INCREASE,
    SPU_VOICE_ADSR_DIRECTION_DECREASE,
};

static enum spu_voice_adsr_mode
spu_voice_adsr_mode(const struct spu_voice *voice)
{
    switch (voice->state) {
    case SPU_VOICE_STATE_ATTACK:
        return voice->adsr & SPU_VOICE_ATTACK_MODE;
    case SPU_VOICE_STATE_DECAY:
        return SPU_VOICE_ADSR_MODE_EXPONENTIAL;
    case SPU_VOICE_STATE_SUSTAIN:
        return voice->adsr & SPU_VOICE_SUSTAIN_MODE;
    case SPU_VOICE_STATE_RELEASE:
        return voice->adsr & SPU_VOICE_RELEASE_MODE;
    default:
        return false;
    }
}

static enum spu_voice_adsr_direction
spu_voice_adsr_direction(const struct spu_voice *voice)
{
    switch (voice->state) {
    case SPU_VOICE_STATE_ATTACK:
        return SPU_VOICE_ADSR_DIRECTION_INCREASE;
    case SPU_VOICE_STATE_DECAY:
        return SPU_VOICE_ADSR_DIRECTION_DECREASE;
    case SPU_VOICE_STATE_SUSTAIN:
        return voice->adsr & SPU_VOICE_SUSTAIN_DIRECTION;
    case SPU_VOICE_STATE_RELEASE:
        return SPU_VOICE_ADSR_DIRECTION_DECREASE;
    default:
        return false;
    }
}

static unsigned int
spu_voice_adsr_shift(const struct spu_voice *voice)
{
    switch (voice->state) {
    case SPU_VOICE_STATE_ATTACK:
        return (voice->adsr & SPU_VOICE_ATTACK_SHIFT) >> 10;
    case SPU_VOICE_STATE_DECAY:
        return (voice->adsr & SPU_VOICE_DECAY_SHIFT) >> 4;
    case SPU_VOICE_STATE_SUSTAIN:
        return (voice->adsr & SPU_VOICE_SUSTAIN_SHIFT) >> 24;
    case SPU_VOICE_STATE_RELEASE:
        return (voice->adsr & SPU_VOICE_RELEASE_SHIFT) >> 16;
    default:
        return 0;
    }
}

static int
spu_voice_adsr_step(const struct spu_voice *voice)
{
    switch (voice->state) {
    case SPU_VOICE_STATE_ATTACK:
        return 7 - ((voice->adsr & SPU_VOICE_ATTACK_STEP) >> 8);
    case SPU_VOICE_STATE_DECAY:
        return -8;
    case SPU_VOICE_STATE_SUSTAIN:
        if (spu_voice_adsr_direction(voice) == SPU_VOICE_ADSR_DIRECTION_INCREASE) {
            return 7 - ((voice->adsr & SPU_VOICE_SUSTAIN_STEP) >> 26);
        } else {
            return -8 + ((voice->adsr & SPU_VOICE_SUSTAIN_STEP) >> 26);
        }
    case SPU_VOICE_STATE_RELEASE:
        return -8;
    default:
        return 0;
    }
}

static int16_t
spu_voice_sustain_level(const struct spu_voice *voice)
{
    int16_t sustain_level;

    sustain_level = voice->adsr & SPU_VOICE_SUSTAIN_LEVEL;
    return (sustain_level + 1) * 0x800;
}

static void
spu_voice_do_adsr(struct spu_voice *voice)
{
    enum spu_voice_state state;
    int16_t sustain_level;

    int shift_value, step_value;
    enum spu_voice_adsr_mode mode;
    enum spu_voice_adsr_direction direction;
    int cycles, step;
    int new_level;

    state = voice->state;
    sustain_level = spu_voice_sustain_level(voice);
    mode = spu_voice_adsr_mode(voice);
    direction = spu_voice_adsr_direction(voice);

    if (voice->adsr_cycles != 0) {
        voice->adsr_cycles -= 1;
    }

    shift_value = spu_voice_adsr_shift(voice);
    step_value = spu_voice_adsr_step(voice);

    cycles = 1 << MAX(0, shift_value - 11);
    step = step_value << MAX(0, 11 - shift_value);

    if (mode == SPU_VOICE_ADSR_MODE_EXPONENTIAL
        && direction == SPU_VOICE_ADSR_DIRECTION_INCREASE
        && voice->adsr_current_volume > 0x6000) {
        cycles *= 4;
    }

    else if (mode == SPU_VOICE_ADSR_MODE_EXPONENTIAL
             && direction == SPU_VOICE_ADSR_DIRECTION_DECREASE) {
        step *= voice->adsr_current_volume / 0x8000;
    }

    if (voice->adsr_cycles == 0) {
        voice->adsr_cycles = cycles;
        
        new_level = voice->adsr_current_volume + step;
        voice->adsr_current_volume = clip_i32(new_level, 0, 0x7fff);

        if (state == SPU_VOICE_STATE_ATTACK
            && voice->adsr_current_volume == 0x7fff) {
            voice->state = SPU_VOICE_STATE_DECAY;
            voice->adsr_cycles = 0;
        }

        else if (state == SPU_VOICE_STATE_DECAY
                 && voice->adsr_current_volume <= sustain_level) {
            voice->state = SPU_VOICE_STATE_SUSTAIN;
            voice->adsr_cycles = 0;
        }

        else if (state == SPU_VOICE_STATE_RELEASE
                 && voice->adsr_current_volume == 0) {
            voice->state = SPU_VOICE_STATE_DISABLED;
            voice->adsr_cycles = 0;
        }
    }
}

static uint32_t
spu_voice_sample_index(const struct spu_voice *voice)
{
    return voice->pitch_counter >> 12;
}

static void
spu_voice_set_sample_index(struct spu_voice *voice, uint32_t value)
{
    voice->pitch_counter &= 0xfff;
    voice->pitch_counter |= value << 12;
}

static uint8_t
spu_voice_interpolation_index(const struct spu_voice *voice)
{
    return voice->pitch_counter >> 4;
}

static uint16_t
spu_voice_read16(struct spu_voice *voice, uint32_t offset)
{
    switch (offset & 0xf) {
    case 0xc:
        return voice->adsr_current_volume;
    }

    printf("spu: voice: error: read from unknown address 0x%x\n", offset);
    PANIC;

    return 0;
}

static void
spu_voice_write16(struct spu_voice *voice, uint32_t offset, uint16_t value)
{
    switch (offset & 0xf) {
    case 0x0:
        voice->volume.left = (value & 0x7fff) * 2;
        return;
    case 0x2:
        voice->volume.right = (value & 0x7fff) * 2;
        return;
    case 0x4:
        voice->sample_rate = value;
        return;
    case 0x6:
        voice->start_address = value * 8;
        return;
    case 0x8:
        voice->adsr &= 0xffff0000;
        voice->adsr |= value;
        return;
    case 0xa:
        voice->adsr &= 0xffff;
        voice->adsr |= value << 16;
        return;
    case 0xc:
        voice->adsr_current_volume = value;
        return;
    case 0xe:
        voice->repeat_address = value * 8;
        return;
    }

    printf("spu: voice: error: write to unknown address 0x%x\n", offset);
    PANIC;
}

static void
spu_voice_key_on(struct spu_voice *voice)
{
    voice->state = SPU_VOICE_STATE_ATTACK;
    voice->adsr_current_volume = 0;
    voice->adsr_cycles = 0;
    voice->current_address = voice->start_address;
    voice->repeat_address = voice->start_address;
}

static void
spu_voice_key_off(struct spu_voice *voice)
{
    voice->state = SPU_VOICE_STATE_RELEASE;
    voice->adsr_cycles = 0;
}

enum spu_transfer_mode {
    SPU_TRANSFER_MODE_STOP,
    SPU_TRANSFER_MODE_MANUAL,
    SPU_TRANSFER_MODE_DMA_WRITE,
    SPU_TRANSFER_MODE_DMA_READ
};

struct spu {
    void *ram;

    struct volume main_volume;
    struct volume reverb_volume;
    struct volume cd_volume;
    struct volume external_volume;

    uint32_t key_on;
    uint32_t key_off;

    uint16_t control;
    uint16_t status;

    struct {
        uint32_t address;
        uint32_t current_address;

        uint16_t buffer[SPU_FIFO_SIZE];
        size_t buffer_index;

        uint16_t control;
    } data_transfer;

    struct spu_voice voice[SPU_NR_VOICES];

    size_t counter;

    int16_t samples[SPU_SAMPLE_BUFFER_SIZE];
    size_t sample_index;
};

static struct spu spu;

static uint16_t
spu_memory_read16(uint32_t address)
{
    assert(address < SPU_RAM_SIZE);

    return ((uint16_t *)spu.ram)[address / sizeof(uint16_t)];
}

static void
spu_memory_write16(uint32_t address, uint16_t value)
{
    assert(address < SPU_RAM_SIZE);

    ((uint16_t *)spu.ram)[address / sizeof(uint16_t)] = value;
}

static void
spu_fifo_push(uint16_t value)
{
    if (spu.data_transfer.buffer_index >= SPU_FIFO_SIZE) {
        return;
    }

    spu.data_transfer.buffer[spu.data_transfer.buffer_index++] = value;
}


static enum spu_transfer_mode
spu_transfer_mode(void)
{
    return (spu.control & SPU_CONTROL_TRANSFER_MODE) >> 4;
}

static void
spu_update_status(void)
{
    uint32_t address;
    uint16_t value;

    spu.status &= ~SPU_STATUS_MODE;
    spu.status |= spu.control & SPU_STATUS_MODE;

    spu.status &= ~SPU_STATUS_DMA_REQUEST;
    spu.status |= (spu.control & 0x20) ? SPU_STATUS_DMA_REQUEST : 0;

    if (spu_transfer_mode() == SPU_TRANSFER_MODE_MANUAL) {
        for (size_t i = 0; i < spu.data_transfer.buffer_index; ++i) {
            address = spu.data_transfer.current_address;
            value = spu.data_transfer.buffer[i];

            spu_memory_write16(address, value);

            spu.data_transfer.current_address += 2;
        }

        spu.data_transfer.buffer_index = 0;
    }
}

static void
spu_update_key_on(void)
{
    for (size_t i = 0; i < SPU_NR_VOICES; ++i) {
        if (spu.key_on & (1 << i)) {
            spu_voice_key_on(&spu.voice[i]);
        }
    }

    spu.key_on = 0;
}

static void
spu_update_key_off(void)
{
    for (size_t i = 0; i < SPU_NR_VOICES; ++i) {
        if (spu.key_off & (1 << i)) {
            spu_voice_key_off(&spu.voice[i]);
        }
    }

    spu.key_off = 0;
}

static void
spu_voice_wrap_sample_index(struct spu_voice *voice)
{
    uint32_t sample_index;

    sample_index = spu_voice_sample_index(voice);
    assert(sample_index >= SPU_VOICE_NR_SAMPLES);
    sample_index -= SPU_VOICE_NR_SAMPLES;

    spu_voice_set_sample_index(voice, sample_index);
}

static int16_t adpcm_filters[16][2] = {
    {0, 0},
    {60, 0},
    {115, -52},
    {98, -55},
    {122, -60}
};

static void
spu_voice_decode_samples(struct spu_voice *voice)
{
    uint16_t header, samples;
    uint8_t flags, filter, shift;

    int32_t sample;

    header = spu_memory_read16(voice->current_address);
    flags = header >> 8;
    filter = (header >> 4) & 0xf;
    shift = header & 0xf;

    if (shift > 12) {
        shift = 8;
    }

    if (flags & 0x4) {
        voice->repeat_address = voice->current_address;
    }

    for (int i = 0; i < 7; ++i) {
        voice->current_address += 2;
        samples = spu_memory_read16(voice->current_address);

        for (int j = 0; j < 4; ++j) {
            sample = (int16_t)(samples << 12);
            sample >>= shift;

            sample += (voice->prev_sample[0] * adpcm_filters[filter][0] +
                       voice->prev_sample[1] * adpcm_filters[filter][1] + 32) / 64;

            sample = clip_i32(sample, INT16_MIN, INT16_MAX);

            voice->sample_buffer[i * 4 + j] = sample;
            voice->prev_sample[1] = voice->prev_sample[0];
            voice->prev_sample[0] = sample;
            samples >>= 4;
        }
    }

    voice->current_address += 2;

    if (flags & 0x1) {
        voice->current_address = voice->repeat_address;

        if (!(flags & 0x2)) {
            spu_voice_key_off(voice);
            voice->adsr_current_volume = 0;
        }
    }
}

static float
spu_voice_get_sample(const struct spu_voice *voice)
{
    uint32_t sample_index;

    sample_index = spu_voice_sample_index(voice);

    return i16_to_f32(voice->sample_buffer[sample_index]);
}

static void
spu_tick(void)
{
    struct spu_voice *voice;
    float sample_left, sample_right, voice_sample;

    spu_update_status();
    spu_update_key_on();
    spu_update_key_off();

    sample_left = 0.0f;
    sample_right = 0.0f;

    for (size_t i = 0; i < SPU_NR_VOICES; ++i) {
        voice = &spu.voice[i];

        if (voice->state == SPU_VOICE_STATE_DISABLED) {
            continue;
        }

        spu_voice_do_adsr(voice);

        voice_sample = spu_voice_get_sample(voice);
        voice_sample *= i16_to_f32(voice->adsr_current_volume);

        sample_left += voice_sample * i16_to_f32(voice->volume.left);
        sample_right += voice_sample * i16_to_f32(voice->volume.right);

        voice->pitch_counter += MIN(voice->sample_rate, 0x4000);

        if (spu_voice_sample_index(voice) >= SPU_VOICE_NR_SAMPLES) {
            spu_voice_wrap_sample_index(voice);
            spu_voice_decode_samples(voice);
        }
    }

    sample_left *= i16_to_f32(spu.main_volume.left);
    sample_right *= i16_to_f32(spu.main_volume.right);

    sample_left = clip_f32(sample_left, -1.0f, 1.0f);
    sample_right = clip_f32(sample_right, -1.0f, 1.0f);

    spu.samples[spu.sample_index++] = f32_to_i16(sample_left);
    spu.samples[spu.sample_index++] = f32_to_i16(sample_right);

    if (spu.sample_index >= SPU_SAMPLE_BUFFER_SIZE) {
        window_audio_write_samples(spu.samples, SPU_SAMPLE_BUFFER_SIZE);
        spu.sample_index = 0;
    }
}

void
spu_setup(void)
{
    spu.ram = malloc(SPU_RAM_SIZE);
    assert(spu.ram);

    spu.counter = 0;
    spu.data_transfer.buffer_index = 0;
    spu.sample_index = 0;
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

    spu.counter = 0;
    spu.data_transfer.buffer_index = 0;
}

void
spu_step(void)
{
    /* Tick the SPU every 33868800 / 44100 cycles */
    if (spu.counter == 0) {
        spu_tick();
    }

    spu.counter = (spu.counter + 1) % SPU_CYCLES_PER_TICK;
}

uint16_t spu_read16(uint32_t address)
{
    struct spu_voice *voice;
    uint32_t offset;

    switch (address) {
    case 0x1f801c00 ... 0x1f801d7f:
        voice = &spu.voice[(address >> 4) & 0x1f];
        offset = address & 0xf;

        return spu_voice_read16(voice, offset);
    case 0x1f801d88:
        return spu.key_on;
    case 0x1f801d8a:
        return spu.key_on >> 16;
    case 0x1f801d8c:
        return spu.key_off;
    case 0x1f801d8e:
        return spu.key_off >> 16;
    case 0x1f801daa:
        return spu.control;
    case 0x1f801dac:
        return spu.data_transfer.control;
    case 0x1f801dae:
        return spu.status;
    }

    printf("spu: error: read from unknown address 0x%08x\n", address);
    PANIC;

    return 0;
}

void spu_write16(uint32_t address, uint16_t value)
{
    struct spu_voice *voice;
    uint32_t offset;

    switch (address) {
    case 0x1f801c00 ... 0x1f801d7f:
        voice = &spu.voice[(address >> 4) & 0x1f];
        offset = address & 0xf;

        spu_voice_write16(voice, offset, value);
        return;
    case 0x1f801d80:
        spu.main_volume.left = value;
        return;
    case 0x1f801d82:
        spu.main_volume.right = value;
        return;
    case 0x1f801d84:
        spu.reverb_volume.left = value;
        return;
    case 0x1f801d86:
        spu.reverb_volume.right = value;
        return;
    case 0x1f801d88:
        spu.key_on &= 0xffff0000;
        spu.key_on |= value;
        return;
    case 0x1f801d8a:
        spu.key_on &= 0xffff;
        spu.key_on |= value << 16;
        return;
    case 0x1f801d8c:
        spu.key_off &= 0xffff0000;
        spu.key_off |= value;
        return;
    case 0x1f801d8e:
        spu.key_off &= 0xffff;
        spu.key_off |= value << 16;
        return;
    case 0x1f801d90:
        /* Voice PMON flags low */
        return;
    case 0x1f801d92:
        /* Voice PMON flags high */
        return;
    case 0x1f801d94:
        /* Voice PNON flags low */
        return;
    case 0x1f801d96:
        /* Voice PNON flags high */
        return;
    case 0x1f801d98:
        /* Voice EON flags low */
        return;
    case 0x1f801d9a:
        /* Voice EON flags high */
        return;
    case 0x1f801da2:
        /* Reverb mBASE */
        return;
    case 0x1f801da6:
        spu.data_transfer.address = value * 8;
        spu.data_transfer.current_address = value * 8;
        return;
    case 0x1f801da8:
        spu_fifo_push(value);
        return;
    case 0x1f801daa:
        spu.control = value;
        return;
    case 0x1f801dac:
        spu.data_transfer.control = value;
        return;
    case 0x1f801db0:
        spu.cd_volume.left = value;
        return;
    case 0x1f801db2:
        spu.cd_volume.right = value;
        return;
    case 0x1f801db4:
        spu.external_volume.left = value;
        return;
    case 0x1f801db6:
        spu.external_volume.right = value;
        return;
    case 0x1f801dc0 ... 0x1f801dff:
        /* Reverb config registers */
        return;
    }

    printf("spu: error: write to unknown address 0x%08x\n", address);
    PANIC;
}

uint8_t *
spu_debug_ram(void)
{
    return spu.ram;
}

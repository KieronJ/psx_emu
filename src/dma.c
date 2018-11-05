#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dma.h"
#include "macros.h"
#include "psx.h"

#define DMA_NR_CHANNELS                 7

#define DMA_BCR_BLOCK_SIZE              0xffff
#define DMA_BCR_BLOCK_AMOUNT            0xffff0000

#define DMA_CHCR_DIRECTION              0x1
#define DMA_CHCR_STEP                   0x2
#define DMA_CHCR_SYNC_MODE              0x600
#define DMA_CHCR_START                  0x1000000
#define DMA_CHCR_TRIGGER                0x10000000
#define DMA_CHCR_OTC_MASK               0x51000000

#define DMA_DICR_IRQ_MASK_BASE          16
#define DMA_DICR_IRQ_FLAG_BASE          24

#define DMA_DICR_IRQ_FORCE              0x00008000
#define DMA_DICR_IRQ_MASKS              0x007f0000
#define DMA_DICR_IRQ_MASTER_ENABLE      0x00800000
#define DMA_DICR_WRITABLE               0x00ff803f
#define DMA_DICR_IRQ_FLAGS              0x7f000000
#define DMA_DICR_IRQ_MASTER             0x80000000

enum dma_channel {
    DMA_CHANNEL_MDEC_IN,
    DMA_CHANNEL_MDEC_OUT,
    DMA_CHANNEL_GPU,
    DMA_CHANNEL_CDROM,
    DMA_CHANNEL_SPU,
    DMA_CHANNEL_PIO,
    DMA_CHANNEL_OTC
};

enum dma_channel_direction {
    DMA_CHANNEL_DIRECTION_TO_RAM,
    DMA_CHANNEL_DIRECTION_FROM_RAM
};

enum dma_channel_step {
    DMA_CHANNEL_STEP_FORWARD,
    DMA_CHANNEL_STEP_BACKWARD
};

enum dma_channel_sync_mode {
    DMA_CHANNEL_SYNC_MODE_MANUAL,
    DMA_CHANNEL_SYNC_MODE_REQUEST,
    DMA_CHANNEL_SYNC_MODE_LINKED_LIST,
    DMA_CHANNEL_SYNC_MODE_RESERVED
};

struct dma {
    struct {
        /* TODO: Handle updating of base address & block control following DMA */
        uint32_t base_address;
        uint32_t block_control;
        uint32_t channel_control;
    } channel[DMA_NR_CHANNELS];

    uint32_t priority_control;
    uint32_t interrupt_control;
};

static struct dma dma;

static bool
dma_irq_force(void)
{
    return dma.interrupt_control & DMA_DICR_IRQ_FORCE;
}

static bool
dma_irq_master_enable(void)
{
    return dma.interrupt_control & DMA_DICR_IRQ_MASTER_ENABLE;
}


static bool
dma_irq_status(void)
{
    uint8_t irq_flags, irq_masks;

    irq_flags = (dma.interrupt_control & DMA_DICR_IRQ_FLAGS) >> 24;
    irq_masks = (dma.interrupt_control & DMA_DICR_IRQ_MASKS) >> 16;

    return irq_flags & irq_masks;
}

static void
dma_update_master_flag(void)
{
    bool force_irq, enable_irq, irq_status, prev_irq;

    force_irq = dma_irq_force();
    enable_irq = dma_irq_master_enable();
    irq_status = dma_irq_status();
    prev_irq = dma.interrupt_control & DMA_DICR_IRQ_MASTER;

    dma.interrupt_control &= ~DMA_DICR_IRQ_MASTER;

    if (force_irq || (enable_irq && irq_status)) {
        dma.interrupt_control |= DMA_DICR_IRQ_MASTER;

        if (!prev_irq) {
            psx_assert_irq(PSX_INTERRUPT_DMA);
        }
    }
}

static uint32_t
dma_channel_base_address(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return dma.channel[channel].base_address;
}

static uint32_t
dma_channel_block_size(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return dma.channel[channel].block_control & DMA_BCR_BLOCK_SIZE;
}

static uint32_t
dma_channel_block_amount(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return (dma.channel[channel].block_control & DMA_BCR_BLOCK_AMOUNT) >> 16;
}

static enum dma_channel_direction
dma_channel_direction(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return dma.channel[channel].channel_control & DMA_CHCR_DIRECTION;
}

static enum dma_channel_step
dma_channel_step(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return (dma.channel[channel].channel_control & DMA_CHCR_STEP) >> 1;
}

static enum dma_channel_sync_mode
dma_channel_sync_mode(enum dma_channel channel)
{
    uint32_t channel_control;
    enum dma_channel_sync_mode sync_mode;

    assert(channel < DMA_NR_CHANNELS);

    channel_control = dma.channel[channel].channel_control;

    sync_mode = (channel_control & DMA_CHCR_SYNC_MODE) >> 9;
    assert(sync_mode != DMA_CHANNEL_SYNC_MODE_RESERVED);

    return sync_mode;
}

static bool
dma_channel_start(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return dma.channel[channel].channel_control & DMA_CHCR_START;
}

static void
dma_channel_start_clear(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    dma.channel[channel].channel_control &= ~DMA_CHCR_START;
}

static bool
dma_channel_trigger(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    return dma.channel[channel].channel_control & DMA_CHCR_TRIGGER;
}

static void
dma_channel_trigger_clear(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    dma.channel[channel].channel_control &= ~DMA_CHCR_TRIGGER;
}

static uint32_t
dma_channel_read32(uint32_t address)
{
    uint32_t channel = (address >> 4) & 0x7;
    uint32_t offset = address & 0xf;

    switch (address & 0xf) {
    case 0x0:
        return dma.channel[channel].base_address;
    case 0x8:
        return dma.channel[channel].channel_control;
    default:
        printf("dma: error: read from unknown channel offset 0x%x\n", offset);
        PANIC;
    }

    return 0;
}

static bool
dma_enabled(enum dma_channel channel)
{
    uint32_t mask;

    assert(channel < DMA_NR_CHANNELS);

    mask = 1 << ((channel * 4) + 3);

    return dma.priority_control & mask;
}

static bool
dma_activated(enum dma_channel channel)
{
    enum dma_channel_sync_mode sync_mode;
    bool trigger;

    assert(channel < DMA_NR_CHANNELS);

    sync_mode = dma_channel_sync_mode(channel);
    trigger = (sync_mode == DMA_CHANNEL_SYNC_MODE_MANUAL) ?
              dma_channel_trigger(channel) : true;

    return dma_channel_start(channel) && trigger;
}

static bool
dma_irq_masked(enum dma_channel channel)
{
    uint32_t mask;

    assert(channel < DMA_NR_CHANNELS);

    mask = 1 << (DMA_DICR_IRQ_MASK_BASE + channel);

    return dma.interrupt_control & mask;
}

static void
dma_assert_irq(enum dma_channel channel)
{
    uint32_t mask;

    assert(channel < DMA_NR_CHANNELS);

    mask = 1 << (DMA_DICR_IRQ_FLAG_BASE + channel);

    dma.interrupt_control |= mask;
    dma_update_master_flag();
}

static uint32_t
dma_channel_remaining(enum dma_channel channel)
{
    enum dma_channel_sync_mode sync_mode;

    assert(channel < DMA_NR_CHANNELS);

    sync_mode = dma_channel_sync_mode(channel);

    switch (sync_mode) {
    case DMA_CHANNEL_SYNC_MODE_MANUAL:
        return dma_channel_block_size(channel);
    case DMA_CHANNEL_SYNC_MODE_REQUEST:
        return dma_channel_block_size(channel)
               * dma_channel_block_amount(channel);
    case DMA_CHANNEL_SYNC_MODE_LINKED_LIST:
        return 0;
    default:
        PANIC;
        return 0;
    }
}

static void
dma_transfer_manual(enum dma_channel channel)
{
    enum dma_channel_direction direction;
    uint32_t address, remaining, value;

    assert(channel < DMA_NR_CHANNELS);

    direction = dma_channel_direction(channel);
    address = dma_channel_base_address(channel) & ~0x3;
    remaining = dma_channel_remaining(channel);
    value = 0;

    switch (channel) {
    case DMA_CHANNEL_OTC:
        assert(direction == DMA_CHANNEL_DIRECTION_TO_RAM);

        if (!remaining) {
            remaining = 0x10000;
        }

        while (remaining) {
            value = (remaining == 1) ? 0xffffff : ((address - 4) & 0x1ffffc);

            psx_write_memory32(address, value);

            address = value;
            remaining--;
        }

        break;
    default:
        printf("dma: error: manual transfer from unknown channel %d\n",
               channel);
        PANIC;
    }
}


static void
dma_transfer_request(enum dma_channel channel)
{
    enum dma_channel_direction direction;
    enum dma_channel_step step;
    uint32_t address, remaining;

    assert(channel < DMA_NR_CHANNELS);

    direction = dma_channel_direction(channel);
    step = dma_channel_step(channel);
    address = dma_channel_base_address(channel) & ~0x3;
    remaining = dma_channel_remaining(channel);

    switch (channel) {
    case DMA_CHANNEL_GPU:
        switch (direction) {
        case DMA_CHANNEL_DIRECTION_TO_RAM:
            PANIC;
            break;
        case DMA_CHANNEL_DIRECTION_FROM_RAM:
            while (remaining--) {
                //data = psx_read_memory32(address);

                /* TODO: Send data to GP0 */

                address += step ? -4 : 4;
                address &= 0x1ffffc;
            }

            break;
        }
        break;
    default:
        printf("dma: error: request transfer from unknown channel %d\n",
               channel);
        PANIC;
    }
}

static void
dma_transfer_linked_list(enum dma_channel channel)
{
    enum dma_channel_direction direction;
    uint32_t address, header, size;

    assert(channel < DMA_NR_CHANNELS);

    direction = dma_channel_direction(channel);
    address = dma_channel_base_address(channel) & ~0x3;

    switch (channel) {
    case DMA_CHANNEL_GPU:
        assert(direction == DMA_CHANNEL_DIRECTION_FROM_RAM);

        for (;;) {
            header = psx_read_memory32(address);
            size = header >> 24;

            for (uint32_t i = 0; i < size; ++i) {
                address = (address + 4) & 0x1ffffc;
                //command = psx_read_memory32(address);

                /* TODO: Send data to GP0 */
            }

            if (header & 0x800000) {
                break;
            }

            address = header & 0x1ffffc;
        }

        break;
    default:
        printf("dma: error: linked list transfer from unknown channel %d\n",
               channel);
        PANIC;
    }
}

static void
dma_transfer_finish(enum dma_channel channel)
{
    assert(channel < DMA_NR_CHANNELS);

    dma_channel_start_clear(channel);

    if (dma_irq_masked(channel)) {
        dma_assert_irq(channel);
    }
}

static void
dma_transfer_start(enum dma_channel channel)
{
    enum dma_channel_sync_mode sync_mode;

    assert(channel < DMA_NR_CHANNELS);

    sync_mode = dma_channel_sync_mode(channel);

    dma_channel_trigger_clear(channel);

    switch (sync_mode) {
    case DMA_CHANNEL_SYNC_MODE_MANUAL:
        dma_transfer_manual(channel);
        break;
    case DMA_CHANNEL_SYNC_MODE_REQUEST:
        dma_transfer_request(channel);
        break;
    case DMA_CHANNEL_SYNC_MODE_LINKED_LIST:
        dma_transfer_linked_list(channel);
        break;
    default:
        PANIC;
    }

    dma_transfer_finish(channel);
}

static void
dma_channel_write32(uint32_t address, uint32_t value)
{
    uint32_t channel = (address >> 4) & 0x7;
    uint32_t offset = address & 0xf;

    switch (offset) {
    case 0x0:
        dma.channel[channel].base_address = value & 0xffffff;
        break;
    case 0x4:
        dma.channel[channel].block_control = value;
        break;
    case 0x8:
        if (channel == DMA_CHANNEL_OTC) {
            value &= DMA_CHCR_OTC_MASK;
            value |= DMA_CHCR_STEP;
        }

        dma.channel[channel].channel_control = value;

        if (dma_activated(channel) && dma_enabled(channel)) {
            dma_transfer_start(channel);
        }

        break;
    default:
        printf("dma: error: write to unknown channel offset 0x%x\n", offset);
        PANIC;
    }
}

void
dma_setup(void)
{
    dma_hard_reset();
    dma_soft_reset();
}

void
dma_soft_reset(void)
{
    dma.priority_control = 0x07654321;
}

void
dma_hard_reset(void)
{
    memset(&dma, 0, sizeof(dma));
}

uint32_t
dma_read32(uint32_t address)
{
    switch (address) {
    case 0x1f801080 ... 0x1f8010ef:
        return dma_channel_read32(address);
    case 0x1f8010f0:
        return dma.priority_control;
    case 0x1f8010f4:
        return dma.interrupt_control;
    default:
        printf("dma: error: read from unknown register 0x%08x\n", address);
        PANIC;
    }

    return 0;
}

void
dma_write32(uint32_t address, uint32_t value)
{
    switch (address) {
    case 0x1f801080 ... 0x1f8010ef:
        dma_channel_write32(address, value);
        break;
    case 0x1f8010f0:
        dma.priority_control = value;
        break;
    case 0x1f8010f4:
        dma.interrupt_control &= ~DMA_DICR_WRITABLE;            /* Mask off all non-writable bits */
        dma.interrupt_control &= ~(value & DMA_DICR_IRQ_FLAGS); /* Acknowledge any IRQ flags */
        dma.interrupt_control |= value & DMA_DICR_WRITABLE;     /* Update writable values */

        dma_update_master_flag();
        break;
    default:
        printf("dma: error: write to unknown register 0x%08x: 0x%08x\n",
               address, value);
        PANIC;
    }
}

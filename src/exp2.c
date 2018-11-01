#include <stdint.h>
#include <stdio.h>

#include "exp2.h"
#include "gui.h"
#include "macros.h"

#define EXP2_BASE               0x1f802000
#define EXP2_DUART_MRA          EXP2_BASE + 0x20
#define EXP2_DUART_SRA          EXP2_BASE + 0x21
#define EXP2_DUART_CSRA         EXP2_BASE + 0x21
#define EXP2_DUART_CRA          EXP2_BASE + 0x22
#define EXP2_DUART_THRA         EXP2_BASE + 0x23
#define EXP2_DUART_ACR          EXP2_BASE + 0x24
#define EXP2_DUART_IMR          EXP2_BASE + 0x25
#define EXP2_DUART_MRB          EXP2_BASE + 0x28
#define EXP2_DUART_CSRB         EXP2_BASE + 0x29
#define EXP2_DUART_CRB          EXP2_BASE + 0x2a
#define EXP2_DUART_OPCR         EXP2_BASE + 0x2d
#define EXP2_DUART_SOP          EXP2_BASE + 0x2e
#define EXP2_DUART_ROP          EXP2_BASE + 0x2f

#define EXP2_PSX_POST           EXP2_BASE + 0x41

#define EXP2_DUART_SR_TXRDY     0x4
#define EXP2_DUART_SR_TXEMT     0x8

#define EXP2_TX_BUF_SIZE        128

static char exp2_tx_buf[EXP2_TX_BUF_SIZE];
static int exp2_tx_buf_len;

static void
exp2_tx_byte(char byte)
{
    assert(exp2_tx_buf_len < EXP2_TX_BUF_SIZE);

    if (byte == '\r') {
        return;
    }

    if (byte == '\n') {
        if (exp2_tx_buf_len != 0) {
            exp2_tx_byte('\0');
            gui_add_tty_entry(exp2_tx_buf, exp2_tx_buf_len);
            exp2_tx_buf_len = 0;
        }

        return;
    }

    exp2_tx_buf[exp2_tx_buf_len++] = byte;
}

void
exp2_setup(void)
{
    exp2_tx_buf_len = 0;
}

uint8_t
exp2_read8(uint32_t address)
{
    switch (address) {
    case EXP2_DUART_SRA:
        return EXP2_DUART_SR_TXEMT | EXP2_DUART_SR_TXRDY; /* Force Tx buffer to be empty & ready */
    default:
        printf("exp2: error: read from unknown register 0x%08x\n", address);
        PANIC;
    }

    return 0;
}

void
exp2_write8(uint32_t address, uint8_t value)
{
    switch (address) {
    case EXP2_DUART_MRA:
        break;
    case EXP2_DUART_CSRA:
        break;
    case EXP2_DUART_CRA:
        break;
    case EXP2_DUART_THRA:
        exp2_tx_byte(value);
        break;
    case EXP2_DUART_ACR:
        break;
    case EXP2_DUART_IMR:
        break;
    case EXP2_DUART_MRB:
        break;
    case EXP2_DUART_CSRB:
        break;
    case EXP2_DUART_CRB:
        break;
    case EXP2_DUART_OPCR:
        break;
    case EXP2_DUART_SOP:
        break;
    case EXP2_DUART_ROP:
        break;
    case EXP2_PSX_POST:
        printf("exp2: info: POST 0x%x\n", value);
        break;
    default:
        printf("exp2: error: write to unknown register 0x%08x: 0x%02x\n",
               address, value);
        PANIC;
    }
}

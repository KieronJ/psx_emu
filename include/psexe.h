#ifndef PSEXE_H
#define PSEXE_H

#include <stdint.h>

#define PSEXE_SIZE 0x800

struct psexe {
    char id[8];
    uint32_t text;
    uint32_t data;
    uint32_t pc;
    uint32_t gp;
    uint32_t text_address;
    uint32_t text_size;
    uint32_t data_address;
    uint32_t data_size;
    uint32_t bss_address;
    uint32_t bss_size;
    uint32_t stack_address;
    uint32_t stack_size;
    uint32_t saved_sp;
    uint32_t saved_fp;
    uint32_t saved_gp;
    uint32_t saved_ra;
    uint32_t saved_s0;
    uint8_t unused[0x7b4];
};

#endif /* PSEXE_H */

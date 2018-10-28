#include <stdio.h>

#include "psx.h"

int
main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: psx_emu bios\n");
        return 1;
    }

    psx_setup(argv[1]);

    for (;;) {
        psx_step();
    }

    return 0;
}

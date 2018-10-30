#include <stdio.h>

#include "gui.h"
#include "psx.h"
#include "window.h"

int
main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: psx_emu bios\n");
        return 1;
    }

    if (!window_setup()) {
        return 1;
    }

    psx_setup(argv[1]);

    for (;;) {
        if (gui_should_run_frame()) {
            psx_run_frame();
        }

        fflush(stdout);

        if (window_update() || gui_should_quit()) {
            break;
        }
    }

    printf("main: info: shutting down\n");

    psx_shutdown();
    window_shutdown();

    return 0;
}

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <GL/gl3w.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "gui.h"

#define WINDOW_MAJOR_VERSION    3
#define WINDOW_MINOR_VERSION    2

#define WINDOW_TITLE            "psx_emu"
#define WINDOW_WIDTH            800
#define WINDOW_HEIGHT           600

static SDL_Window *window;
static SDL_GLContext context;

bool
window_setup(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("window: error: unable to init SDL2: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, WINDOW_MAJOR_VERSION);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, WINDOW_MINOR_VERSION);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow(WINDOW_TITLE,
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_WIDTH, WINDOW_HEIGHT,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (!window) {
        printf("window: error: unable to create window: %s\n", SDL_GetError());
        return false;
    }

    context = SDL_GL_CreateContext(window);

    if (!context) {
        printf("window: error: unable to create context: %s\n",
               SDL_GetError());
        return false;
    }

    if (gl3wInit()) {
        printf("window: error: unable to init OpenGl\n");
        return false;
    }

    if (!gl3wIsSupported(WINDOW_MAJOR_VERSION, WINDOW_MINOR_VERSION)) {
        printf("window: error: OpenGL %d.%d is not supported\n",
               WINDOW_MAJOR_VERSION, WINDOW_MINOR_VERSION);
        return false;
    }

    gui_setup(window, context);

    return true;
}

bool
window_update(void)
{
    bool quit;
    SDL_Event event;

    quit = false;

    while (SDL_PollEvent(&event)) {
        gui_process_event(event);
        switch(event.type) {
        case SDL_QUIT:
            quit = true;
            break;

        case SDL_KEYUP:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                quit = true;
            }
            break;

        case SDL_DROPFILE:
            printf("window: info: dropped file %s\n", event.drop.file);
            SDL_free(event.drop.file);
            break;
        }
    }

    gui_render(window);

    SDL_GL_MakeCurrent(window, context);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gui_draw();

    SDL_GL_SwapWindow(window);
    SDL_Delay(15);

    return quit;
}

void
window_shutdown(void)
{
    assert(window);
    assert(context);

    gui_shutdown();

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

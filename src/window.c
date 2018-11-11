#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <GL/gl3w.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "gui.h"
#include "rb.h"
#include "window.h"

#define WINDOW_GL_MAJOR_VERSION         3
#define WINDOW_GL_MINOR_VERSION         2

#define WINDOW_AUDIO_SAMPLE_RATE        44100
#define WINDOW_AUDIO_NR_CHANNELS        2
#define WINDOW_AUDIO_DELAY              0.05

#define WINDOW_TITLE                    "psx_emu"
#define WINDOW_WIDTH                    800
#define WINDOW_HEIGHT                   600

#define WINDOW_FRAME_TIME               (1000.0 / 60.0)

static SDL_Window *window;
static SDL_GLContext context;
static SDL_AudioDeviceID audio_device;

static unsigned int window_last_time = 0;

static struct rb window_audio_buffer;

void
window_audio_callback(void *userdata, uint8_t *stream, int len)
{
    (void)userdata;

    memset(stream, 0, len);
    rb_read(&window_audio_buffer, stream, len);
}

bool
window_setup(void)
{
    size_t buffer_size;

    SDL_AudioSpec want, have;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        printf("window: error: unable to init SDL2: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, true);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, WINDOW_GL_MAJOR_VERSION);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, WINDOW_GL_MINOR_VERSION);
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

    SDL_GL_SetSwapInterval(0);

    if (!context) {
        printf("window: error: unable to create context: %s\n",
               SDL_GetError());
        return false;
    }

    if (gl3wInit()) {
        printf("window: error: unable to init OpenGl\n");
        return false;
    }

    if (!gl3wIsSupported(WINDOW_GL_MAJOR_VERSION, WINDOW_GL_MINOR_VERSION)) {
        printf("window: error: OpenGL %d.%d is not supported\n",
               WINDOW_GL_MAJOR_VERSION, WINDOW_GL_MINOR_VERSION);
        return false;
    }

    buffer_size = WINDOW_AUDIO_SAMPLE_RATE * WINDOW_AUDIO_NR_CHANNELS *
                  WINDOW_AUDIO_DELAY * sizeof(int16_t) * 2;

    if (!rb_init(&window_audio_buffer, buffer_size)) {
        printf("window: error: unable to allocate audio buffer\n");
        return false;
    }

    rb_clear(&window_audio_buffer);

    SDL_zero(want);
    want.freq = WINDOW_AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16;
    want.channels = WINDOW_AUDIO_NR_CHANNELS;
    want.samples = 1024;
    want.callback = window_audio_callback;

    audio_device = SDL_OpenAudioDevice(NULL, false, &want, &have, 0);

    if (!audio_device) {
        printf("window: error: unable to open audio device: %s\n",
               SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(audio_device, true);

    gui_setup(window, context);

    window_last_time = SDL_GetTicks();

    return true;
}

void
window_shutdown(void)
{
    assert(window);

    gui_shutdown();

    rb_free(&window_audio_buffer);

    SDL_CloseAudioDevice(audio_device);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

bool
window_update(void)
{
    bool quit;
    unsigned int elapsed_time;
    SDL_Event event;

    quit = false;

    if (SDL_PollEvent(&event)) {
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

    elapsed_time = SDL_GetTicks() - window_last_time;

    if (elapsed_time < WINDOW_FRAME_TIME) {
        SDL_Delay(WINDOW_FRAME_TIME - elapsed_time);
    }

    window_last_time = SDL_GetTicks();

    return quit;
}

void
window_audio_pause(bool pause)
{
    SDL_PauseAudioDevice(audio_device, pause);
}

void
window_audio_write_samples(int16_t *samples, size_t amount)
{
    rb_write(&window_audio_buffer, samples, amount * sizeof(int16_t));
}

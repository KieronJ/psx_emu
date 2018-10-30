#ifndef GUI_H
#define GUI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

void gui_setup(SDL_Window *window, SDL_GLContext context);
void gui_shutdown(void);

void gui_process_event(SDL_Event event);
void gui_render(SDL_Window *window);
void gui_draw(void);

bool gui_should_quit(void);
bool gui_should_run_frame(void);

#ifdef __cplusplus
}
#endif

#endif /* GUI_H */

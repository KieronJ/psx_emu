#ifndef WINDOW_H
#define WINDOW_H

#include <stdbool.h>

bool window_setup(void);
void window_shutdown(void);

bool window_update(void);

void window_audio_pause(bool pause);
void window_audio_write_samples(int16_t *samples, size_t amount);

#endif /* WINDOW_H */

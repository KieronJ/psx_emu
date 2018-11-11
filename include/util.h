#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>

bool between(int address, int start, int end);

bool overflow_u32(uint32_t a, uint32_t b, uint32_t v);

uint16_t clip_u16(uint16_t value, uint16_t min, uint16_t max);
int32_t clip_i32(int32_t value, int32_t min, int32_t max);
float clip_f32(float value, float min, float max);

float i16_to_f32(int16_t value);
int16_t f32_to_i16(float value);

#endif /* UTIL_H */

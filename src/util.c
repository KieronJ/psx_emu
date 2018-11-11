#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "util.h"

bool
between(int address, int start, int end)
{
    return (address >= start) && (address < end);
}

bool
overflow_u32(uint32_t a, uint32_t b, uint32_t v)
{
    return ~(a ^ b) & (a ^ v) & 0x80000000;
}

uint16_t
clip_u16(uint16_t value, uint16_t min, uint16_t max)
{
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

int32_t
clip_i32(int32_t value, int32_t min, int32_t max)
{
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

float
clip_f32(float value, float min, float max)
{
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

float
i16_to_f32(int16_t value)
{
    if (value >= 0) {
        return (float)value / (float)INT16_MAX;
    } else {
        return -(float)value / (float)INT16_MIN;
    }
}

int16_t
f32_to_i16(float value)
{
    assert(value >= -1.0f);
    assert(value <= 1.0f);

    if (value >= 0.0f) {
        return value * INT16_MAX;
    } else {
        return -value * INT16_MIN;
    }
}

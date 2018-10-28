#include <stdbool.h>
#include <stdint.h>

#include "util.h"

bool
between(int address, int start, int end)
{
    return (address >= start) && (address < end);
}

bool
overflow32(uint32_t a, uint32_t b, uint32_t v)
{
    return ~(a ^ b) & (a ^ v) & 0x80000000;
}

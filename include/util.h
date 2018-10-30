#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>
#include <stdint.h>

bool between(int address, int start, int end);

bool overflow32(uint32_t a, uint32_t b, uint32_t v);

#endif /* UTIL_H */

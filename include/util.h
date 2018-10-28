#ifndef UTIL_H
#define UTIL_H

#include <stdbool.h>

bool
between(int address, int start, int end)
{
    return (address >= start) && (address < end);
}

#endif /* UTIL_H */

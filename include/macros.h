#ifndef MACROS_H
#define MACROS_H

#include <assert.h>
#include <stdbool.h>

#define KILOBYTES(x)    ((x) * 1024)
#define MEGABYTES(x)    ((x) * KILOBYTES(1024))

#define HEX_SIGN_32(x)  ((x) >= 0x80000000 ? "-" : "")
#define HEX_ABS_32(x)   ((x) >= 0x80000000 ? -(x) : (x))

#define _PANIC          false
#define PANIC           (assert(_PANIC))

#endif /* MACROS_H */

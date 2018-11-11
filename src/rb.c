#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "rb.h"

bool
rb_init(struct rb *rb, size_t length) {
    assert(rb);

    rb->buffer = malloc(length);

    if (!rb->buffer) {
        return false;
    }

    rb->length = length;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return true;
}

void
rb_free(struct rb *rb)
{
    assert(rb);
    assert(rb->buffer);

    free(rb->buffer);
}

void
rb_clear(struct rb *rb)
{
    assert(rb);

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

size_t
rb_read(struct rb *rb, void *dest, size_t amount)
{
    size_t bytes_read;

    assert(rb);
    assert(dest);

    bytes_read = MIN(rb->count, amount);

    for (size_t i = 0; i < bytes_read; ++i) {
        ((uint8_t *)dest)[i] = ((uint8_t *)rb->buffer)[(rb->tail + i) % rb->length];
    }

    rb->tail = (rb->tail + bytes_read) % rb->length;
    rb->count -= bytes_read;

    return bytes_read;
}

size_t
rb_write(struct rb *rb, const void *src, size_t amount)
{
    size_t bytes_written;

    assert(rb);
    assert(src);

    bytes_written = MIN(rb->length - rb->count, amount);

    for (size_t i = 0; i < bytes_written; ++i) {
        ((uint8_t *)rb->buffer)[(rb->head + i) % rb->length] = ((uint8_t *)src)[i];
    }

    rb->head = (rb->head + bytes_written) % rb->length;
    rb->count += bytes_written;

    return bytes_written;
}

float rb_usage(struct rb *rb)
{
    assert(rb);

    return (float)rb->count / (float)rb->length;
}

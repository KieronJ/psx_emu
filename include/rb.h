#ifndef RB_H
#define RB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct rb {
    void *buffer;
    size_t length;
    size_t head;
    size_t tail;
    size_t count;
};

bool rb_init(struct rb *rb, size_t length);
void rb_free(struct rb *rb);
void rb_clear(struct rb *rb);

size_t rb_read(struct rb *rb, void *dest, size_t amount);
size_t rb_write(struct rb *rb, const void *src, size_t amount);

float rb_usage(struct rb *rb);

#endif /* RB_H */

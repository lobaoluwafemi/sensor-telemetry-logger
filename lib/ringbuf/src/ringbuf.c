#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint8_t capacity)
{
    rb->data = storage;
    rb->mask = (uint8_t)(capacity - 1u);
    rb->head = 0;
    rb->tail = 0;
}

bool ringbuf_put(ringbuf_t *rb, uint8_t byte)
{
    const uint8_t head = rb->head;

    /* Unsigned wraparound keeps head - tail correct once the indices roll over. */
    if ((uint8_t)(head - rb->tail) > rb->mask) {
        return false;
    }

    rb->data[head & rb->mask] = byte;
    rb->head = (uint8_t)(head + 1u); /* publish only after the byte is stored */
    return true;
}

bool ringbuf_get(ringbuf_t *rb, uint8_t *byte)
{
    const uint8_t tail = rb->tail;

    if (tail == rb->head) {
        return false;
    }

    *byte = rb->data[tail & rb->mask];
    rb->tail = (uint8_t)(tail + 1u);
    return true;
}

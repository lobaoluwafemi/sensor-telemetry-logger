/*
 * Lock-free byte FIFO for one producer and one consumer, such as main code
 * writing and a UART interrupt reading. Each side only writes its own index, and
 * single-byte indices load and store atomically on AVR, so no critical section
 * is needed.
 */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;
    uint8_t mask;
    volatile uint8_t head; /* free-running; written only by the producer */
    volatile uint8_t tail; /* free-running; written only by the consumer */
} ringbuf_t;

/* capacity must be a power of two, no larger than 128. */
void ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint8_t capacity);

/* Returns false, storing nothing, if the buffer is full. */
bool ringbuf_put(ringbuf_t *rb, uint8_t byte);

/* Returns false, leaving *byte untouched, if the buffer is empty. */
bool ringbuf_get(ringbuf_t *rb, uint8_t *byte);

#endif /* RINGBUF_H */

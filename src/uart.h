/* Interrupt-driven UART transmit, so sending telemetry never delays sampling. */
#ifndef UART_H
#define UART_H

#include <stddef.h>

void uart_init(void);

/* Queues bytes for transmission, waiting for space if the buffer is full. */
void uart_write(const char *data, size_t len);

#endif /* UART_H */

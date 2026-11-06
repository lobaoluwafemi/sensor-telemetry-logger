#include "uart.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

#include "config.h"
#include "ringbuf.h"

#define UART_TX_BUFFER_SIZE 128u

static uint8_t tx_storage[UART_TX_BUFFER_SIZE];
static ringbuf_t tx_buffer;

void uart_init(void)
{
    ringbuf_init(&tx_buffer, tx_storage, UART_TX_BUFFER_SIZE);

    UBRR0 = UART_UBRR_VALUE;
    UCSR0A = _BV(U2X0);
    UCSR0B = _BV(TXEN0);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8 data bits, no parity, 1 stop bit */
}

void uart_write(const char *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        while (!ringbuf_put(&tx_buffer, (uint8_t)data[i])) {
            /* Full: the interrupt below is draining it. Only this loop waits. */
        }
        UCSR0B |= _BV(UDRIE0);
    }
}

ISR(USART_UDRE_vect)
{
    uint8_t byte;

    if (ringbuf_get(&tx_buffer, &byte)) {
        UDR0 = byte;
    } else {
        UCSR0B &= (uint8_t)~_BV(UDRIE0);
    }
}

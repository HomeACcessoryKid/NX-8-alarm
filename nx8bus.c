#include "nx8bus.h"
#include <stdint.h>
#include <esp/gpio.h>
#include <espressif/esp_common.h>
#include <stdio.h>

typedef struct
{
    uint16_t receive_buffer[NX8BUS_MAX_RX_BUFF];
    uint8_t receive_buffer_tail;
    uint8_t receive_buffer_head;
    uint8_t buffer_overflow;
} nx8bus_buffer_t;

uint8_t rx_pin, tx_pin, enable_pin;
uint16_t bit_time=250; //4000BAUD
volatile nx8bus_buffer_t buffer;

// GPIO interrupt handler
static void handle_rx(uint8_t gpio_num)
{
    // Disable interrupt
    gpio_set_interrupt(gpio_num, GPIO_INTTYPE_NONE, handle_rx);

    // Wait till start bit is half over so we can sample the next one in the center
    sdk_os_delay_us(bit_time / 2);

    // Now sample bits
    uint16_t data = 0;
    uint32_t start_time = 0x7FFFFFFF & sdk_system_get_time();

    for (uint8_t i = 0; i < 9; i++)
    {
        while ((0x7FFFFFFF & sdk_system_get_time()) < (start_time + (bit_time * (i + 1))))
        {
            // If system timer overflow, escape from while loop
            if ((0x7FFFFFFF & sdk_system_get_time()) < start_time)
                break;
        }
        // Shift data to the right
        data >>= 1;

        // Read bit
        if (gpio_read(rx_pin))
        {
            // If high, set msb of 9bit to 1
            data |= 0x100;
        }
    }

    // Store byte in buffer
    // If buffer full, set the overflow flag and return
    uint8_t next = (buffer.receive_buffer_tail + 1) % NX8BUS_MAX_RX_BUFF;
    if (next != buffer.receive_buffer_head)
    {
        // save new data in buffer: tail points to where byte goes
        buffer.receive_buffer[buffer.receive_buffer_tail] = data; // save new byte
        buffer.receive_buffer_tail = next;
    }
    else
    {
        buffer.buffer_overflow = 1;
    }

    // Wait for stop bit
    sdk_os_delay_us(bit_time);

    // Done, reenable interrupt
    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_EDGE_NEG, handle_rx);
}

bool nx8bus_open(uint8_t rx, uint8_t tx, uint8_t enable) {
    if (rx == tx || rx == enable) return false;
    rx_pin = rx;
    tx_pin = tx;
    enable_pin = enable;

    // Setup Rx
    gpio_enable(rx_pin, GPIO_INPUT);
    gpio_set_pullup(rx_pin, false, false);
    // Setup Tx
    gpio_enable(tx_pin, GPIO_OUTPUT);
    gpio_set_pullup(tx_pin, false, false);
    gpio_write(tx_pin, 1); //TODO check basic state
    // Setup Enable
    gpio_enable(enable_pin, GPIO_OUTPUT);
    gpio_set_pullup(enable_pin, false, false);
    gpio_write(enable_pin, 1); //TODO check basic state

    // Setup the interrupt handler to get the start bit
    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_EDGE_NEG, handle_rx);

    //sdk_os_delay_us(1000); // TODO: not sure if it really needed

    return true;
}

bool nx8bus_available() {
    return (buffer.receive_buffer_tail + NX8BUS_MAX_RX_BUFF - buffer.receive_buffer_head) % NX8BUS_MAX_RX_BUFF;
}

uint16_t nx8bus_read() {
    // Empty buffer?
    if (buffer.receive_buffer_head == buffer.receive_buffer_tail) return 0;

    // Read from "head"
    uint16_t data = buffer.receive_buffer[buffer.receive_buffer_head]; // grab next byte
    buffer.receive_buffer_head = (buffer.receive_buffer_head + 1) % NX8BUS_MAX_RX_BUFF;
    return data;
}


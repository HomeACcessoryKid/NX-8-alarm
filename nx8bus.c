#include "nx8bus.h"
#include <stdint.h>
#include <esp/gpio.h>
#include <espressif/esp_common.h>
#include <stdio.h>

#define NX8BUS_RX_BUFF 128 //!< Must be power of two: 2, 4, 8, 16 etc.
#define systime     (0x7FFFFFFF & sdk_system_get_time())
#define wait4bit(n) do {    while (systime < (start_time + (bit_time * (n)))) \
                               if (systime <  start_time) break; \
                       } while(0)

typedef struct {
    uint16_t receive_buffer[NX8BUS_RX_BUFF];
    uint8_t receive_buffer_tail;
    uint8_t receive_buffer_head;
    uint8_t buffer_overflow;
} nx8bus_buffer_t;

uint8_t rx_pin, tx_pin;
uint16_t bit_time=250; //4000BAUD
volatile nx8bus_buffer_t buffer;

uint16_t nx8bus_CRC(const uint8_t * data, int len) { //https://en.wikipedia.org/wiki/Fletcher%27s_checksum
    uint32_t c0=0, c1=0;
    int i;

    for (i = 0; i < len; ++i) {
            c0 = c0 + *data++;
            c1 = c1 + c0;
    }
    c0 = c0 % 255;
    c1 = c1 % 255;
    return (c1 << 8 | c0);
}

// GPIO interrupt handler
static void handle_rx(uint8_t gpio_num) {
    // Disable interrupt
    gpio_set_interrupt(gpio_num, GPIO_INTTYPE_NONE, handle_rx);

    // Wait till start bit is half over so we can sample the next one in the center
    sdk_os_delay_us(bit_time / 2);

    uint16_t data = 0;
    uint32_t start_time = systime;

    for (uint8_t i = 0; i < 9; i++) {
        wait4bit(i + 1);
        data >>= 1; // Shift data to the right
        if (gpio_read(rx_pin)) data |= 0x100; // If read bit high, set msb of 9bit to 1
    }
    // Store byte in buffer. If buffer full, set the overflow flag and return
    uint8_t next = (buffer.receive_buffer_tail + 1) % NX8BUS_RX_BUFF;
    if (next != buffer.receive_buffer_head) {
        // save new data in buffer: tail points to where byte goes
        buffer.receive_buffer[buffer.receive_buffer_tail] = data; // save new byte
        buffer.receive_buffer_tail = next;
    } else buffer.buffer_overflow = 1;

    sdk_os_delay_us(bit_time); // Wait for stop bit

    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_EDGE_NEG, handle_rx); // Done, reenable interrupt
}

bool nx8bus_open(uint8_t rx, uint8_t tx) {
    if (rx == tx) return false;
    rx_pin = rx; tx_pin = tx;

    // Setup Rx
    gpio_enable(rx_pin, GPIO_INPUT);
    gpio_set_pullup(rx_pin, false, false);
    // Setup Tx
    gpio_enable(tx_pin, GPIO_OUTPUT);
    gpio_write(tx_pin, 0); //sets bus driver to high Z

    // Setup the interrupt handler to get the start bit
    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_EDGE_NEG, handle_rx);

    return true;
}

void nx8bus_put(uint16_t cc) {
    //tx->1 sends a BIT zero on the bus as a start bit
    gpio_write(tx_pin, 1); //start bit

    uint32_t start_time = systime;
    for (uint8_t i = 0; i < 9; i++) {
        wait4bit(i + 1);
        gpio_write(tx_pin, (cc & (1 << i))?0:1); // TX bits are inverted!
    }

    wait4bit(10);
    gpio_write(tx_pin, 0); //stop bit=idle state of TX
    sdk_os_delay_us(bit_time);
}

void nx8bus_command(uint8_t * data, uint8_t len) {
    uint16_t ss;    
    uint16_t crc;    
    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_NONE, handle_rx); //we must be half duplex else read interrupt will stop us
    sdk_os_delay_us(bit_time);
    for (int i=0;i<len;i++){
        ss=data[i];
        if (!i) ss+=0x100;
        nx8bus_put(ss);
    }
    crc=nx8bus_CRC(data,len);
    nx8bus_put(crc%256);
    nx8bus_put(crc/256);
    sdk_os_delay_us(bit_time);
    gpio_set_interrupt(rx_pin, GPIO_INTTYPE_EDGE_NEG, handle_rx);
}

bool nx8bus_available() {
    return (buffer.receive_buffer_tail + NX8BUS_RX_BUFF - buffer.receive_buffer_head) % NX8BUS_RX_BUFF;
}

uint16_t nx8bus_read() {
    if (buffer.receive_buffer_head == buffer.receive_buffer_tail) return 0; // Empty buffer?

    // Read from "head"
    uint16_t data = buffer.receive_buffer[buffer.receive_buffer_head]; // grab next byte
    buffer.receive_buffer_head = (buffer.receive_buffer_head + 1) % NX8BUS_RX_BUFF;
    return data;
}


#ifndef NX8BUS_H_
#define NX8BUS_H_

#include <stdint.h>
#include <stdbool.h>


#ifndef NX8BUS_MAX_RX_BUFF
    #define NX8BUS_MAX_RX_BUFF 128 //!< Must be power of two: 2, 4, 8, 16 etc.
#endif

/**
 * Initialize nx8 bus and setup interrupt handler
 * @param rx_pin GPIO pin number for RX
 * @param tx_pin GPIO pin number for TX
 * @return true if no errors occured otherwise false
 */
bool nx8bus_open(uint8_t rx_pin, uint8_t tx_pin, uint8_t enable);

/**
 * Check if data is available
 * @return true if data is available otherwise false
 */
bool nx8bus_available();

/**
 * Read current byte from internal buffer if available.
 *
 * NOTE: This call is non blocking.
 * NOTE: You have to check softuart_available() first.
 * @return current byte if available otherwise 0
 */
uint16_t nx8bus_read();

#endif /* NX8BUS_H_ */

/** (c) 2019 HomeACcessoryKid@gmail.com
 * This is a driver for the NX8 bus based on a 4512 bus driver with a fixed address selected and that pin gets 0 V input
 * The inhibit pin 15 is connected to a NPN driver with 10k collector to 12V bus power and by a base 180kohm to the tx pin 
 * The Zoutput pin 14 is connected to the bus dataline. So tx=0 is high-Z and tx=1 is zero out. The bus itself is 12v idle
 * The rx pin is reading the bus by a divider of 10k to ground and 33k to the dataline
 * Some credit goes to the softuart writers on which this is loosly based
 */
#ifndef NX8BUS_H_
#define NX8BUS_H_

#include <stdint.h>
#include <stdbool.h>


/**
 * Calculate the CRC over a character string of len
 * @param data character string
 * @param len  number of bytes in that string
 * @return 16bit CRC
 */
uint16_t nx8bus_CRC(const uint8_t * data, int len);

/**
 * Initialize nx8 bus and setup interrupt handler
 * @param rx_pin GPIO pin number for RX
 * @param tx_pin GPIO pin number for TX
 * @return true if no errors occured otherwise false
 */
bool nx8bus_open(uint8_t rx_pin, uint8_t tx_pin);

/**
 * Put command to nx8bus uart
 * @param data 8 bit symbols which create a command plus content, without CRC
 * @param len  number of symbols without CRC
 */
void nx8bus_command(uint8_t * data, uint8_t len);

/**
 * Check if data is available
 * @return true if data is available otherwise false
 */
bool nx8bus_available();

/**
 * Read current symbol from internal buffer if available.
 *
 * NOTE: This call is non blocking.
 * NOTE: You have to check nx8bus_available() first.
 * @return current two byte symbol if available otherwise 0
 */
uint16_t nx8bus_read();

#endif /* NX8BUS_H_ */

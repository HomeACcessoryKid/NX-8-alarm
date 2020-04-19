# NX-8-alarm

## (c) 2019-2020 HomeACcessoryKid@gmail.com
 *  This example makes an NetworX NX-8 alarm system homekit enabled.
 *  The alarm can switch between off, away and sleep
 *  the individual sensors are set out as individual motion sensors accessories
 *  It uses any ESP8266 with as little as 1MB flash. 
 *  read nx8bus.h for more intructions
 *  UDPlogger is used to have remote logging
 *  LCM is enabled in case you want remote updates

## The NX-8 bus
 * This is a driver for the NX8 bus based on a 4512 bus driver with a fixed address selected and that pin gets 0 V input
 * The inhibit pin 15 is connected to a NPN driver with 10k collector to 12V bus power and by a base 180kohm to the tx pin 
 * The Zoutput pin 14 is connected to the bus dataline. So tx=0 is high-Z and tx=1 is zero out. The bus itself is 12v idle
 * The rx pin is reading the bus by a divider of 10k to ground and 33k to the dataline
 * Some credit goes to the softuart writers on which this is loosly based

## Change history
### 0.3.1 added all printf to UPDlogger

### 0.3.0 updated all submodules to april 2020 versions

### 0.2.2 the first operational version
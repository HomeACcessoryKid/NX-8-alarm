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
### 0.4.0 removed NTP time but count 32bit microsecond wraparound=71minutes

### 0.3.9 tuning heap info, ordblks doesn't work

### 0.3.8 added NTP time and tuned heap info

### 0.3.7 improved monitor with heap info

### 0.3.6 added a monitor task to show heap and wifi channel

### 0.3.5 reverted UDPlog change and homekit update bugfix

### 0.3.4 started UDPlog from the very beginning

### 0.3.3 made bus  messages DEBUG print only and fix PIN -1 mode

### 0.3.2 changed PIN feedback to -1 and introduced debug switch

### 0.3.1 added all printf to UPDlogger

### 0.3.0 updated all submodules to april 2020 versions

### 0.2.2 the first operational version
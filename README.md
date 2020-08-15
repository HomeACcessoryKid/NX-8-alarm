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
### 0.5.1 reduced various stack sizes of tasks

### 0.5.0 added task statistics readout for high water mark of various tasks

### 0.4.13 removed stealing heap for long term stability test

### 0.4.12 changed heap report in json code and stealing 8k heap

### 0.4.11 using memory branch and stealing 7k heap

### 0.4.10 reduce ram by 5k to test memory pressure and changed remove code

### 0.4.9 removing old sockets from same IP

### 0.4.8 also report address of closing connection

### 0.4.7 make homekit report existing connections when new connection arrives

### 0.4.6 changed to debug line in json_end and json_free

### 0.4.5 added debug line in write_characteristic_json of esp-homekit/server.c

### 0.4.4 make heap probe table based and make H:M:S timeoutput

### 0.4.3 set heap probe factor to 4/5 instead of 2/3

### 0.4.2 crude routine to monitor the size of the biggest available heap block

### 0.4.1 fixed monitor report format

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
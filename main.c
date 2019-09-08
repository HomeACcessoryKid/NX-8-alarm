/*  (c) 2019 HomeAccessoryKid
 *  This example drives a basic curtain motor.
 *  It uses any ESP8266 with as little as 1MB flash. 
 *  GPIO-0 reads a button for manual instructions
 *  GPIO-5 instructs a relay to drive the motor
 *  GPIO-4 instructs the direction=polarity of the motor by means of two relays: up or down
 *  obviously your own motor setup might be using different ways of providing these functions
 *  a HomeKit custom integer value can be set to define the time needed for 100% travel
 *  this will be interpolated to set values between 0% and 100%
 *  UDPlogger is used to have remote logging
 *  LCM is enabled in case you want remote updates
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_system.h> //for timestamp report only
#include <esp/uart.h>
#include <esp8266.h>
#include <espressif/esp_common.h> //find us-delay support
#include <FreeRTOS.h>
#include <timers.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <string.h>
#include "lwip/api.h"
#include <wifi_config.h>
#include <udplogger.h>

#define INITIALCURRENT 0

/* ============== BEGIN HOMEKIT CHARACTERISTIC DECLARATIONS =============================================================== */
int currentstate=INITIALCURRENT;
int pinbyte1=0,pinbyte2=0;
// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  "X");
homekit_characteristic_t serial       = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, "1");
homekit_characteristic_t model        = HOMEKIT_CHARACTERISTIC_(MODEL,         "Z");
homekit_characteristic_t revision     = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  "0.0.0");

// next use these two lines before calling homekit_server_init(&config);
//    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
//                                      &model.value.string_value,&revision.value.string_value);
//    config.accessories[0]->config_number=c_hash;
// end of OTA add-in instructions

homekit_value_t target_get();
void target_set(homekit_value_t value);
homekit_characteristic_t target       = HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_TARGET_STATE,  0, .getter=target_get, .setter=target_set);
homekit_characteristic_t current      = HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_CURRENT_STATE, INITIALCURRENT                           );
homekit_characteristic_t alarmtype    = HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_ALARM_TYPE,    0,                                       );


homekit_value_t target_get() {
    return HOMEKIT_UINT8(target.value.int_value);
}
void target_set(homekit_value_t value) {
    if (value.format != homekit_format_uint8) {
        UDPLUS("Invalid target-value format: %d\n", value.format);
        return;
    }
    UDPLUS("Target:%3d\n",value.int_value);
    target.value=value;
    //send the right command and verify the result
//     current.value.int_value=value.int_value;
//     homekit_characteristic_notify(&current,  HOMEKIT_UINT8(  current.value.int_value));
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_PIN1CODE HOMEKIT_CUSTOM_UUID("F0000011")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PIN1CODE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PIN1CODE, \
    .description = "PIN1-digit", \
    .format = homekit_format_int, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {9}, \
    .min_step  = (float[]) {1}, \
    .value = HOMEKIT_INT_(_value), \
    ##__VA_ARGS__
#define HOMEKIT_CHARACTERISTIC_CUSTOM_PIN2CODE HOMEKIT_CUSTOM_UUID("F0000012")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PIN2CODE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PIN2CODE, \
    .description = "PIN2-digit", \
    .format = homekit_format_int, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {9}, \
    .min_step  = (float[]) {1}, \
    .value = HOMEKIT_INT_(_value), \
    ##__VA_ARGS__
#define HOMEKIT_CHARACTERISTIC_CUSTOM_PIN3CODE HOMEKIT_CUSTOM_UUID("F0000013")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PIN3CODE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PIN3CODE, \
    .description = "PIN3-digit", \
    .format = homekit_format_int, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {9}, \
    .min_step  = (float[]) {1}, \
    .value = HOMEKIT_INT_(_value), \
    ##__VA_ARGS__
#define HOMEKIT_CHARACTERISTIC_CUSTOM_PIN4CODE HOMEKIT_CUSTOM_UUID("F0000014")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_PIN4CODE(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_PIN4CODE, \
    .description = "PIN4-digit", \
    .format = homekit_format_int, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .unit = homekit_unit_none, \
    .min_value = (float[]) {0}, \
    .max_value = (float[]) {9}, \
    .min_step  = (float[]) {1}, \
    .value = HOMEKIT_INT_(_value), \
    ##__VA_ARGS__

homekit_value_t pin_get();
homekit_characteristic_t pin1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN1CODE, 0, .getter=pin_get);
homekit_characteristic_t pin2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN2CODE, 0, .getter=pin_get);
homekit_characteristic_t pin3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN3CODE, 0, .getter=pin_get);
homekit_characteristic_t pin4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN4CODE, 0, .getter=pin_get);

homekit_value_t pin_get() {
    if (pin1.value.int_value || pin2.value.int_value || pin3.value.int_value || pin4.value.int_value  ) {
        pinbyte1=pin1.value.int_value+pin2.value.int_value*0x10;
        pinbyte2=pin3.value.int_value+pin4.value.int_value*0x10;
        pin1.value.int_value=0; pin2.value.int_value=0; pin3.value.int_value=0; pin4.value.int_value=0;
    }
    UDPLUO("\nPIN bytes %02x %02x\n",pinbyte1,pinbyte2);
    return HOMEKIT_INT(0); 
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_RETENTION HOMEKIT_CUSTOM_UUID("F0000007")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_RETENTION(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_RETENTION, \
    .description = "RetentionTime (s)", \
    .format = homekit_format_uint8, \
    .min_value=(float[])   {1}, \
    .max_value=(float[]) {240}, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .value = HOMEKIT_UINT8_(_value), \
    ##__VA_ARGS__

//TODO turn all these sensor bits into several macros
int old_motion1,old_motion2,old_motion3,old_motion4,old_motion5,old_motion6;
TimerHandle_t motionTimer1,motionTimer2,motionTimer3,motionTimer4,motionTimer5,motionTimer6;
homekit_characteristic_t motion1 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t motion2 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t motion3 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t motion4 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t motion5 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t motion6 = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);

void retention1_set(homekit_value_t value);
void retention2_set(homekit_value_t value);
void retention3_set(homekit_value_t value);
void retention4_set(homekit_value_t value);
void retention5_set(homekit_value_t value);
void retention6_set(homekit_value_t value);
homekit_characteristic_t retention1=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention1_set);
homekit_characteristic_t retention2=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention2_set);
homekit_characteristic_t retention3=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention3_set);
homekit_characteristic_t retention4=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention4_set);
homekit_characteristic_t retention5=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention5_set);
homekit_characteristic_t retention6=HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention6_set);
void retention1_set(homekit_value_t value) {
    UDPLUS("Retention1 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer1,pdMS_TO_TICKS(value.int_value*1000),100);
    retention1.value=value;
}
void retention2_set(homekit_value_t value) {
    UDPLUS("Retention2 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer2,pdMS_TO_TICKS(value.int_value*1000),100);
    retention2.value=value;
}
void retention3_set(homekit_value_t value) {
    UDPLUS("Retention3 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer3,pdMS_TO_TICKS(value.int_value*1000),100);
    retention3.value=value;
}
void retention4_set(homekit_value_t value) {
    UDPLUS("Retention4 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer4,pdMS_TO_TICKS(value.int_value*1000),100);
    retention4.value=value;
}
void retention5_set(homekit_value_t value) {
    UDPLUS("Retention5 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer5,pdMS_TO_TICKS(value.int_value*1000),100);
    retention5.value=value;
}
void retention6_set(homekit_value_t value) {
    UDPLUS("Retention6 time: %d\n", value.int_value);
    xTimerChangePeriod(motionTimer6,pdMS_TO_TICKS(value.int_value*1000),100);
    retention6.value=value;
}

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLUS("Identify\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */


#include <nx8bus.h>
#define RX_PIN 5
#define TX_PIN 2
#define ENABLE_PIN 4
#define MY_ID  0x1d8

//#define send_command(cmd) do{   nx8bus_command(cmd,sizeof(cmd)); } while(0)
#define send_command(cmd) do{   UDPLUO("\n SEND                       => "); \
                                for (int i=0;i<sizeof(cmd);i++) UDPLUO(" %02x",cmd[i]); \
                                nx8bus_command(cmd,sizeof(cmd)); \
                            } while(0)
#define read_byte(data)   do{   while(1) { \
                                    if (!nx8bus_available()) {vTaskDelay(1);continue;} \
                                    data = nx8bus_read(); break; \
                                } \
                            } while(0) //must not monopolize CPU
uint8_t command[20]; //assuming no command will be longer
uint8_t ack210[]={0x08, 0x44, 0x00};
int  pending_ack=0;
int  armed=0, stay=0, alarm=0;

void parse18(void) { //command is 10X 18 PP
    int old_current =   current.value.int_value;
    int old_target  =    target.value.int_value;
    int old_alarm   = alarmtype.value.int_value;

    armed=command[3]&0x40;
    alarm=command[4]&0x01;
    stay =command[5]&0x04;
    
    if (armed) {
        if (stay) currentstate=2; else currentstate=1;
    } else {
        currentstate=3;
    }
    current.value.int_value = alarm ? 4 : currentstate;
    if (  current.value.int_value!=old_current) 
                                    homekit_characteristic_notify(&current,  HOMEKIT_UINT8(  current.value.int_value));
    target.value.int_value=current.value.int_value;
    if (   target.value.int_value!=old_target )
                                    homekit_characteristic_notify(&target,   HOMEKIT_UINT8(   target.value.int_value));
    alarmtype.value.int_value=alarm;
    if (alarmtype.value.int_value!=old_alarm  )
                                    homekit_characteristic_notify(&alarmtype,HOMEKIT_UINT8(alarmtype.value.int_value));
                                    
    UDPLUO(" ar%d st%d cu%d al%d at%d",armed,stay,current.value.int_value,alarm,alarmtype.value.int_value);
}

void motion1timer( TimerHandle_t xTimer ) {
    if (old_motion1) homekit_characteristic_notify(&motion1,HOMEKIT_BOOL(old_motion1=motion1.value.bool_value=0));
}
void motion2timer( TimerHandle_t xTimer ) {
    if (old_motion2) homekit_characteristic_notify(&motion2,HOMEKIT_BOOL(old_motion2=motion2.value.bool_value=0));
}
void motion3timer( TimerHandle_t xTimer ) {
    if (old_motion3) homekit_characteristic_notify(&motion3,HOMEKIT_BOOL(old_motion3=motion3.value.bool_value=0));
}
void motion4timer( TimerHandle_t xTimer ) {
    if (old_motion4) homekit_characteristic_notify(&motion4,HOMEKIT_BOOL(old_motion4=motion4.value.bool_value=0));
}
void motion5timer( TimerHandle_t xTimer ) {
    if (old_motion5) homekit_characteristic_notify(&motion5,HOMEKIT_BOOL(old_motion5=motion5.value.bool_value=0));
}
void motion6timer( TimerHandle_t xTimer ) {
    if (old_motion6) homekit_characteristic_notify(&motion6,HOMEKIT_BOOL(old_motion6=motion6.value.bool_value=0));
}

void parse04(void) { //command is 10X 04
    if (command[2]&(1<<(1-1))) {
        if (!old_motion1) homekit_characteristic_notify(&motion1,HOMEKIT_BOOL(old_motion1=motion1.value.bool_value=1));
        xTimerReset(motionTimer1,100);
    }
    if (command[2]&(1<<(2-1))) {
        if (!old_motion2) homekit_characteristic_notify(&motion2,HOMEKIT_BOOL(old_motion2=motion2.value.bool_value=1));
        xTimerReset(motionTimer2,100);
    }
    if (command[2]&(1<<(3-1))) {
        if (!old_motion3) homekit_characteristic_notify(&motion3,HOMEKIT_BOOL(old_motion3=motion3.value.bool_value=1));
        xTimerReset(motionTimer3,100);
    }
    if (command[2]&(1<<(4-1))) {
        if (!old_motion4) homekit_characteristic_notify(&motion4,HOMEKIT_BOOL(old_motion4=motion4.value.bool_value=1));
        xTimerReset(motionTimer4,100);
    }
    if (command[2]&(1<<(5-1))) {
        if (!old_motion5) homekit_characteristic_notify(&motion5,HOMEKIT_BOOL(old_motion5=motion5.value.bool_value=1));
        xTimerReset(motionTimer5,100);
    }
    if (command[2]&(1<<(6-1))) {
        if (!old_motion6) homekit_characteristic_notify(&motion6,HOMEKIT_BOOL(old_motion6=motion6.value.bool_value=1));
        xTimerReset(motionTimer6,100);
    }
}

int CRC_OK(int len) {
    int l=len;
    uint16_t crc=nx8bus_CRC(command,len);
    read_byte(command[l++]);
    read_byte(command[l++]);
    UDPLUO(" checked:");
    for (int i=0;i<l;i++) UDPLUO(" %02x",command[i]);
    UDPLUO(" CRC=%04x",crc);
    if (command[l-2]==crc%256 && command[l-1]==crc/256) return 1; else {return 0; UDPLUO(" failed!");}
}

void receive_task(void *argv) {
    int state=0, send_ok=0;
    uint16_t data;
    char fill[20];
    uint32_t newtime, oldtime;
    int i=0;
    
    oldtime=sdk_system_get_time()/1000;

    nx8bus_open(RX_PIN, TX_PIN, ENABLE_PIN);
    while (true) {
        if (send_ok) UDPLUO(" SEND_OK");
        send_ok=0; //TODO replace by a semaphore
        read_byte(data);

        if (data>0xff) {
            newtime=sdk_system_get_time()/1000;
            sprintf(fill,"\n%5d%9d: ",newtime-oldtime,newtime);
            oldtime=newtime;
        }
        UDPLUO("%s%02x", data>0xff?fill:" ", data);

        switch(state) {
            case 0: { //waiting for a command
                if (data>0xff) command[0]=data-0x100;
                switch(data){
                    case 0x100: case 0x101: case 0x102: case 0x103: 
                    case 0x104: case 0x105: case 0x106: case 0x107: { //status messages
                        state=1;
                    } break;      //status messages
                    case MY_ID: { //message for me
                        state=2;
                    } break;      //message for me
                } //switch data state 0
            } break;  //waiting for a command
            case 1: { //status message 1st level
                command[1]=data;
                switch(data){
                    case 0x04: { //status 04 contains triggered zones
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) {UDPLUO(" status 04");parse04();}
                    } break;
                    case 0x07: { //status 07 contains blocked zones
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 07");
                    } break;
                    case 0x18: { //status 18 contains partition status
                        for (i=2;i< 10;i++) read_byte(command[i]);
                        if (command[2]==0 && CRC_OK(10)) {UDPLUO(" status 18 00");parse18();}
                        else {UDPLUO(" %02x skip",command[2]); read_byte(command[i++]); read_byte(command[i]);}
                    } break;
                    case 0x00: case 0x02: case 0x03: case 0x05: case 0x06:   //status 00,02,03,05,06
                    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: { //status 08-0c
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status %02x",command[1]);
                    } break;
                    case 0x01: { //status 01
                        for (i=2;i<12;i++) read_byte(command[i]);
                        if (CRC_OK(12)) UDPLUO(" status 01");
                    } break;
                    default: UDPLUO(" status unknown"); break; //unknown status message
                } //switch data state 1
                state=0; //ready for the next command because all commands read complete
                if (command[0]==0) send_ok=1; //TODO replace by a semaphore
            } break;  //status message 1st level
            case 2: { //message for me 1st level
                command[1]=data;
                switch(data){
                    case 0x10: { //2 10 is keepalive polling
                        if (CRC_OK(2)) send_command(ack210);
                    } break;
                    case 0x40: { //2 40 is 108 command ACK
                        if (CRC_OK(2)) pending_ack=0;
                    } break;
                    default: { //unknown command for me
                    } break;
                } //switch data state 2
                state=0; //ready for the next command because max len is 2+2
            } break; //message for me 1st level
            default: {
                UDPLUO(" undefined state encountered\n");
                state=0;
            } break;
        }
    }//while true
}

void alarm_init() {
    xTaskCreate(receive_task, "receive", 512, NULL, 2, NULL);
    motionTimer1=xTimerCreate("mt1",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion1timer);
    motionTimer2=xTimerCreate("mt2",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion2timer);
    motionTimer3=xTimerCreate("mt3",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion3timer);
    motionTimer4=xTimerCreate("mt4",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion4timer);
    motionTimer5=xTimerCreate("mt5",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion5timer);
    motionTimer6=xTimerCreate("mt6",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion6timer);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_security_system,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-alarm"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(SECURITY_SYSTEM, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Alarm"),
                    &target,
                    &current,
                    &alarmtype,
                    &pin1,
                    &pin2,
                    &pin3,
                    &pin4,
                    &ota_trigger,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=2,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor1"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor1"),
                    &motion1,
                    &retention1,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=3,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor2"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor2"),
                    &motion2,
                    &retention2,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=4,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor3"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor3"),
                    &motion3,
                    &retention3,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=5,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor4"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor4"),
                    &motion4,
                    &retention4,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=6,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor5"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor5"),
                    &motion5,
                    &retention5,
                    NULL
                }),
            NULL
        }),
    HOMEKIT_ACCESSORY(
        .id=7,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor6"),
                    &manufacturer,
                    &serial,
                    &model,
                    &revision,
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
                    NULL
                }),
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor6"),
                    &motion6,
                    &retention6,
                    NULL
                }),
            NULL
        }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    udplog_init(3);
    UDPLUS("\n\n\nNX-8-alarm 0.1.3\n");

    alarm_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1001; revision.value.string_value="0.1.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 230400);
    wifi_config_init("NX-8", NULL, on_wifi_ready);
}

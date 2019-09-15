/*  (c) 2019 HomeAccessoryKid
 *  This example makes an NetworX NX-8 alarm system homekit enabled.
 *  The alarm can switch between off, away and sleep
 *  the individual sensors are set out as individual motion sensors accessories
 *  It uses any ESP8266 with as little as 1MB flash. 
 *  read nx8bus.h for more intructions
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
#include <semphr.h>
#include <task.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <string.h>
#include "lwip/api.h"
#include <wifi_config.h>
#include <udplogger.h>
#include <nx8bus.h>
#define RX_PIN 5
#define TX_PIN 2
#define ENABLE_PIN 4
#define MY_ID  0xd8

uint8_t command[20]; //assuming no command will be longer
uint8_t ack210[]={0x08, 0x44, 0x00};
uint8_t  sleep[]={0x08, 0xd1, MY_ID, 0x00, 0x01}; //this is button 0
uint8_t   away[]={0x08, 0xd1, MY_ID, 0x02, 0x01}; //this is button 2
uint8_t    off[]={0x08, 0xd0, MY_ID, 0x00, 0x01, 0, 0, 0x00}; //still must set off[5] and off[6] to pin bytes
SemaphoreHandle_t send_ok;
SemaphoreHandle_t acked;
#define           INITIALCURRENT 3
int  currentstate=INITIALCURRENT,new_target=INITIALCURRENT,acked_target=-1,r2arm=0;
// values of acked_target: -2=command sent but not yet acked; -1=stable situation between changes; 0+=target from homekit

/* ============== BEGIN HOMEKIT CHARACTERISTIC DECLARATIONS =============================================================== */
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

void target_set(homekit_value_t value) {
    int my_target=value.int_value;
    //we should only move between armed states and off
    if (my_target<3 && currentstate==3)   new_target=my_target;
    if (my_target==3 && currentstate<3)   new_target=my_target;
    UDPLUS("\nNewTarget:%d MyTarget:%d\n",new_target,my_target);
}
homekit_characteristic_t target   =HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_TARGET_STATE,  INITIALCURRENT, .setter=target_set);
homekit_characteristic_t current  =HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_CURRENT_STATE, INITIALCURRENT                    );
homekit_characteristic_t alarmtype=HOMEKIT_CHARACTERISTIC_(SECURITY_SYSTEM_ALARM_TYPE,    0                                 );


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
        off[5]=pin1.value.int_value+pin2.value.int_value*0x10;
        off[6]=pin3.value.int_value+pin4.value.int_value*0x10;
        pin1.value.int_value=0; pin2.value.int_value=0; pin3.value.int_value=0; pin4.value.int_value=0;
    }
    if (off[5] || off[6]) UDPLUO("\nPIN bytes set\n"); else UDPLUO("\nPIN bytes ZERO!\n");
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

#define timerNcreate(N) \
    int old_motion ## N; \
    TimerHandle_t motionTimer ## N; \
    homekit_characteristic_t motion ## N = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0); \
    void retention ## N ## _set(homekit_value_t value); \
    homekit_characteristic_t retention ## N =HOMEKIT_CHARACTERISTIC_(CUSTOM_RETENTION,60,.setter=retention ## N ## _set); \
    void retention ## N ## _set(homekit_value_t value) { \
        UDPLUS("Retention" #N " time: %d\n", value.int_value); \
        xTimerChangePeriod(motionTimer ## N,pdMS_TO_TICKS(value.int_value*1000),100); \
        retention ## N.value=value; \
    }
//TODO turn all these sensor bits into several macros
timerNcreate(1)
timerNcreate(2)
timerNcreate(3)
timerNcreate(4)
timerNcreate(5)
timerNcreate(6)
/*int old_motion1,old_motion2,old_motion3,old_motion4,old_motion5,old_motion6;
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
}*/

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLUS("Identify\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */


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

void target_task(void *argv) {
    //if pincode=0000 then wait
    while(1) {
        if (xSemaphoreTake(send_ok,portMAX_DELAY)) {
            UDPLUO(" SEND_OK");
            if (r2arm && new_target!=target.value.int_value && new_target!=acked_target) {
                UDPLUO(" Target=%d",new_target);
                acked_target=-2; //indicates the attempt to send a new_target
                switch(new_target) {
                    case 1: send_command( away);
                      break;
                    case 2: send_command(sleep);
                      break;
                    case 3: if (currentstate<3) send_command(off);
                      break;//ONLY DO THIS IF SURE ALARM IS ARMED NOW else it arms the alarm instead of turning it off
                    default: break;
                }
                if (xSemaphoreTake(acked,pdMS_TO_TICKS(150))) { //150ms should be enough
                    acked_target=new_target;
                }
            }
        }
    }
}

void parse18(void) { //command is 10X 18 PP; see Caddx_NX-584_Communication_Protocol.pdf message 06h
    int old_target =   target.value.int_value;
    int old_current=  current.value.int_value;
    int old_alarm  =alarmtype.value.int_value;
    int armed,alarm,stay,stable;
    
    armed =command[3]&0x40;
    alarm =command[4]&0x01;
    stay  =command[5]&0x04;
    stable=command[8]&0x90?0:1; //combines acceptance_beep (bit7) and valid_pin_accepted (bit4)
    r2arm =command[8]&0x04;
    
    if (armed) {
        if (stay) currentstate=2; else currentstate=1;
    } else {
        currentstate=3;
    }
    if (stable) {
        target.value.int_value = currentstate;
        if (acked_target>=0) {new_target=currentstate; acked_target=-1;}
    } else { //recent arming or disarming
        if (acked_target==-1) acked_target=new_target; //must have come from NX-8 console
    } //it will correct new_target when stable while preventing target_task from sending out stale new_target
    if (   target.value.int_value!=old_target ) 
                                    homekit_characteristic_notify(&target,   HOMEKIT_UINT8(   target.value.int_value));
    current.value.int_value = alarm ? 4 : currentstate;
    if (  current.value.int_value!=old_current) 
                                    homekit_characteristic_notify(&current,  HOMEKIT_UINT8(  current.value.int_value));
    alarmtype.value.int_value=alarm;
    if (alarmtype.value.int_value!=old_alarm  )
                                    homekit_characteristic_notify(&alarmtype,HOMEKIT_UINT8(alarmtype.value.int_value));
                                    
    UDPLUO(" ar%d st%d cu%d al%d at%d",armed,stay,current.value.int_value,alarm,alarmtype.value.int_value);
}

#define timerNaccessory(N) \
 void motion ## N ## timer( TimerHandle_t xTimer ) { \
  if (old_motion ## N) homekit_characteristic_notify(&motion ## N,HOMEKIT_BOOL(old_motion ## N=motion ## N.value.bool_value=0)); \
 }
timerNaccessory(1)
timerNaccessory(2)
timerNaccessory(3)
timerNaccessory(4)
timerNaccessory(5)
timerNaccessory(6)
/*void motion1timer( TimerHandle_t xTimer ) {
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
}*/

#define timerNparse(N) do { \
 if (command[2]&(1<<(N-1))) { \
  if (!old_motion ## N) homekit_characteristic_notify(&motion ## N,HOMEKIT_BOOL(old_motion ## N=motion ## N.value.bool_value=1)); \
  xTimerReset(motionTimer ## N,100); \
 } \
} while(0)
void parse04(void) { //command is 10X 04
    timerNparse(1);
    timerNparse(2);
    timerNparse(3);
    timerNparse(4);
    timerNparse(5);
    timerNparse(6);
/*    if (command[2]&(1<<(1-1))) {
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
    } */
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
    int state=0;
    uint16_t data;
    char fill[20];
    uint32_t newtime, oldtime;
    int i=0;
    
    oldtime=sdk_system_get_time()/1000;

    nx8bus_open(RX_PIN, TX_PIN, ENABLE_PIN);
    while (true) {
        read_byte(data);
        xSemaphoreTake(send_ok,0);

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
                    case MY_ID+0x100: { //message for me
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
                if (command[0]==0) xSemaphoreGive(send_ok);
            } break;  //status message 1st level
            case 2: { //message for me 1st level
                command[1]=data;
                switch(data){
                    case 0x10: { //2 10 is keepalive polling
                        if (CRC_OK(2)) send_command(ack210);
                    } break;
                    case 0x40: { //2 40 is 108 command ACK
                        if (CRC_OK(2)) xSemaphoreGive(acked);
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

#define timerNcallback(N) motionTimer ## N=xTimerCreate("mt" #N,pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion ## N ## timer)
void alarm_init() {
    send_ok = xSemaphoreCreateBinary();
    acked   = xSemaphoreCreateBinary();
    xTaskCreate(receive_task, "receive", 512, NULL, 2, NULL);
    xTaskCreate( target_task,  "target", 512, NULL, 1, NULL);
    timerNcallback(1);
    timerNcallback(2);
    timerNcallback(3);
    timerNcallback(4);
    timerNcallback(5);
    timerNcallback(6);
/*  motionTimer1=xTimerCreate("mt1",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion1timer);
    motionTimer2=xTimerCreate("mt2",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion2timer);
    motionTimer3=xTimerCreate("mt3",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion3timer);
    motionTimer4=xTimerCreate("mt4",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion4timer);
    motionTimer5=xTimerCreate("mt5",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion5timer);
    motionTimer6=xTimerCreate("mt6",pdMS_TO_TICKS(60*1000),pdFALSE,NULL,motion6timer); */
}

#define timerNdefine(N,ID) \
    HOMEKIT_ACCESSORY( \
        .id=ID, \
        .category=homekit_accessory_category_sensor, \
        .services=(homekit_service_t*[]){ \
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, \
                .characteristics=(homekit_characteristic_t*[]){ \
                    HOMEKIT_CHARACTERISTIC(NAME, "NX-8-Sensor" #N), \
                    &manufacturer, \
                    &serial, \
                    &model, \
                    &revision, \
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, identify), \
                    NULL \
                }), \
            HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true, \
                .characteristics=(homekit_characteristic_t*[]){ \
                    HOMEKIT_CHARACTERISTIC(NAME, "Sensor" #N), \
                    &motion ## N, \
                    &retention ## N, \
                    NULL \
                }), \
            NULL \
        }),

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
    timerNdefine(1,2)
    timerNdefine(2,3)
    timerNdefine(3,4)
    timerNdefine(4,5)
    timerNdefine(5,6)
    timerNdefine(6,7)
    NULL
};

/*
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
*/

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    udplog_init(3);
    UDPLUS("\n\n\nNX-8-alarm 0.1.7\n");

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

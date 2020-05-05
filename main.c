/*  (c) 2019-2020 HomeAccessoryKid
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
#include <malloc.h>
#include <unistd.h>

#ifndef VERSION
 #error You must set VERSION=x.y.z to match github version tag x.y.z
#endif

#define RX_PIN 5
#define TX_PIN 4
#define MY_ID  0xd8

uint8_t command[20]; //assuming no command will be longer
uint8_t ack210[]={0x08, 0x44, 0x00};
uint8_t asleep[]={0x08, 0xd1, MY_ID, 0x00, 0x01}; //this is button 0
uint8_t   away[]={0x08, 0xd1, MY_ID, 0x02, 0x01}; //this is button 2
uint8_t    off[]={0x08, 0xd0, MY_ID, 0x00, 0x01, 0, 0, 0x00}; //still must set off[5] and off[6] to pin bytes
uint8_t   prog[]={0x08, 0xd0, MY_ID, 0x01, 0x01, 0, 0, 0x00}; //still must set prog[5] and prog[6] to pin bytes
uint8_t ack270[]={0x08, 0x40, 0x00};
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
    .min_value = (float[]){-1}, \
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
    .min_value = (float[]){-1}, \
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
    .min_value = (float[]){-1}, \
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
    .min_value = (float[]){-1}, \
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
    if (pin1.value.int_value>0 || pin2.value.int_value>0 || pin3.value.int_value>0 || pin4.value.int_value>0  ) {
        off[5]=pin1.value.int_value+pin2.value.int_value*0x10;
        off[6]=pin3.value.int_value+pin4.value.int_value*0x10;
        UDPLUS("\nPIN bytes set\n");
        prog[5]=off[5]; prog[6]=off[6];
        pin1.value.int_value=-1; pin2.value.int_value=-1; pin3.value.int_value=-1; pin4.value.int_value=-1;
    }
    if (off[5] || off[6]) return HOMEKIT_INT(-1); else return HOMEKIT_INT(0);
}

#define HOMEKIT_CHARACTERISTIC_CUSTOM_DEBUG HOMEKIT_CUSTOM_UUID("F0000008")
#define HOMEKIT_DECLARE_CHARACTERISTIC_CUSTOM_DEBUG(_value, ...) \
    .type = HOMEKIT_CHARACTERISTIC_CUSTOM_DEBUG, \
    .description = "}DebugOutput", \
    .format = homekit_format_bool, \
    .permissions = homekit_permissions_paired_read \
                 | homekit_permissions_paired_write \
                 | homekit_permissions_notify, \
    .value = HOMEKIT_BOOL_(_value), \
    ##__VA_ARGS__
    
homekit_characteristic_t debug = HOMEKIT_CHARACTERISTIC_(CUSTOM_DEBUG, false);

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

//TODO: store timer values in sysparam
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
timerNcreate(1)
timerNcreate(2)
timerNcreate(3)
timerNcreate(4)
timerNcreate(5)
timerNcreate(6)


#define DEBUGP(format,...) if (debug.value.bool_value) UDPLUO(format, ##__VA_ARGS__)

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLUS("\nIdentify: ");
    if (off[5] || off[6]) UDPLUS("PIN bytes set\n"); else UDPLUS("PIN bytes ZERO!\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */


//#define send_command(cmd) do{   nx8bus_command(cmd,sizeof(cmd)); } while(0)
#define send_command(cmd) do{   DEBUGP("\n SEND                       => "); \
                                for (int i=0;i<sizeof(cmd);i++) DEBUGP(" %02x",cmd[i]); \
                                nx8bus_command(cmd,sizeof(cmd)); \
                            } while(0)
#define read_byte(data)   do{   while(1) { \
                                    if (!nx8bus_available()) {vTaskDelay(1);continue;} \
                                    data = nx8bus_read(); break; \
                                } \
                            } while(0) //must not monopolize CPU

void target_task(void *argv) {
    while(!off[5] && !off[6]) vTaskDelay(100); //if pincode=0000 then wait 1 second
    while(1) {
        if (xSemaphoreTake(send_ok,portMAX_DELAY)) {
            DEBUGP(" SEND_OK");
//             send_command(prog); continue; //to test transmission reliability
            if (r2arm && new_target!=target.value.int_value && new_target!=acked_target) {
                DEBUGP(" Target=%d",new_target);
                acked_target=-2; //indicates the attempt to send a new_target
                switch(new_target) {
                    case 1: send_command( away);
                      break;
                    case 2: send_command(asleep);
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
                                    
    DEBUGP(" ar%d st%d cu%d al%d at%d",armed,stay,current.value.int_value,alarm,alarmtype.value.int_value);
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
}

int CRC_OK(int len) {
    int l=len;
    uint16_t crc=nx8bus_CRC(command,len);
    read_byte(command[l++]);
    read_byte(command[l++]);
    DEBUGP(" checked:");
    for (int i=0;i<l;i++) DEBUGP(" %02x",command[i]);
    DEBUGP(" CRC=%04x",crc);
    if (command[l-2]==crc%256 && command[l-1]==crc/256) return 1; else {return 0; DEBUGP(" failed!");}
}

void receive_task(void *argv) {
    int state=0;
    uint16_t data;
    char fill[20];
    uint32_t newtime, oldtime;
    int i=0;
    
    oldtime=sdk_system_get_time()/1000;

    nx8bus_open(RX_PIN, TX_PIN);
    while (true) {
        read_byte(data);
        xSemaphoreTake(send_ok,0);

        if (data>0xff) {
            newtime=sdk_system_get_time()/1000;
            sprintf(fill,"\n%5d%9d: ",newtime-oldtime,newtime);
            oldtime=newtime;
        }
        DEBUGP("%s%02x", data>0xff?fill:" ", data);

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
                        if (CRC_OK( 8)) {DEBUGP(" status 04");parse04();}
                    } break;
                    case 0x07: { //status 07 contains blocked zones
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) DEBUGP(" status 07");
                    } break;
                    case 0x18: { //status 18 contains partition status
                        for (i=2;i< 10;i++) read_byte(command[i]);
                        if (command[2]==0 && CRC_OK(10)) {DEBUGP(" status 18 00");parse18();}
                        else {DEBUGP(" %02x skip",command[2]); read_byte(command[i++]); read_byte(command[i]);}
                    } break;
                    case 0x00: case 0x02: case 0x03: case 0x05: case 0x06:   //status 00,02,03,05,06
                    case 0x08: case 0x09: case 0x0a: case 0x0b: case 0x0c: { //status 08-0c
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) DEBUGP(" status %02x",command[1]);
                    } break;
                    case 0x01: { //status 01
                        for (i=2;i<12;i++) read_byte(command[i]);
                        if (CRC_OK(12)) DEBUGP(" status 01");
                    } break;
                    default: DEBUGP(" status unknown"); break; //unknown status message
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
                    case 0x70: { //2 70 is pin-return message
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK(8)) send_command(ack270);
                    } break;
                    default: { //unknown command for me
                    } break;
                } //switch data state 2
                state=0; //ready for the next command because max len is 2+2
            } break; //message for me 1st level
            default: {
                DEBUGP(" undefined state encountered\n");
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
    xTaskCreate( target_task,  "target", 512, NULL, 3, NULL);
    timerNcallback(1);
    timerNcallback(2);
    timerNcallback(3);
    timerNcallback(4);
    timerNcallback(5);
    timerNcallback(6);
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
                    &debug,
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


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void monitor_task(void *arg) {
    uint32_t current_time, old_time=0;
    uint32_t long_time=0, seconds=0, second, minute, minutes, hour;
    extern uint32_t xPortSupervisorStackPointer;
    uint8_t old_channel=0, current_channel=0;
    uint32_t old_heap=0, current_heap=0, i=0;
    int delta_heap;
    char *dummy;
    int ref[]={60000,16000,14000,12000,11000,10000,9000,8000,7500,7000,6500,6000,5500,5000,4666,4333,4000,
                3666, 3333, 3000, 2750, 2500, 2250,2000,1800,1600,1400,1200,1000, 800, 600, 400, 200,   0};
    while(1) {
        vTaskDelay(10);
        current_heap=xPortGetFreeHeapSize();
        delta_heap=old_heap-current_heap; if (delta_heap<0) delta_heap*=-1;
        if (sdk_wifi_station_get_connect_status() == STATION_GOT_IP) current_channel=sdk_wifi_get_channel();
        if (old_channel!=current_channel || delta_heap>300) {
            old_channel =current_channel;   old_heap =current_heap;
            i=1; while (!(current_heap>ref[i])) i++;
            while (!(dummy=malloc(ref[i]))) i++;
            free(dummy); //get size of biggest block available
            current_time=sdk_system_get_time(); if (current_time<old_time) long_time+=4295; old_time=current_time;
            seconds=long_time+current_time/1000000; second=seconds%60; minutes=seconds/60; minute=minutes%60; hour=minutes/60;
            UDPLUO("--- ch:%2d big:(%5d-%5d) free:%5d sp-brk:%d @ %d:%02d:%02d\n",
                current_channel,ref[i-1],ref[i],current_heap,xPortSupervisorStackPointer-(uint32_t)sbrk(0),hour,minute,second);
        }
    }
}

void on_wifi_ready() {
    udplog_init(2);
    UDPLUS("\n\n\nNX-8-alarm " VERSION "\n");

    xTaskCreate(monitor_task, "monitor", 512, NULL, 1, NULL);
    alarm_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1001; revision.value.string_value="0.1.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}
char *reduce_available_ram;
void user_init(void) {
    uart_set_baud(0, 115200);
    reduce_available_ram=malloc(8000); //TODO: remove after experiments for memory pressure
    wifi_config_init("NX-8", NULL, on_wifi_ready);
}

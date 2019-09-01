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
    current.value.int_value=value.int_value;
    homekit_characteristic_notify(&current,  HOMEKIT_UINT8(  current.value.int_value));
}


homekit_value_t pin_get() {
    return HOMEKIT_INT(0);
}
void pin1_set(homekit_value_t value) {
    UDPLSO("Pin1Set: %d\n", value.int_value);
}
void pin2_set(homekit_value_t value) {
    UDPLSO("Pin2Set: %d\n", value.int_value);
}
void pin3_set(homekit_value_t value) {
    UDPLSO("Pin3Set: %d\n", value.int_value);
}
void pin4_set(homekit_value_t value) {
    UDPLSO("Pin4Set: %d\n", value.int_value);
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

homekit_characteristic_t pin1 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN1CODE, 0, .setter=pin1_set, .getter=pin_get);
homekit_characteristic_t pin2 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN2CODE, 0, .setter=pin2_set, .getter=pin_get);
homekit_characteristic_t pin3 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN3CODE, 0, .setter=pin3_set, .getter=pin_get);
homekit_characteristic_t pin4 = HOMEKIT_CHARACTERISTIC_(CUSTOM_PIN4CODE, 0, .setter=pin4_set, .getter=pin_get);

// void identify_task(void *_args) {
//     vTaskDelete(NULL);
// }

void identify(homekit_value_t _value) {
    UDPLUS("Identify\n");
//    xTaskCreate(identify_task, "identify", 256, NULL, 2, NULL);
}

/* ============== END HOMEKIT CHARACTERISTIC DECLARATIONS ================================================================= */


void state_task(void *argv) {
    int i;
    while(1) {
        for (i=0;i<2;i++) {
            vTaskDelay(10000/portTICK_PERIOD_MS);
            UDPLUS("alarm=>%d\n",i);
            alarmtype.value.int_value=i;
            homekit_characteristic_notify(&alarmtype,HOMEKIT_UINT8(alarmtype.value.int_value));
            if (i) current.value.int_value=4; else current.value.int_value=currentstate;
            homekit_characteristic_notify(&current,  HOMEKIT_UINT8(  current.value.int_value));
        }
    }
}
#include <nx8bus.h>
#define RX_PIN 5
#define TX_PIN 2
#define ENABLE_PIN 4
#define MY_ID  0x1d8

//#define send_command(cmd) do{   nx8bus_command(cmd,sizeof(cmd)); } while(0)
#define send_command(cmd) do{   UDPLUO("\nSEND         => 1"); \
                                for (int i=0;i<sizeof(cmd);i++) UDPLUO("%02x ",cmd[i]); \
                                nx8bus_command(cmd,sizeof(cmd)); \
                            } while(0)
#define read_byte(data)   do{   while(1) { \
                                    if (!nx8bus_available()) {vTaskDelay(1);continue;} \
                                    data = nx8bus_read(); break; \
                                } \
                            } while(0) //must not monopolize CPU
uint8_t command[20]; //assuming no command will be longer
char ack210[]={0x08, 0x44, 0x00, 0x4c, 0xa0};
int  pending_ack=0;

int CRC_OK(int len) {
    int l=len;
    read_byte(command[l++]);
    read_byte(command[l++]);

    UDPLUO(" checked:");
    for (int i=0;i<l;i++) UDPLUO(" %02x",command[i]);
    return 1; //TODO verify CRC
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
                    case 0x00: { //status 00
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 00.");
                    } break;
                    case 0x01: { //status 01
                        for (i=2;i<12;i++) read_byte(command[i]);
                        if (CRC_OK(12)) UDPLUO(" status 01.");
                        //crunch command[] for message
                    } break;
                    case 0x02: { //status 02
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 02.");
                    } break;
                    case 0x04: { //status 04
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 04.");
                    } break;
                    case 0x05: { //status 05
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 05.");
                    } break;
                    case 0x06: { //status 06
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 06.");
                    } break;
                    case 0x07: { //status 07
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 07.");
                    } break;
                    case 0x08: { //status 08
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 08.");
                    } break;
                    case 0x09: { //status 09
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 09.");
                    } break;
                    case 0x0a: { //status 0a
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 0a.");
                    } break;
                    case 0x0c: { //status 0c
                        for (i=2;i< 8;i++) read_byte(command[i]);
                        if (CRC_OK( 8)) UDPLUO(" status 0c.");
                    } break;
                    case 0x18: { //status 18
                        for (i=2;i< 10;i++) read_byte(command[i]);
                        if (command[2]==0 && CRC_OK(10)) UDPLUO(" status 18 00."); else UDPLUO(" status 18 %02x ignored",command[2]);
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
//    xTaskCreate(state_task, "State", 512, NULL, 1, NULL);
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
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    udplog_init(3);
    UDPLUS("\n\n\nNX-8-alarm 0.0.5\n");

    alarm_init();
    
    int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                      &model.value.string_value,&revision.value.string_value);
    //c_hash=1; revision.value.string_value="0.0.1"; //cheat line
    config.accessories[0]->config_number=c_hash;
    
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 230400);
    wifi_config_init("NX-8", NULL, on_wifi_ready);
}

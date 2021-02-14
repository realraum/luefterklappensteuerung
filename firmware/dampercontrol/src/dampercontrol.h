/*
 *  Damper Control Firmware
 *
 *
 *  Copyright (C) 2016 Bernhard Tittelbach <xro@realraum.at>
 *
 *  This software is made with love and spreadspace avr utils.
 *
 *  Damper Control Firmware is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  This firmware is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with these files. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DAMPER_CONTROL_H
#define DAMPER_CONTROL_H

#include <stdint.h>

/* Hardware: ESPRESSIF ESP32-WROOM-32E
 *
 *
 * ==== PINS ====
 *
 * IO0.... Boot0 / Button
 * IO1.... TXD0
 * IO2.... Onobard LED
 * IO3.... RXD0
 *
 * IO12... MISO
 * IO13... MOSI
 * IO14... SCK
 * IO15... LAN_CS
 * IO16... LAN_RESET
 * IO17... Endstop Damper 0
 * IO18... Endstop Damper 1
 * IO19... Endstop Damper 2
 *
 * IO21... Damper Motor 0
 * IO22... Damper Motor 1
 * IO23... Damper Motor 2
 *
 * IO25... PJON
 * IO26... SPI Sensor0 CS
 * IO27... SPI Sensor1 CS
 *
 * IO32... SPI Sensor2 CS
 * IO33... Main Ventilation Fan

*/

#define PIN_CS_S0 GPIO26
#define PIN_CS_S1 GPIO27
#define PIN_CS_S2 GPIO32
#define PIN_ENDSTOP_0 GPIO17
#define PIN_ENDSTOP_1 GPIO18
#define PIN_ENDSTOP_2 GPIO19
#define PIN_DAMPER_0 GPIO21
#define PIN_DAMPER_1 GPIO22
#define PIN_DAMPER_2 GPIO23
#define PIN_FAN GPIO33

//aka PD7
// see ../contrib/avr-utils/lib/arduino-leonardo/pins_arduino.h
#define PIN_PJON GPIO25


#define PIN_HIGH(REG, PIN) digitalWrite(PIN,HIGH)
#define PIN_LOW(REG, PIN)  digitalWrite(PIN,LOW)
#define PINMODE_OUTPUT(REG, PIN) pinMode(PIN,OUTPUT)
#define PINMODE_INPUT(REG, PIN) pinMode(PIN,INPUT)
#define IS_HIGH(REG,PIN) digitalRead(PIN) == HIGH

#define OP_SETBIT |=
#define OP_CLEARBIT &= ~
#define OP_CHECK &
#define PIN_SW(PORTDDRREG, PIN, OP) PORTDDRREG OP (1 << PIN)


#define _BV(x) (1 << x)

#define HIGHv OP_SETBIT
#define LOWv OP_CLEARBIT

//with F_CPU = 16MHz and TIMER3 Prescaler set to /1024, TIMER3 increments with f = 16KHz. Thus if TIMER3 reaches 16, 1ms has passed.
#define T3_MS     *16
//set TICK_TIME to 1/20 of a second
#define TICK_DURATION_IN_MS 8
#define TICK_TIME (TICK_DURATION_IN_MS T3_MS)

///// HARDWARE CONTROL DEFINES /////

#define ENDSTOP_0_ISHIGH digitalRead(PIN_ENDSTOP_0) == HIGH
#define ENDSTOP_1_ISHIGH digitalRead(PIN_ENDSTOP_1) == HIGH
#define ENDSTOP_2_ISHIGH digitalRead(PIN_ENDSTOP_2) == HIGH
#define ENDSTOP_ISHIGH(x) digitalRead(PIN_ENDSTOP_0 + x) == HIGH

#define DAMPER_MOTOR_RUN(x) digitalWrite(PIN_DAMPER_0 + x, HIGH)
#define DAMPER_MOTOR_STOP(x) digitalWrite(PIN_DAMPER_0 + x, LOW)
#define DAMPER_ISRUNNING(x) digitalRead(PIN_DAMPER_0 + x) == HIGH

#define FAN_RUN  digitalWrite(PIN_FAN,LOW)
#define FAN_STOP digitalWrite(PIN_FAN,HIGH)
#define FAN_ISRUNNING digitalRead(PIN_FAN) == LOW


/// GLOBALS ///

#define NUM_DAMPER 3

#define LAMINA_DAMPER_ID 1

enum pjon_msg_type_t {MSG_DAMPERCMD, MSG_PRESSUREINFO, MSG_ERROR, MSG_UPDATESETTINGS, MSG_PJONID_DOAUTO, MSG_PJONID_QUESTION, MSG_PJONID_INFO, MSG_PJONID_SET};
enum damper_cmds_t {DAMPER_CLOSED, DAMPER_OPEN, DAMPER_HALFOPEN};
enum fan_cmds_t {FAN_OFF=0, FAN_ON=1};
enum error_type_t {NO_ERROR, DAMPER_CONTROL_TIMEOUT};


typedef struct {
  uint8_t damper[NUM_DAMPER];
  uint8_t fan : 1;
  uint8_t fanlamina : 1;
} dampercmd_t;

typedef struct {
  uint8_t sensorid;
  float celsius;
  float pascal;
} pressureinfo_t;

typedef struct {
  uint8_t damperid;
  uint8_t errortype;
} errorinfo_t;

typedef struct {
  uint8_t damper_open_pos[NUM_DAMPER];
} updatesettings_t;

typedef struct {
  uint8_t pjon_id;
} pjonidsetting_t;

typedef struct {
  uint8_t reach; // bitfield to indicate which hardware saw this packet: damper0, damper1, damper2, fan
  union {
    dampercmd_t dampercmd;
    updatesettings_t updatesettings;
  };
} pjon_chaincast_t;

typedef struct {
  uint8_t type;
  union {
    pjon_chaincast_t chaincast;
    pressureinfo_t pressureinfo;
    errorinfo_t errorinfo;
    pjonidsetting_t pjonidsetting;
  };
} pjon_message_t;

typedef struct {
  uint8_t id;
  uint8_t length;
  pjon_message_t msg;
} pjon_message_with_sender_t;

extern bool damper_installed_[NUM_DAMPER];
extern bool sensor_installed_[NUM_DAMPER];
extern uint8_t damper_open_pos_[NUM_DAMPER];
extern uint8_t pjon_device_id_;
extern uint8_t pjon_sensor_destination_id_;

bool are_all_dampers_closed(void);
bool have_dampers_reached_target(void);
inline void task_control_dampers(void);
void task_control_fan(void);
void task_check_pressure(void);
void task_pjon(void);
void task_usbserial(void);
void handle_damper_cmd(bool didreachall, dampercmd_t *rxmsg);

void saveSettings2EEPROM();
void loadSettingsFromEEPROM();
void updateSettingsFromPacket(updatesettings_t *s);
void updateInstalledDampersFromChar(uint8_t damper_installed);
uint8_t getInstalledDampersAsBitfield();

void pjon_init();
void pjon_change_deviceid(uint8_t id);
void pjon_inject_msg(uint8_t dst, uint8_t length, uint8_t *payload);
void pjon_inject_broadcast_msg(uint8_t length, uint8_t *payload);
void pjon_send_pressure_infomsg(uint8_t sensorid, float temperature, float pressure);
void pjon_senderror_dampertimeout(uint8_t damperid);
void pjon_send_dampercmd(dampercmd_t dcmd);
void pjon_chaincast_forward(uint8_t fromid, bool didreachall, pjon_message_t* msg);
void pjon_identify_myself(uint8_t toid);
void pjon_startautoiddiscover();
void pjon_become_master_of_ids();
void pjon_broadcast_get_autoid();

void pressure_sensors_init();
void task_check_pressure();
float get_latest_pressure(uint8_t sensorid);
float get_latest_temperature(uint8_t sensorid);

#endif
/*
 *  Damper Control Firmware
 *
 *
 *  Copyright (C) 2016,2021 Bernhard Tittelbach <xro@realraum.at>
 *
 *  This software is made with love
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

#include <stdint.h>
#include "dampercontrol.h"
#include <math.h>
#include <EEPROM.h>
#include <vector>



//damper states: 0 means closed (means photoelectric fork sensor pulled LOW)
//         otherwise, time units since we enabled the motor
//               100 should be open
//               if we go over 110 without the photoelectric fork sensor signaling us, we raise an error
//   we start at 1 in order to seek the 0 position at startup via the endstop
uint8_t damper_states_[NUM_DAMPER] = {1,1,1};

//damper target states: the state that damper states is supposed to reach
uint8_t damper_target_states_[NUM_DAMPER] = {0,0,0};

bool damper_state_overflowed_[NUM_DAMPER] = {false,false,false};

uint8_t fan_target_state_ = FAN_OFF;
uint8_t fanlamina_target_state_ = FAN_OFF;

// ISR sets true if photoelectric fork x went low
bool damper_endstop_reached_[NUM_DAMPER];

////// HELPER FUNCTIONS //////

void initSysClkTimer3(void)
{
  // set counter to 0
  TCNT3 = 0x0000;
  // no outputs
  TCCR3A = 0;
  // Prescaler for Timer3: F_CPU / 1024 -> counts with f= 16KHz ms
  TCCR3B = _BV(CS32) | _BV(CS30);
  // set up "clock" comparator for first tick
  OCR3A = TICK_TIME & 0xFFFF;
  // enable interrupt
  TIMSK3 = _BV(OCIE3A);
}

void initPCInterrupt(void)
{
  //enable PinChange Interrupt
  PCICR = _BV(PCIE0);
  //set up Endstop PinChange Interrupts toggle interrupt
  PCMSK0 = (1<<PCINT4) | (1<<PCINT5) | (1<<PCINT6);
}

void initPINs()
{
  //PJON pin is intialized by pjon_init
  //endstop inputs
  PINMODE_INPUT(REG_ENDSTOP_0,PIN_ENDSTOP_0);
  PINMODE_INPUT(REG_ENDSTOP_1,PIN_ENDSTOP_1);
  PINMODE_INPUT(REG_ENDSTOP_2,PIN_ENDSTOP_2);
  //enable internal pullups
  PIN_HIGH(REG_ENDSTOP_0,PIN_ENDSTOP_0);
  PIN_HIGH(REG_ENDSTOP_1,PIN_ENDSTOP_1);
  PIN_HIGH(REG_ENDSTOP_2,PIN_ENDSTOP_2);
  //set damper and fan motor as output
  PINMODE_OUTPUT(REG_DAMPER_0,PIN_DAMPER_0);
  PINMODE_OUTPUT(REG_DAMPER_2,PIN_DAMPER_1);
  PINMODE_OUTPUT(REG_DAMPER_2,PIN_DAMPER_2);
  //PD3 is pjon, maybe it can use the INT4 someday
  DAMPER_MOTOR_STOP(0);
  DAMPER_MOTOR_STOP(1);
  DAMPER_MOTOR_STOP(2);
  PINMODE_OUTPUT(REG_FAN,PIN_FAN); //FAN
  FAN_STOP;
}

/*
//Bad Idea
void initGuessPositionFromEndstop()
{
  _delay_ms(50); // give Endstop Pins time to settle
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_endstop_reached_[d] = false;
    if (ENDSTOP_ISHIGH(d))
    {
      //if not at endstop on boot, assume we are open
      damper_states_[d] = damper_open_pos_[d];
      damper_target_states_[d] = damper_open_pos_[d];
    } else {
      damper_states_[d] = 0;
      damper_target_states_[d] = 0;
    }
  }
}*/

//note includes simulated not-installed dampers
bool are_all_dampers_closed()
{
  bool rv = true;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    rv &= (damper_states_[d] == 0);
  }
  return rv;
}

bool have_dampers_reached_target()
{
  bool rv = true;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    rv &= (damper_states_[d] == damper_target_states_[d]) || !damper_installed_[d];
  }
  return rv;
}

//return yes if the endstop of damperX was toggled since last time we asked
bool did_damper_pass_endstop(uint8_t damperid)
{
  bool rv = damper_endstop_reached_[damperid];
  //reset endstop bit, but only if we saw it set (to guard against race condition)
  //the assumption being, that it could just now have been set to true
  //but we check so much more often while the motor is turning that it's unlikely
  //we'll set a bit to false that was just set to true again
  if (rv)
    damper_endstop_reached_[damperid] = false;
  return rv;
}

//Act on a remote (or injected) command to open/close dampers and start/stop fan (dampercmd_t)
//
//Thanks to what we call chaincasting, each dampercmd_t will reach us twice.
//once with didreachall unset and later with didreachall set
//@var didreachall if this is set, we know that all other µC acknowledged the same message
//
//Dampers: move them right away, when didreachall is still false
//FAN: if to be switched off: do it right away (didreachall == false)
//FAN: if to be switched on: wait until pkt reached everyone (didreachall == true)
void handle_damper_cmd(bool didreachall, dampercmd_t *rxmsg)
{
  if (didreachall) //only switch fan if we know all dampers got the message
  {
    switch (rxmsg->fan)
    {
      case FAN_ON:
      case FAN_OFF:
        fan_target_state_ = rxmsg->fan;
        if (rxmsg->damper[LAMINA_DAMPER_ID] != DAMPER_CLOSED)
          fanlamina_target_state_ = rxmsg->fanlamina;
        break;
      default:
        fan_target_state_ = FAN_OFF;
        fanlamina_target_state_ = FAN_OFF;
        break;
    }
  }
  //on top of ladder, message will be received only once,
  //so we set dampers every time we get the message
  //even if most µC will get it twice. They start setting the damper early then
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    switch(rxmsg->damper[d])
    {
      default:
      case DAMPER_CLOSED:
        damper_target_states_[d] = 0;
        break;
      case DAMPER_OPEN:
        damper_target_states_[d] = damper_open_pos_[d];
        break;
      case DAMPER_HALFOPEN:
        damper_target_states_[d] = damper_open_pos_[d] / 2;
        break;
    }
  }
}


///////////////// Serial Interface and Debugging Code ////////////////////

void printSettings()
{
  printf("=== State ===\r\n");
  printf("PJON device id: %d\r\n", pjon_device_id_);
  printf("PJON sensor destid: %d\r\n", pjon_sensor_destination_id_);
  printf("#Dampers: %d\r\n", NUM_DAMPER);
  for (uint8_t d=0; d<NUM_DAMPER; d++) {
    printf("Damper%d: %s installed\r\n", d, (damper_installed_[d])?"is":"NOT");
    printf("\t pos: consid. open at: %d, current: %d, target: %d\r\n", damper_open_pos_[d],damper_states_[d],damper_target_states_[d]);
    printf("\t endstop lightbeam: %s\r\n", (ENDSTOP_ISHIGH(d))?"interrupted":"uninterrupted");
    printf("Pressure Sensor%d: %s installed\r\n", d, (sensor_installed_[d])?"is":"NOT");
    if (sensor_installed_[d])
    {
      printf("\t Pressure: %.2f Pa @ %.2f degC\r\n", (double) get_latest_pressure(d), (double) get_latest_temperature(d));
    }
  }
  printf("Fan Main is %s and set to %d\r\n", (FAN_ISRUNNING)?"on":"off", fan_target_state_);
  printf("Fan Laminaflow is %s and set to %d\r\n", (FAN_ISRUNNING)?"on":"off", fanlamina_target_state_);
}

enum next_char_state_t {CCMD, CDEVID, CINSTALLEDDAMPERS, CPKTDST, CPKTLEN, CPKTDATA};

//handle chars from second serial interface, or from first after prompt
next_char_state_t handle_serial2pjon(char c)
{
  static next_char_state_t next_char = CPKTDST;
  static uint8_t read_num_chars = 0;
  static uint8_t msg_dst = 0;
  static uint8_t msg_len = 0;
  static uint8_t msg_buf[0xff];

  switch (next_char) {
    default:
    case CCMD:
    case CPKTDST:
      msg_dst = c;
      next_char = CPKTLEN;
    break;
    case CPKTLEN:
      if (c == 0) {
        next_char = CPKTDST;
        break;
      }
      read_num_chars = c;
      msg_len = 0;
      next_char = CPKTDATA;
      break;
    case CPKTDATA:
      msg_buf[msg_len++] = c;
      read_num_chars--;
      if (read_num_chars == 0)
      {
        pjon_inject_msg(msg_dst, msg_len, msg_buf);
        next_char = CCMD; //CPKTDST;
      }
      break;
  }
  return next_char;
}

//handle serial byte from first ttyACM
void handle_serialdata(char c)
{
  static next_char_state_t next_char = CCMD;

  switch (next_char) {
    default:
    case CCMD:
      switch(c) {
        case '>': next_char = CPKTDST; break; //inject PJON msg
        case 'P': next_char = CDEVID; break; //set PJON ID
        case 'I': next_char = CINSTALLEDDAMPERS; break; //set installed dampers
        case 'A': pjon_broadcast_get_autoid(); break;
        case '1': pjon_send_dampercmd(dampercmd_t{{DAMPER_OPEN,DAMPER_CLOSED,DAMPER_CLOSED},FAN_ON}); break;
        case '2': pjon_send_dampercmd(dampercmd_t{{DAMPER_CLOSED,DAMPER_OPEN,DAMPER_CLOSED},FAN_ON}); break;
        case '3': pjon_send_dampercmd(dampercmd_t{{DAMPER_CLOSED,DAMPER_CLOSED,DAMPER_OPEN},FAN_ON}); break;
        case '4': pjon_send_dampercmd(dampercmd_t{{DAMPER_OPEN,DAMPER_OPEN,DAMPER_CLOSED},FAN_ON}); break;
        case '5': pjon_send_dampercmd(dampercmd_t{{DAMPER_OPEN,DAMPER_CLOSED,DAMPER_OPEN},FAN_ON}); break;
        case '6': pjon_send_dampercmd(dampercmd_t{{DAMPER_CLOSED,DAMPER_OPEN,DAMPER_OPEN},FAN_ON}); break;
        case '7': pjon_send_dampercmd(dampercmd_t{{DAMPER_OPEN,DAMPER_OPEN,DAMPER_OPEN},FAN_ON}); break;
        case '0': pjon_send_dampercmd(dampercmd_t{{DAMPER_CLOSED,DAMPER_CLOSED,DAMPER_CLOSED},FAN_OFF}); break;
        case 'o':
          damper_target_states_[0] = damper_open_pos_[0];
          damper_target_states_[1] = damper_open_pos_[1];
          damper_target_states_[2] = damper_open_pos_[2];
          printf("opening Damper0..3\r\n");
          break;
        case 'c':
          damper_target_states_[0] = 0;
          damper_target_states_[1] = 0;
          damper_target_states_[2] = 0;
          printf("closing Damper0..3\r\n");
          break;
        case 'h':
          damper_target_states_[0] = damper_open_pos_[0]/2;
          damper_target_states_[1] = damper_open_pos_[1]/2;
          damper_target_states_[2] = damper_open_pos_[2]/2;
          printf("half-open Damper0..3\r\n");
          break;
        case 'm': pjon_become_master_of_ids(); break;
        case 's': printSettings(); break;
        case '!': reset2bootloader(); break;
      }
    break;
    case CDEVID:
      pjon_change_deviceid(c - '0');
      printf("device id is now: %d\r\n", c - '0');
      next_char = CCMD;
    break;
    case CINSTALLEDDAMPERS:
      updateInstalledDampersFromChar(c - '0');
      printf("installed dampers updated\r\n");
      next_char = CCMD;
    break;
    case CPKTDST:
    case CPKTLEN:
    case CPKTDATA:
      next_char = handle_serial2pjon(c); break;
  }
}


/////////////// TASKS ////////////////////
////// repeatedly called from main ///////

/// INTERRUPT ROUTINES

/* ISR writes:
 - damper_states_
 - switch_damper_motor

 * ISR reads:
 - damper_target_states_

 * ISR only use (read/write):
 - damper_endstop_reached

*/

//called by pinchange interrupt
//set's flag if a particular endstop has been reached
//which can be checked at leasure by calling did_damper_pass_endstop(x)
void task_check_endstops()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    //trigger if endstop is pulled low, but ignore input if motor is not actually turning
    if (ENDSTOP_ISHIGH(d) == 0 && DAMPER_ISRUNNING(d))
    {
      damper_endstop_reached_[d] = true;
    }
  }
}

//instead of calling task_check_endstops in an interrupt routine
//we could call this task repeatedly in case we don't want to use interrupts
void task_simulate_pinchange_interrupt()
{
  bool interrupt=false;
  static uint8_t last_state[3] = {0,0,0};
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    uint8_t curstate = ENDSTOP_ISHIGH(d);
    if (curstate != last_state[d])
    {
      printf("pinchange on endstop %d, state:%d\r\n", d, curstate);
      interrupt=true;
      last_state[d]=curstate;
    }
  }
  if (interrupt)
    task_check_endstops();
}

//called by timer TIMER3_COMPA_vect
//for each damper whose position != target_position:
// - let motor move
// - increment counter until target_position is reached
//for each damper whose position == target_position:
// - stop motor
//self-synchronize position (which is guessed from time elapsed) each time we pass endstop
void task_control_dampers()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (!damper_installed_[d])
      continue;

    //here we self-synchronize the position time counter
    if (did_damper_pass_endstop(d))
      damper_states_[d] = 0;

    //send warning, since we timed out and that might mean the endstop does not work
    //(0x80 << sizeof(damper_states_[0]))-1 is the max-value of uint8_t aka 0xFF
    if (damper_target_states_[d] == 0 && damper_states_[d] == (0x80 << sizeof(damper_states_[0]))-1)
      damper_state_overflowed_[d] = true;

    if (damper_states_[d] != damper_target_states_[d])
    {
      //move motor
      DAMPER_MOTOR_RUN(d);
      //increment position time counter if we are moving
      damper_states_[d]++;
      // printf("Motor %d Run @%d\r\n", d, damper_states_[d]);
    } else {
      //stop motor
      DAMPER_MOTOR_STOP(d);
      // printf("Motor %d Stop @%d\r\n", d, damper_states_[d]);
    }
  }
}

//enable/disable the fan SSR
//depending on fan_target_state_ and state of local dampers
//note that remote dampers are consideren insofar that fan_target_state_ does not get set to FAN_ON unless the message has sucessfully passed all µC
void task_control_fan()
{
  switch (fan_target_state_) {
    case FAN_OFF:
    // switching off can always be done right away
    FAN_STOP;
    break;

    default:
    case FAN_ON:
    // once dampers have reached their target and if at least one damper is not closed, we switch the fan on
    if (have_dampers_reached_target() && !are_all_dampers_closed())
      FAN_RUN;
    else
      FAN_STOP;
    break;
  }
  switch (fanlamina_target_state_) {
    case FAN_OFF:
    // switching off can always be done right away
    FANLAMINA_STOP;
    break;

    default:
    case FAN_ON:
    // once dampers 2 is open and other fan is running and target state has been reached, switch on fan 2
    if (have_dampers_reached_target() && FAN_ISRUNNING)
      FANLAMINA_RUN;
    else
      FANLAMINA_STOP;
    break;
  }
}

void task_check_damper_state_overflow()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (damper_state_overflowed_[d])
    {
      damper_state_overflowed_[d] = false;
      pjon_senderror_dampertimeout(d);
    }
  }
}


///////////////// Interrupt Handlers ////////////////////

ISR(TIMER3_COMPA_vect)
{
  //called every TIME_TICK (aka 8ms)
  task_control_dampers();
  //set up "clock" comparator for next tick
  OCR3A = (OCR3A + TICK_TIME) & 0xFFFF;
/*  if (debug_)
    led_toggle();
*/
}

//https://sites.google.com/site/qeewiki/books/avr-guide/external-interrupts-on-the-atmega328
ISR(PCINT0_vect)
{
  task_check_endstops();
}


///////////////// MAIN ////////////////////
int main()
{
  MCUSR &= ~(1 << WDRF);
  wdt_disable();

  cpu_init();
  led_init();
  usbio_init();

  // init
  eeprom_init();
  loadSettingsFromEEPROM();
  pjon_init(); //PJON first since it calls arduino init which might do who knows what
  initPINs();
  // initGuessPositionFromEndstop(); //does not work well, since we never really stop exactly at the endstop
  initSysClkTimer3();
  initPCInterrupt();
  sei();
  pressure_sensors_init();

  uint16_t loop_count = 0;

  // loop
  for (;;)
  {
    int16_t BytesReceived = usbio_bytes_received();
    while(BytesReceived > 0)
    {
      int ReceivedByte = fgetc(stdin);
      if(ReceivedByte != EOF)
      {
        handle_serialdata(ReceivedByte);
      }
      BytesReceived--;
    }

    usbio_task();
    if ((loop_count & 0xFFF) == 0)
      task_check_pressure();
    if ((loop_count & 0xFFFF) == 0)
    {
      for (uint8_t d=0; d<NUM_DAMPER; d++)
      {
        if (sensor_installed_[d])
        {
          pjon_send_pressure_infomsg(d, get_latest_pressure(d), get_latest_temperature(d));
        }
      }
    }
    task_pjon();
    //task_control_dampers(); // called by timer in precise intervals, do not call from loop
    //task_simulate_pinchange_interrupt();
    task_control_fan();
    task_check_damper_state_overflow();
    loop_count++;
  }
}


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

#include <stdio.h>
#include <EEPROM.h>
#include "dampercontrol.h"

#define EEPROM_CURRENT_VERSION 1
#define EEPROM_ADDR_VERS 0
#define EEPROM_SIZE_VERS 1
#define EEPROM_ADDR_DATA (EEPROM_ADDR_VERS+EEPROM_SIZE_VERS)
#define EEPROM_SIZE_DATA 6


//read this from eeprom on start
//update on receiving installed_msg
//tells us which damper is actually controlled by this µC
bool damper_installed_[NUM_DAMPER] = {false, false, false};
bool sensor_installed_[NUM_DAMPER] = {false, false, false};

//damper time divisor:
//every millis that we increase a damper_state if damper is currently moving
//128 is a good value for TICK_DURATION_IN_MS == 7
//103 is a good value for TICK_DURATION_IN_MS == 8
//90 is a good value for TICK_DURATION_IN_MS == 10
// in Theory it would be great to time the half-rotations of the damper perfectly
// so that we would not have to rely on the endstop.
// unfortunately time and damper position correlate very poorly
// so this does not work out. Thus we have to choose a TICK_DURATION_IN_MS > 7 and a damper_open_pos < 128.
// This way we can at least garantee that we always stop at the endstop (if the endstop works) if we close.
// Otherwise the damper_state_ position might overflow and reach 0 before we are at the endstop.
uint8_t damper_open_pos_[NUM_DAMPER] = {80,80,80};

uint8_t pjon_device_id_ = 255; //not assigned
uint8_t pjon_sensor_destination_id_ = 0; //BROADCAST


///EEPROM LAYOUT
/// byte0: version
/// byte1: num dampers
/// byte2: damper0_open_pos
/// byte3: damper1_open_pos
/// byte4: damper2_open_pos
/// byte5: damper installed bitfield

void eeprom_save_settings()
{
  uint8_t eeprom_pos=EEPROM_ADDR_DATA;
  uint8_t damper_installed=0;

  EEPROM.write(eeprom_pos++, (uint8_t) pjon_device_id_);
  EEPROM.write(eeprom_pos++, (uint8_t) NUM_DAMPER);
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    EEPROM.put(eeprom_pos++, damper_open_pos_[d]);
    if (damper_installed_[d])
      damper_installed |= _BV(d);
  }
  EEPROM.put(eeprom_pos++, damper_installed);
}


void eeprom_load_settings()
{

  if (EEPROM.read(EEPROM_ADDR_VERS) != EEPROM_CURRENT_VERSION)
    return 0;
  uint8_t eeprom_pos=EEPROM_ADDR_DATA;

  pjon_device_id_ = EEPROM.read(eeprom_pos++);

  if (EEPROM.read(eeprom_pos++) != NUM_DAMPER)
    return;

  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_open_pos_[d] = EEPROM.read(eeprom_pos++);;
  }
  uint8_t damper_installed = EEPROM.read(eeprom_pos++);;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_installed_[d] = 0 < (_BV(d) & damper_installed);
  }
}

void eeprom_init()
{
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(EEPROM_ADDR_VERS) != EEPROM_CURRENT_VERSION)
  {
    EEPROM.write(EEPROM_ADDR_VERS, EEPROM_CURRENT_VERSION);
    EEPROM.commit();
    eeprom_save_settings();
  }
}

void updateSettingsFromPacket(updatesettings_t *s)
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_open_pos_[d] =  s->damper_open_pos[d];
  }
  eeprom_save_settings();
}

void updateInstalledDampersFromChar(uint8_t damper_installed)
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    damper_installed_[d] = 0 < (_BV(d) & damper_installed);
  }
  eeprom_save_settings();
}

uint8_t getInstalledDampersAsBitfield()
{
  uint8_t rv = 0;
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    if (damper_installed_[d])
      rv |= _BV(d);
  }
  return rv;
}
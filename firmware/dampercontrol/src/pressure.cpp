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

#ifdef BMPE280_ENABLED

#include <stdio.h>
#include "bmp280.h"
#include "dampercontrol.h"

bmp280_sensor psensor[NUM_DAMPER];
bmp280_result cur_temp_pressure[NUM_DAMPER];

uint8_t init_sensor_number(uint8_t d)
{
  switch (d)
  {
    default:
    case 0:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S0), PIN_CS_S0);
    case 1:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S1), PIN_CS_S1);
    case 2:
      return bmp280_init(&psensor[d], &PORTREG(REG_CS_S2), PIN_CS_S2);
  }
}

void pressure_sensors_init()
{
  //SPI chipselect Pins
  PINMODE_OUTPUT(REG_CS_S0,PIN_CS_S0);
  PINMODE_OUTPUT(REG_CS_S1,PIN_CS_S1);
  PINMODE_OUTPUT(REG_CS_S2,PIN_CS_S2);
  CS_SENSOR(0,HIGHv);
  CS_SENSOR(1,HIGHv);
  CS_SENSOR(2,HIGHv);
  SPI_Init(BMP280_LUFA_SPIO_OPTIONS);

  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    //TODO: try to initialize each sensor via SPI
    // set sensor_installed true on success
    sensor_installed_[d] = init_sensor_number(d);
  }
}

void task_check_pressure()
{
  for (uint8_t d=0; d<NUM_DAMPER; d++)
  {
    sensor_installed_[d] = ((sensor_installed_[d])? bmp280_check_chipid(&psensor[d]) : init_sensor_number(d));
    if (!sensor_installed_[d])
      continue;

    cur_temp_pressure[d] = bmp280_readTempAndPressure(&psensor[d]);
  }
}

float get_latest_pressure(uint8_t sensorid)
{
  return cur_temp_pressure[sensorid].pressure;
}

float get_latest_temperature(uint8_t sensorid)
{
  return cur_temp_pressure[sensorid].temperature;
}

#endif
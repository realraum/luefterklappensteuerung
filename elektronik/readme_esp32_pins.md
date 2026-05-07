

| Pin Name       | GPIO | Hardware Controlled                                          | Notes                                                        |
| :------------- | ---- | :----------------------------------------------------------- | :----------------------------------------------------------- |
| `Damper1`      | 21   | Damper DC Motor MOSFET (Q101)                                | PWM capable, Controls the first damper motor via MOSFET gate driver. |
| `Damper2`      | 22   | Damper DC Motor MOSFET (Q102)                                | PWM capable, Controls the second damper motor via MOSFET gate driver. |
| `Damper3`      | 23   | Damper DC Motor MOSFET (Q103) / Fan Control MOSFET           | PWM capable, Controls the third damper motor. This same pin is also used for fan control via the same MOSFET (Q103) [^1]. |
| Fan            | 33   | Fan on/off SSR                                               |                                                              |
| EndstopDamper1 | 17   |                                                              | pulls down when endstop-sync hit                             |
| EndstopDamper2 | 18   |                                                              | pulls down when endstop-sync hit                             |
| EndstopDamper3 | 19   |                                                              | pulls down when endstop-sync hit                             |
| `ONBOARD_LED`  | 2    | Onboard LED (D105)                                           | Drives the green onboard LED. The circuit is connected to +3V3 through a 110Ω resistor (R112), indicating it is active low (pin must be driven low to turn the LED on) [^1]. |
| `PS1_CS`       | 26   | Optical Endstop 1 (U101)                                     | Connected to the first optical endstop (Gabellichtschranke) [^1]. |
| `PS2_CS`       | 27   | Optical Endstop 2 (U102)                                     | Connected to the second optical endstop (Gabellichtschranke) [^1]. |
| `PS3_CS`       | 32   | Optical Endstop 3 (U103)                                     | Connected to the third optical endstop (Gabellichtschranke) [^1]. |
| `MISO`         | 12   | SPI Pressure Sensors (P103, P105, P104) / Ethernet Daughter Board | Master In Slave Out line for SPI communication with all three pressure sensors and the Ethernet daughter board (J103) [^1]. |
| `MOSI`         | 13   | SPI Pressure Sensors (P103, P105, P104) / Ethernet Daughter Board | Master Out Slave In line for SPI communication [^1].         |
| `SCK`          | 14   | SPI Pressure Sensors (P103, P105, P104) / Ethernet Daughter Board | Serial Clock line for SPI communication [^1].                |
| `LAN_CS`       | 15   | Ethernet Daughter Board (J103)                               | Chip Select for the Ethernet daughter board.                 |
| `LAN_RESET`    | 16   | Ethernet Daughter Board (J103)                               | Reset signal for the Ethernet daughter board.                |
| `LAN_CLKOUT`   | -    | Ethernet Daughter Board (J103)                               | Clock output for the Ethernet daughter board.                |
| `LAN_INT`      | -    | Ethernet Daughter Board (J103)                               | Interrupt signal from the Ethernet daughter board.           |
| `LAN_WOL`      | -    | Ethernet Daughter Board (J103)                               | Wake-on-LAN signal for the Ethernet daughter board.          |
| `PJON`         | 25   | PJON Communication Line                                      | Dedicated pin for the PJON (Padded Jittering Operative Network) serial communication protocol [^1]. |


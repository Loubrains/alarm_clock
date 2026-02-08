# alarm_clock

Alarm clock firmware built with the Raspberry Pi Pico SDK (C++).

## Hardware

- Raspberry Pi Pico 2
- DS3231 RTC Module (I2C)
- SSD1309 OLED (SPI)
- DFPlayer Mini (Uart)

## Attribution

- **RTC Driver:** Based on [Adafruit RTClib](https://github.com/adafruit/RTClib) (MIT), ported from Arduino to Pico SDK
- **Display Driver:** Based on [pico-ssd1306](https://github.com/daschr/pico-ssd1306) by David Schramm (MIT), ported from I2C to SPI and adapted for SSD1309
- **DFPlayer Driver:** Based on [DFRobotDFPlayerMini](https://github.com/DFRobot/DFRobotDFPlayerMini) by Angelo Qiao (LGPL), ported from Arduino to Pico SDK.

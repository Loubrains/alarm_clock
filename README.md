# alarm_clock

Alarm clock firmware for the Raspberry Pi Pico 2, built with the Raspberry Pi Pico SDK (C++).

## Hardware

- Raspberry Pi Pico 2
- DS3231 RTC Module
- SSD1309 OLED (128x64, SPI)
- DFPlayer Mini

## Attribution

- **RTC Driver:** Based on [Adafruit RTClib](https://github.com/adafruit/RTClib) (MIT), ported from Arduino to Pico SDK
- **Display Driver:** Based on [pico-ssd1306](https://github.com/daschr/pico-ssd1306) by David Schramm (MIT), ported from I2C to SPI and adapted for SSD1309

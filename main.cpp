#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/sync.h"

#include <RTClib.h>
extern "C"
{
#include "ssd1309.h"
}

#define RTC_SDA_PIN 26
#define RTC_SCL_PIN 27
#define RTC_INT_PIN 22

#define OLED_CLK_PIN 2
#define OLED_DIN_PIN 3
#define OLED_CS_PIN 5
#define OLED_DC_PIN 6
#define OLED_RST_PIN 7

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

i2c_inst_t *i2c = i2c1;
spi_inst_t *spi = spi0;
RTC_DS3231 rtc;

DateTime DEFAULT_DATETIME = DateTime(2000, 1, 1, 0, 0, 0); // 2000-01-01 00:00:00

// void setupInterrupts()
// {
//     gpio_set_dormant_irq_enabled(RTC_INT_PIN, GPIO_IRQ_EDGE_FALL, true);
// }

void programAlarmTest()
{
    rtc.disableAlarm(1);
    rtc.clearAlarm(1);

    DateTime now = rtc.now();
    DateTime alarm(
        now.year(), now.month(), now.day(),
        now.hour(), now.minute() + 1, 0);

    rtc.setAlarm1(alarm, DS3231_A1_Hour);
}

int displayTest(ssd1309_t &display)
{
    // Clear the display
    ssd1309_clear(&display);

    // Draw some text
    ssd1309_draw_string(&display, 0, 0, 2, "SPI Test");
    ssd1309_draw_string(&display, 0, 20, 1, "Hello World!");
    ssd1309_draw_string(&display, 0, 30, 1, "Pico + SSD1309");

    // Draw some shapes
    ssd1309_draw_line(&display, 0, 50, 127, 50);
    ssd1309_draw_empty_square(&display, 10, 52, 20, 10);
    ssd1309_draw_square(&display, 35, 52, 20, 10);

    // Update the display
    ssd1309_show(&display);

    printf("Display updated\n");

    // Animation loop
    int x = 0;
    int direction = 1;

    while (true)
    {
        // Clear a strip where the animation happens
        ssd1309_clear_square(&display, 0, 40, 128, 8);

        // Draw moving pixel
        ssd1309_draw_square(&display, x, 42, 4, 4);

        // Update display
        ssd1309_show(&display);

        // Move position
        x += direction * 2;
        if (x >= 124 || x <= 0)
        {
            direction *= -1;
        }

        sleep_ms(20);
    }

    // Cleanup (never reached in this example)
    ssd1309_deinit(&display);

    return 0;
}

int main()
{
    stdio_init_all();
    sleep_ms(2000); // Wait for USB to initialize

    i2c_init(i2c1, 100 * 1000);                    // 100 kHz
    gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C); // SDA pin
    gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C); // SCL pin
    gpio_pull_up(RTC_SDA_PIN);
    gpio_pull_up(RTC_SCL_PIN);

    gpio_init(RTC_INT_PIN);
    gpio_set_dir(RTC_INT_PIN, GPIO_IN);
    gpio_pull_up(RTC_INT_PIN);

    if (!rtc.begin(i2c1))
    {
        printf("Failed to initialize RTC!\n");
        return -1;
    }
    // Adjust RTC time to compile time
    // rtc.adjust(DateTime(__DATE__, __TIME__));
    if (rtc.lostPower())
    {
        printf("RTC lost power, setting time to %04d-%02d-%02d %02d:%02d:%02d.\n",
               DEFAULT_DATETIME.year(), DEFAULT_DATETIME.month(), DEFAULT_DATETIME.day(),
               DEFAULT_DATETIME.hour(), DEFAULT_DATETIME.minute(), DEFAULT_DATETIME.second());
        rtc.adjust(DEFAULT_DATETIME);
    }

    spi_init(spi0, 10 * 1000 * 1000); // 10 MHz
    gpio_set_function(OLED_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(OLED_DIN_PIN, GPIO_FUNC_SPI);

    ssd1309_t display;
    if (!ssd1309_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                      spi0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN))
    {
        printf("Display initialization failed!\n");
        return -1;
    }
    ssd1309_clear(&display);

    programAlarmTest();

    while (true)
    {
        bool rtc_wake = gpio_get(RTC_INT_PIN) == 0;

        static DateTime last_time = DEFAULT_DATETIME;
        DateTime now = rtc.now();
        if (now.second() != last_time.second())
        {
            last_time = now;
            ssd1309_clear(&display);
            char time_str[20];
            snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
            ssd1309_draw_string(&display, 0, 0, 2, time_str);
            ssd1309_show(&display);
        }

        if (rtc_wake)
        {
            ssd1309_draw_string(&display, 0, 20, 1, "ALARM!");
            ssd1309_show(&display);
        }

        // setupInterrupts();
        // __wfi();
    }
    return 0;
}

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/sync.h"

#include <RTClib.h>

#define RTC_SDA_PIN 26
#define RTC_SCL_PIN 27
#define RTC_INT_PIN 22

i2c_inst_t *i2c = i2c1;
RTC_DS3231 rtc;

DateTime DEFAULT_DATETIME = DateTime(2000, 1, 1, 0, 0, 0); // 2000-01-01 00:00:00

void initGPIO()
{
    gpio_init(RTC_INT_PIN);
    gpio_set_dir(RTC_INT_PIN, GPIO_IN);
    gpio_pull_up(RTC_INT_PIN);
}

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

int main()
{
    stdio_init_all();

    initGPIO();

    i2c_init(i2c1, 100 * 1000);                    // 100 kHz
    gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C); // SDA pin
    gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C); // SCL pin
    gpio_pull_up(RTC_SDA_PIN);
    gpio_pull_up(RTC_SCL_PIN);

    rtc.begin(i2c1);
    // Adjust RTC time to compile time
    // rtc.adjust(DateTime(__DATE__, __TIME__));
    if (rtc.lostPower())
    {
        rtc.adjust(DEFAULT_DATETIME);
    }
    programAlarmTest();

    while (true)
    {
        bool rtc_wake = gpio_get(RTC_INT_PIN) == 0;

        static DateTime last_time = DEFAULT_DATETIME;
        DateTime now = rtc.now();
        if (now.second() != last_time.second())
        {
            last_time = now;
            printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
        }

        if (rtc_wake)
        {
            printf("ALARM!\n");
            programAlarmTest();
            rtc_wake = false;
        }

        // setupInterrupts();
        // __wfi();
    }
    return 0;
}

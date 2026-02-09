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
#include "test_image.h"
}
#include "DFRobotDFPlayerMini.h"

#define RTC_SDA_PIN 26
#define RTC_SCL_PIN 27
#define RTC_INT_PIN 22
#define RTC_BAUDRATE 100 * 1000 // 100 kHz

#define DISP_CLK_PIN 2
#define DISP_DIN_PIN 3
#define DISP_CS_PIN 5
#define DISP_DC_PIN 6
#define DISP_RST_PIN 7
#define DISP_BAUDRATE 10 * 1000 * 1000 // 10 MHz
#define DISP_WIDTH 128
#define DISP_HEIGHT 64

#define DFPLAYER_TX_PIN 12
#define DFPLAYER_RX_PIN 13
#define DFPLAYER_BAUDRATE 9600

i2c_inst_t *_i2c1 = i2c1;
spi_inst_t *_spi0 = spi0;
uart_inst_t *_uart0 = uart0;

RTC_DS3231 rtc;
ssd1309_t display;
DFRobotDFPlayerMini player;

const char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
DateTime DEFAULT_DATETIME = DateTime(2000, 1, 1, 0, 0, 0); // 2000-01-01 00:00:00

int main()
{
    stdio_init_all();
    sleep_ms(5000); // Wait for USB to initialize

    // --- Display Initialization ---
    spi_init(_spi0, DISP_BAUDRATE);
    gpio_set_function(DISP_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISP_DIN_PIN, GPIO_FUNC_SPI);

    if (!ssd1309_init(&display, DISP_WIDTH, DISP_HEIGHT,
                      _spi0, DISP_CS_PIN, DISP_DC_PIN, DISP_RST_PIN))
    {
        printf("Failed to initialize display!\n");
        return -1;
    }
    ssd1309_clear(&display);
    ssd1309_bmp_show_image(&display, image_data, image_size);
    ssd1309_show(&display);

    // --- RTC Initialization ---
    i2c_init(_i2c1, RTC_BAUDRATE);
    gpio_set_function(RTC_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(RTC_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(RTC_SDA_PIN);
    gpio_pull_up(RTC_SCL_PIN);
    gpio_init(RTC_INT_PIN);
    gpio_set_dir(RTC_INT_PIN, GPIO_IN);
    gpio_pull_up(RTC_INT_PIN);

    if (!rtc.begin(_i2c1))
    {
        printf("Failed to initialize RTC!\n");
        ssd1309_clear(&display);
        ssd1309_draw_string(&display, 0, 0, 1, "RTC init failed!");
        ssd1309_show(&display);
        return -1;
    }
    // Adjust RTC time to compile time
    // rtc.adjust(DateTime(__DATE__, __TIME__));
    if (rtc.lostPower())
    {
        printf("RTC lost power, setting time to %04d-%02d-%02d %02d:%02d:%02d.\n",
               DEFAULT_DATETIME.year(), DEFAULT_DATETIME.month(), DEFAULT_DATETIME.day(),
               DEFAULT_DATETIME.hour(), DEFAULT_DATETIME.minute(), DEFAULT_DATETIME.second());
        ssd1309_clear(&display);
        ssd1309_draw_string(&display, 0, 0, 1, "RTC lost power!");
        ssd1309_show(&display);
        rtc.adjust(DEFAULT_DATETIME);
        sleep_ms(2000);
    }

    // --- DFPlayer Initialization ---
    uart_init(_uart0, DFPLAYER_BAUDRATE);
    gpio_set_function(DFPLAYER_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DFPLAYER_RX_PIN, GPIO_FUNC_UART);

    // Flush UART buffer
    while (uart_is_readable(_uart0))
    {
        uart_getc(_uart0);
    }

    if (!player.begin(_uart0))
    {
        printf("Failed to initialize DFPlayer!\n");
        ssd1309_clear(&display);
        ssd1309_draw_string(&display, 0, 0, 1, "DFPlayer init failed!");
        ssd1309_show(&display);
        return -1;
    }
    player.volume(15);

    // Final init
    ssd1309_clear(&display);
    ssd1309_show(&display);

    // --- Main Loop ---
    while (true)
    {
        bool rtc_wake = gpio_get(RTC_INT_PIN) == 0;

        if (player.available())
        {
            player.printDetail(player.readType(), player.read());
        }

        // Display current time
        static DateTime last_time = DEFAULT_DATETIME;
        DateTime now = rtc.now();
        if (now.minute() != last_time.minute())
        {
            last_time = now;
            ssd1309_clear(&display);
            char time_str[16];
            snprintf(time_str, sizeof(time_str), "%02d:%02d", now.hour(), now.minute());
            ssd1309_draw_string(&display, 20, 24, 3, time_str);
            char date_str[24];
            snprintf(date_str, sizeof(date_str), "%s %d %s",
                     DAY_NAMES[now.dayOfTheWeek()],
                     now.day(),
                     MONTH_NAMES[now.month() - 1]);
            ssd1309_draw_string(&display, 20, 50, 1, date_str);
            ssd1309_show(&display);
        }

        // Handle alarm
        if (rtc_wake)
        {
            ssd1309_draw_square(&display, 20, 15, 80, 5);
            ssd1309_show(&display);
            player.play(1);
            rtc.clearAlarm(1);
        }

        // Wait for interrupts
        // gpio_set_dormant_irq_enabled(RTC_INT_PIN, GPIO_IRQ_EDGE_FALL, true);
        // __wfi();
    }
    return 0;
}

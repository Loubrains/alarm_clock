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

// void setupInterrupts()
// {
//     gpio_set_dormant_irq_enabled(RTC_INT_PIN, GPIO_IRQ_EDGE_FALL, true);
// }

void alarmTest()
{
    rtc.disableAlarm(1);
    rtc.clearAlarm(1);
    DateTime now = rtc.now();
    DateTime alarm = now + TimeSpan(0, 0, 0, 10);
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

void printDetail(uint8_t type, int value)
{
    switch (type)
    {
    case TimeOut:
        printf("Time Out!\n");
        break;
    case WrongStack:
        printf("Stack Wrong!\n");
        break;
    case DFPlayerCardInserted:
        printf("Card Inserted!\n");
        break;
    case DFPlayerCardRemoved:
        printf("Card Removed!\n");
        break;
    case DFPlayerCardOnline:
        printf("Card Online!\n");
        break;
    case DFPlayerUSBInserted:
        printf("USB Inserted!\n");
        break;
    case DFPlayerUSBRemoved:
        printf("USB Removed!\n");
        break;
    case DFPlayerPlayFinished:
        printf("Number: %d Play Finished!\n", value);
        break;
    case DFPlayerError:
        printf("DFPlayerError: ");
        switch (value)
        {
        case Busy:
            printf("Card not found\n");
            break;
        case Sleeping:
            printf("Sleeping\n");
            break;
        case SerialWrongStack:
            printf("Get Wrong Stack\n");
            break;
        case CheckSumNotMatch:
            printf("Check Sum Not Match\n");
            break;
        case FileIndexOut:
            printf("File Index Out of Bound\n");
            break;
        case FileMismatch:
            printf("Cannot Find File\n");
            break;
        case Advertise:
            printf("In Advertise\n");
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

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
        ssd1309_draw_string(&display, 0, 0, 1, "RTC lost power!");
        ssd1309_show(&display);
        rtc.adjust(DEFAULT_DATETIME);
        sleep_ms(2000);
    }

    // --- DFPlayer Initialization ---
    uart_init(_uart0, DFPLAYER_BAUDRATE);
    gpio_set_function(DFPLAYER_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(DFPLAYER_TX_PIN, GPIO_FUNC_UART);

    if (!player.begin(_uart0))
    {
        printf("Failed to initialize DFPlayer!\n");
        ssd1309_draw_string(&display, 0, 0, 1, "DFPlayer init failed!");
        ssd1309_show(&display);
        return -1;
    }
    player.volume(15);

    // --- Final initialization ---
    alarmTest();

    // --- Main Loop ---
    while (true)
    {
        bool rtc_wake = gpio_get(RTC_INT_PIN) == 0;

        if (player.available())
        {
            printDetail(player.readType(), player.read());
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
            ssd1309_draw_string(&display, 17, 24, 3, time_str);
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
            ssd1309_draw_line(&display, 20, 19, 100, 19);
            ssd1309_draw_line(&display, 20, 20, 100, 20);
            ssd1309_show(&display);
            player.play(1);
            rtc.clearAlarm(1);
        }

        // setupInterrupts();
        // __wfi();
    }
    return 0;
}

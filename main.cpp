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

#define BTN_UP_PIN 20
#define BTN_DOWN_PIN 19
#define BTN_SELECT_PIN 18

#define DISPLAY_TIMEOUT_S 20
#define BUTTON_DEBOUNCE_MS 200

i2c_inst_t *_i2c1 = i2c1;
spi_inst_t *_spi0 = spi0;
uart_inst_t *_uart0 = uart0;

RTC_DS3231 rtc;
ssd1309_t display;
DFRobotDFPlayerMini player;

enum State
{
    STATE_CLOCK,        // Main clock display
    STATE_MENU,         // Menu mode
    STATE_SET_ALARM,    // Setting alarm time
    STATE_SET_TIME,     // Setting clock time
    STATE_ALARM_RINGING // Alarm is ringing
};
enum MenuOption
{
    // TODO: VOLUME STATE?

    MENU_SET_ALARM,
    MENU_SET_TIME,
    MENU_EXIT,
    MENU_COUNT
};
enum TimeSetting
{
    TIME_HOUR,
    TIME_MINUTE,
    // TIME_SECOND,
    // TIME_DAY,
    // TIME_MONTH,
    // TIME_YEAR
};

const char *DAY_NAMES[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *MONTH_NAMES[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                             "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
DateTime DEFAULT_DATETIME = DateTime(2000, 1, 1, 0, 0, 0); // 2000-01-01 00:00:00

State current_state = STATE_CLOCK;
MenuOption current_menu_option = MENU_SET_ALARM;
TimeSetting edit_time_field = TIME_HOUR;
volatile bool rtc_interrupt_fired = false;
volatile bool button_up_pressed = false;
volatile bool button_down_pressed = false;
volatile bool button_select_pressed = false;
bool display_dirty = false; // Flag to indicate that display needs to be updated
bool display_on = true;
bool player_on = true;
bool alarm_enabled = false;
uint8_t alarm_hour = 7;
uint8_t alarm_minute = 0;
uint8_t time_setting_hour = 7;
uint8_t time_setting_minute = 0;
uint8_t current_volume = 15;
uint64_t last_activity_time = 0;
DateTime current_time = DEFAULT_DATETIME;

bool initDisplay()
{
    spi_init(_spi0, DISP_BAUDRATE);
    gpio_set_function(DISP_CLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(DISP_DIN_PIN, GPIO_FUNC_SPI);

    if (!ssd1309_init(&display, DISP_WIDTH, DISP_HEIGHT,
                      _spi0, DISP_CS_PIN, DISP_DC_PIN, DISP_RST_PIN))
    {
        printf("Failed to initialize display!\n");
        return false;
    }
    ssd1309_clear(&display);
    ssd1309_bmp_show_image(&display, image_data, image_size);
    ssd1309_show(&display);
    return true;
}

bool initRTC()
{
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
        return false;
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

    // get alarm if set
    if (rtc.getAlarmEnabled(1))
    {
        DateTime alarm_time = rtc.getAlarm1();
        alarm_hour = alarm_time.hour();
        alarm_minute = alarm_time.minute();
        alarm_enabled = true;
    }

    return true;
}

bool initPlayer()
{
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
        return false;
    }
    player.volume(current_volume);
    return true;
}

void initButtons()
{
    gpio_init(BTN_UP_PIN);
    gpio_set_dir(BTN_UP_PIN, GPIO_IN);
    gpio_pull_up(BTN_UP_PIN);

    gpio_init(BTN_DOWN_PIN);
    gpio_set_dir(BTN_DOWN_PIN, GPIO_IN);
    gpio_pull_up(BTN_DOWN_PIN);

    gpio_init(BTN_SELECT_PIN);
    gpio_set_dir(BTN_SELECT_PIN, GPIO_IN);
    gpio_pull_up(BTN_SELECT_PIN);
}

void interruptHandler(uint gpio, uint32_t events)
{
    switch (gpio)
    {
    case RTC_INT_PIN:
        rtc_interrupt_fired = true;
        break;
    case BTN_UP_PIN:
        button_up_pressed = true;
        break;
    case BTN_DOWN_PIN:
        button_down_pressed = true;
        break;
    case BTN_SELECT_PIN:
        button_select_pressed = true;
        break;
    }
}

void initInterrupts()
{
    gpio_set_irq_enabled_with_callback(RTC_INT_PIN, GPIO_IRQ_EDGE_FALL, true, &interruptHandler);
    gpio_set_irq_enabled_with_callback(BTN_UP_PIN, GPIO_IRQ_EDGE_FALL, true, &interruptHandler);
    gpio_set_irq_enabled_with_callback(BTN_DOWN_PIN, GPIO_IRQ_EDGE_FALL, true, &interruptHandler);
    gpio_set_irq_enabled_with_callback(BTN_SELECT_PIN, GPIO_IRQ_EDGE_FALL, true, &interruptHandler);
}

void powerDownPeripherals()
{
    if (display_on == true)
    {
        ssd1309_poweroff(&display);
        display_on = false;
    }
    if (player_on == true)
    {
        // Flush UART buffer
        // while (uart_is_readable(_uart0))
        // {
        //     uart_getc(_uart0);
        // }
        // player.sleep();
        // player_on = false;
    }
}

void powerUpPeripherals()
{
    if (display_on == false)
    {
        ssd1309_poweron(&display);
        display_on = true;
    }
    if (player_on == false)
    {
        // Flush UART buffer
        // while (uart_is_readable(_uart0))
        // {
        //     uart_getc(_uart0);
        // }
        // player.start();
        // sleep_ms(1000);
        // player.volume(current_volume);
        // player_on = true;
    }
}

void drawClock(DateTime &now)
{
    // Draw time
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", now.hour(), now.minute());
    ssd1309_draw_string(&display, 20, 24, 3, time_str);

    // Draw date
    char date_str[24];
    snprintf(date_str, sizeof(date_str), "%s %d %s",
             DAY_NAMES[now.dayOfTheWeek()],
             now.day(),
             MONTH_NAMES[now.month() - 1]);
    ssd1309_draw_string(&display, 20, 50, 1, date_str);

    // Show alarm indicator if enabled
    if (alarm_enabled)
    {
        char alarm_str[16];
        snprintf(alarm_str, sizeof(alarm_str), "<> %02d:%02d", alarm_hour, alarm_minute);
        ssd1309_draw_string(&display, 70, 0, 1, alarm_str);
    }

    display_dirty = true;
}

void drawAlarmIndicator()
{
    // Flashing indicator
    static bool flash = false;
    flash = !flash;
    if (flash)
    {
        ssd1309_draw_square(&display, 20, 13, 87, 5);
    }
    else
    {
        ssd1309_clear_square(&display, 20, 13, 87, 5);
    }
    display_dirty = true;
}

void drawMenu()
{
    // Draw title
    ssd1309_draw_string(&display, 50, 0, 1, "MENU");

    const char *menu_items[] = {
        "Set Alarm",
        "Set Time",
        "Exit"};

    // Draw menu options
    for (int i = 0; i < MENU_COUNT; i++)
    {
        int y = 15 + (i * 12);
        if (i == current_menu_option)
        {
            // Draw selection indicator
            ssd1309_draw_string(&display, 5, y, 1, ">");
        }
        ssd1309_draw_string(&display, 15, y, 1, menu_items[i]);
    }

    display_dirty = true;
}

void drawSetAlarm()
{
    // Draw title
    ssd1309_draw_string(&display, 60, 0, 1, "Set Alarm");

    // Draw hour and minute
    char alarm_str[16];
    snprintf(alarm_str, sizeof(alarm_str), "%02d:%02d", alarm_hour, alarm_minute);
    ssd1309_draw_string(&display, 20, 24, 3, alarm_str);

    // Draw selection indicator
    if (edit_time_field == TIME_HOUR)
    {
        ssd1309_draw_square(&display, 20, 50, 28, 2);
    }
    else if (edit_time_field == TIME_MINUTE)
    {
        ssd1309_draw_square(&display, 75, 50, 28, 2);
    }

    display_dirty = true;
}

void drawSetTime()
{
    // TODO: SET DATE

    // Draw title
    ssd1309_draw_string(&display, 60, 0, 1, "Set Time");

    // Draw hour and minute
    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d", time_setting_hour, time_setting_minute);
    ssd1309_draw_string(&display, 20, 24, 3, time_str);

    // Draw selection indicator
    if (edit_time_field == TIME_HOUR)
    {
        ssd1309_draw_square(&display, 20, 50, 28, 2);
    }
    else if (edit_time_field == TIME_MINUTE)
    {
        ssd1309_draw_square(&display, 75, 50, 28, 2);
    }

    display_dirty = true;
}

void programAlarm(const DateTime &alarm_time)
{
    rtc.disableAlarm(1);
    rtc.clearAlarm(1);
    rtc.setAlarm1(alarm_time, DS3231_A1_Hour);
    alarm_enabled = true;
}

// void snoozeAlarm()
// {
//     DateTime current_time = rtc.now();
//     DateTime snooze_time = current_time + TimeSpan(0, 0, 5, 0); // days, hours, minutes, seconds
//     programAlarm(snooze_time);
// }

void resetActivity()
{

    last_activity_time = time_us_64();
    powerUpPeripherals();
}

void handleAlarmFired()
{
    resetActivity();
    current_state = STATE_ALARM_RINGING;
    player.play(1);
    rtc.clearAlarm(1);
}

void handleButtonUp()
{
    // Debounce
    static uint64_t last_handled_time = 0;
    uint64_t now = time_us_64();
    if (now - last_handled_time < BUTTON_DEBOUNCE_MS * 1000)
    {
        return;
    }
    last_handled_time = now;

    resetActivity();

    switch (current_state)
    {
    case STATE_CLOCK:
        player.volumeUp();
        current_volume += 1;
        break;

    case STATE_MENU:
        // Move up in menu
        current_menu_option = (MenuOption)((current_menu_option - 1 + MENU_COUNT) % MENU_COUNT);
        ssd1309_clear(&display);
        drawMenu();
        break;

    case STATE_SET_TIME:
        // Increment current time setting field
        if (edit_time_field == TIME_HOUR)
        {
            time_setting_hour = (time_setting_hour + 1 + 24) % 24;
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            time_setting_minute = (time_setting_minute + 1 + 60) % 60;
        }
        ssd1309_clear(&display);
        drawSetTime();
        break;

    case STATE_SET_ALARM:
        // Increment current alarm setting field
        if (edit_time_field == TIME_HOUR)
        {
            alarm_hour = (alarm_hour + 1 + 24) % 24;
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            alarm_minute = (alarm_minute + 1 + 60) % 60;
        }
        ssd1309_clear(&display);
        drawSetAlarm();
        break;

    case STATE_ALARM_RINGING:
        // Stop alarm
        player.stop();
        rtc.clearAlarm(1);
        current_state = STATE_CLOCK;
        ssd1309_clear(&display);
        current_time = rtc.now();
        drawClock(current_time);
        break;
    }
}

void handleButtonDown()
{
    // Debounce
    static uint64_t last_handled_time = 0;
    uint64_t now = time_us_64();
    if (now - last_handled_time < BUTTON_DEBOUNCE_MS * 1000)
    {
        return;
    }
    last_handled_time = now;

    resetActivity();

    switch (current_state)
    {
    case STATE_CLOCK:
        player.volumeDown();
        current_volume -= 1;
        break;

    case STATE_MENU:
        // Move down in menu
        current_menu_option = (MenuOption)((current_menu_option + 1 + MENU_COUNT) % MENU_COUNT);
        ssd1309_clear(&display);
        drawMenu();
        break;

    case STATE_SET_ALARM:
        // Increment current alarm setting field
        if (edit_time_field == TIME_HOUR)
        {
            alarm_hour = (alarm_hour - 1 + 24) % 24;
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            alarm_minute = (alarm_minute - 1 + 60) % 60;
        }
        ssd1309_clear(&display);
        drawSetAlarm();
        break;

    case STATE_SET_TIME:
        // Increment current time setting field
        if (edit_time_field == TIME_HOUR)
        {
            time_setting_hour = (time_setting_hour - 1 + 24) % 24;
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            time_setting_minute = (time_setting_minute - 1 + 60) % 60;
        }
        ssd1309_clear(&display);
        drawSetTime();
        break;

    case STATE_ALARM_RINGING:
        // Snooze for 5 minutes
        // snoozeAlarm(5);
        break;
    }
}

void handleButtonSelect()
{
    // Debounce
    static uint64_t last_handled_time = 0;
    uint64_t now = time_us_64();
    if (now - last_handled_time < BUTTON_DEBOUNCE_MS * 1000)
    {
        return;
    }
    last_handled_time = now;

    resetActivity();

    switch (current_state)
    {
    case STATE_CLOCK:
        // Enter menu
        current_state = STATE_MENU;
        current_menu_option = MENU_SET_ALARM;
        ssd1309_clear(&display);
        drawMenu();
        break;

    case STATE_MENU:
        // Execute menu item
        switch (current_menu_option)
        {
        case MENU_SET_ALARM:
            current_state = STATE_SET_ALARM;
            edit_time_field = TIME_HOUR;
            ssd1309_clear(&display);
            drawSetAlarm();
            break;

        case MENU_SET_TIME:
            current_time = rtc.now();
            time_setting_hour = current_time.hour();
            time_setting_minute = current_time.minute();
            current_state = STATE_SET_TIME;
            edit_time_field = TIME_HOUR;
            ssd1309_clear(&display);
            drawSetTime();
            break;

        case MENU_EXIT:
            current_state = STATE_CLOCK;
            ssd1309_clear(&display);
            current_time = rtc.now();
            drawClock(current_time);
            break;

        default:
            break;
        }
        break;

    case STATE_SET_ALARM:
        if (edit_time_field == TIME_HOUR)
        {
            // Move to next field
            edit_time_field = TIME_MINUTE;
            ssd1309_clear(&display);
            drawSetAlarm();
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            // Save and exit
            programAlarm(DateTime(2000, 1, 1, alarm_hour, alarm_minute, 0));
            current_state = STATE_CLOCK;
            current_time = rtc.now();
            ssd1309_clear(&display);
            drawClock(current_time);
        }
        break;

    case STATE_SET_TIME:
        if (edit_time_field == TIME_HOUR)
        {
            // Move to next field
            edit_time_field = TIME_MINUTE;
            ssd1309_clear(&display);
            drawSetTime();
        }
        else if (edit_time_field == TIME_MINUTE)
        {
            // Save and exit
            rtc.adjust(DateTime(current_time.year(), current_time.month(), current_time.day(), time_setting_hour, time_setting_minute, 0));
            current_state = STATE_CLOCK;
            current_time = rtc.now();
            ssd1309_clear(&display);
            drawClock(current_time);
        }
        break;

    case STATE_ALARM_RINGING:
        // Stop alarm completely
        player.stop();
        rtc.clearAlarm(1);
        current_state = STATE_CLOCK;
        current_time = rtc.now();
        ssd1309_clear(&display);
        drawClock(current_time);
        break;
    }
}

int main()
{
    // --- Setup ---
    stdio_init_all();
    sleep_ms(5000); // Wait for USB to initialize

    if (!initDisplay())
    {
        return -1;
    }
    if (!initRTC())
    {
        return -1;
    }
    if (!initPlayer())
    {
        return -1;
    }
    initButtons();
    initInterrupts();

    ssd1309_clear(&display);
    ssd1309_show(&display);

    last_activity_time = time_us_64();

    // --- Main Loop ---
    while (true)
    {
        // Handle interrupts
        if (rtc_interrupt_fired)
        {
            rtc_interrupt_fired = false;
            handleAlarmFired();
        }
        if (button_up_pressed)
        {
            button_up_pressed = false;
            handleButtonUp();
        }
        if (button_down_pressed)
        {
            button_down_pressed = false;
            handleButtonDown();
        }
        if (button_select_pressed)
        {
            button_select_pressed = false;
            handleButtonSelect();
        }

        // Handle clock state
        static DateTime last_time = DEFAULT_DATETIME;
        current_time = rtc.now();
        if ((current_state == STATE_CLOCK || current_state == STATE_ALARM_RINGING) &&
            current_time.minute() != last_time.minute())
        {
            last_time = current_time;
            ssd1309_clear(&display);
            drawClock(current_time);
        }

        // Handle alarm state
        if (current_state == STATE_ALARM_RINGING)
        {
            // Flashing alarm indicator
            static uint64_t last_flash = 0;
            if (time_us_64() - last_flash > 500000) // 500ms
            {
                last_flash = time_us_64();
                drawAlarmIndicator();
            }

            // Loop the alarm
            if (player.available())
            {
                uint8_t type = player.readType();
                uint16_t value = player.read();

                if (type == DFPlayerPlayFinished)
                {
                    player.play(1);
                }
            }
        }

        // Update display
        if (display_dirty)
        {
            ssd1309_show(&display);
            display_dirty = false;
        }

        // Timeout
        if (current_state != STATE_ALARM_RINGING &&
            display_on &&
            time_us_64() - last_activity_time > (DISPLAY_TIMEOUT_S * 1000000ULL))
        {
            current_state = STATE_CLOCK;
            // ssd1309_clear(&display);
            // ssd1309_show(&display);
            powerDownPeripherals();
            __wfi();
        }
        // else
        // {
        //     sleep_ms(50);
        // }
    }
    return 0;
}

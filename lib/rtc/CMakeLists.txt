add_library(rtc_ds3231 STATIC
    RTClib.cpp
    RTC_DS3231.cpp
)

target_include_directories(rtc_ds3231 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(rtc_ds3231
    pico_stdlib
    hardware_i2c
)
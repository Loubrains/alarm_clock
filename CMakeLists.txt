# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(alarm_clock C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add any subdirectories with CMakeLists.txt files (such as libraries)
add_subdirectory(lib/rtc)
add_subdirectory(lib/ssd1309)
add_subdirectory(lib/dfplayer)

# Add executable. Default name is the project name, version 0.1
add_executable(alarm_clock
    main.cpp
)

pico_set_program_name(alarm_clock "alarm_clock")
pico_set_program_version(alarm_clock "0.1")


# Add the standard library to the build
target_link_libraries(alarm_clock
    pico_stdlib
    hardware_i2c
    hardware_spi
    rtc_ds3231
    ssd1309
    dfplayer
)

# Add the standard include files to the build
target_include_directories(alarm_clock PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(alarm_clock 1)
pico_enable_stdio_uart(alarm_clock 0)

# Create .uf2 and .hex files etc.
pico_add_extra_outputs(alarm_clock)


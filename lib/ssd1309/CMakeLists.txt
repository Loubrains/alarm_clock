add_library(ssd1309 STATIC
    ssd1309.c
)

target_include_directories(ssd1309 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(ssd1309
    pico_stdlib
    hardware_spi
)
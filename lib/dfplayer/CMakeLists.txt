add_library(dfplayer STATIC
    DFRobotDFPlayerMini.cpp
)

target_include_directories(dfplayer PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(dfplayer
    pico_stdlib
    hardware_uart
)
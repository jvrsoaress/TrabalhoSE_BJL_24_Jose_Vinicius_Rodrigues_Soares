cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)
project(RFID_multicore C CXX ASM) # Renomeei o projeto para clareza
pico_sdk_init()

add_executable(${PROJECT_NAME}
    main.c
    lib/mfrc522.c
    lib/ssd1306.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    lib
)

target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_adc     # <--- ADICIONADO PARA O JOYSTICK [cite: 549]
    pico_multicore   # <--- ADICIONADO PARA O SEGUNDO NÚCLEO [cite: 275, 390]
)

pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_add_extra_outputs(${PROJECT_NAME})
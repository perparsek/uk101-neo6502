cmake_minimum_required(VERSION 3.13)

# Beroendena ligger i vendor\ och klonas av tools\setup-pico.ps1.
set(PICO_SDK_PATH   "${CMAKE_CURRENT_LIST_DIR}/../vendor/pico-sdk"  CACHE PATH "")
set(PICODVI_PATH    "${CMAKE_CURRENT_LIST_DIR}/../vendor/PicoDVI"   CACHE PATH "")

set(PICO_BOARD pico CACHE STRING "")
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(uk101_neo6502 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(uk101
    main.c
    ../src/uk101.c
)

pico_generate_pio_header(uk101 ${CMAKE_CURRENT_LIST_DIR}/uk101_bus.pio)

# libdvi kommer ur PicoDVI som INTERFACE-bibliotek, så flaggorna nedan sätts
# per bygge och inte i biblioteket.
add_subdirectory(${PICODVI_PATH}/software/libdvi libdvi)

target_include_directories(uk101 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../src
    ${PICODVI_PATH}/software/include
)

target_compile_definitions(uk101 PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=pico_sock_cfg   # skrivs över i koden med Neo6502:ans pinout
    DVI_VERTICAL_REPEAT=2                     # 640x240 bildminne, tecken 8x16 pa skarm
    DVI_N_TMDS_BUFFERS=3
    DVI_MONOCHROME_TMDS=1                     # UK101 var vitt pa svart
    DVI_1BPP_BIT_REVERSE=1                    # bit 7 = vanster pixel, som teckengeneratorn
    PICO_NEO6502=1
)

target_link_libraries(uk101
    pico_stdlib
    pico_multicore
    pico_util
    hardware_pio
    hardware_dma
    tinyusb_host
    tinyusb_board
    libdvi
)

pico_add_extra_outputs(uk101)

# Generated CMake Pico project file - Rudraa-X Firmware

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ====================================================================================
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(TinQa_RudraaX_V1 C CXX ASM)

set(PICO_CXX_ENABLE_RTTI 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable target and source files
add_executable(TinQa_RudraaX_V1 
    src/TinQa_RudraaX_V1.cpp

    src/drivers/FrameStreamer.h
    src/drivers/BH1750Driver.cpp
    src/drivers/LedMatrixDriver.cpp   # <--- Added Matrix Driver
    
    src/network/WifiWsServer.h

    src/config/KedarnathClimateData.h

    src/effects/thunder/ThunderEffect.cpp
    src/effects/aurora/AuroraEffect.cpp
    src/effects/auto/AutoEffect.cpp
    src/effects/sunrise/SunriseEffect.cpp
    src/effects/sunset/SunsetEffect.cpp
    src/effects/test_pattern/TestPatternEffect.cpp

    src/effects/auto/weather/WeatherProvider.cpp
    src/effects/auto/weather/WeatherProvider.h
    src/effects/auto/weather/WeatherModifier.cpp
    src/effects/auto/weather/WeatherModifier.h
    src/effects/test_pattern/TestPatternEffect.h
)

# Compile PIO file into C++ header automatically
pico_generate_pio_header(TinQa_RudraaX_V1 ${CMAKE_CURRENT_LIST_DIR}/src/drivers/ws2812.pio)

# Suppress GCC macro redefinition warnings (-Wno-macro-redefinition target)
target_compile_options(TinQa_RudraaX_V1 PRIVATE 
    -Wno-builtin-macro-redefinition
)

# Binary Metadata
pico_set_program_name(TinQa_RudraaX_V1 "TinQa_RudraaX_V1")
pico_set_program_version(TinQa_RudraaX_V1 "0.1")

# Enable USB serial output (stdio over USB)
pico_enable_stdio_uart(TinQa_RudraaX_V1 0)
pico_enable_stdio_usb(TinQa_RudraaX_V1 1)

# Include directories
target_include_directories(TinQa_RudraaX_V1 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}       # <--- Allows #include "ws2812.pio.h"
)

# Hardware & Networking Libraries
target_link_libraries(TinQa_RudraaX_V1
    pico_stdlib
    hardware_gpio
    hardware_dma
    hardware_pio                      # <--- Hardware PIO library required
    hardware_adc
    hardware_timer
    hardware_rtc
    hardware_i2c
    pico_multicore
    
    # CYW43 Wi-Fi Architecture & Async Background lwIP Networking Stack
    pico_cyw43_arch_lwip_threadsafe_background
)

pico_add_extra_outputs(TinQa_RudraaX_V1)
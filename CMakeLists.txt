# Generated CMake Pico Project File - Rudraa-X Firmware

cmake_minimum_required(VERSION 3.13)

# System Standards & Settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ====================================================================================
# Pico SDK & Toolchain Discovery Setup
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

if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# ====================================================================================
# Target Board & SDK Import
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must precede project declaration)
include(pico_sdk_import.cmake)

project(TinQa_RudraaX_V1 C CXX ASM)

set(PICO_CXX_ENABLE_RTTI 1)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# ====================================================================================
# Executable & Source Compilation Target
# ====================================================================================
add_executable(TinQa_RudraaX_V1 
    # Main Application
    src/TinQa_RudraaX_V1.cpp

    # Startup Effect
    src/effects/startup/StartupEffect.h
    src/effects/startup/StartupEffect.cpp
    
    # Hardware Drivers
    src/drivers/BH1750Driver.cpp
    src/drivers/LedMatrixDriver.cpp

    # Weather System
    src/effects/auto/weather/WeatherProvider.cpp
    src/effects/auto/weather/WeatherModifier.cpp

    # Auto Pipeline Layers
    src/effects/auto/layers/SolarLightingLayer.cpp
    src/effects/auto/layers/LunarLightingLayer.cpp
    src/effects/auto/layers/StarfieldLayer.cpp
    src/effects/auto/layers/MeteorShowerLayer.cpp

    # Atmosphere & Overlay Effects
    src/effects/auto/AutoEffect.cpp
    src/effects/sunrise/SunriseEffect.cpp
    src/effects/sunset/SunsetEffect.cpp
    src/effects/aurora/AuroraEffect.cpp
    src/effects/thunder/ThunderEffect.cpp
    src/utils/WatchdogManager.h
    src/system/StateController.h
    src/utils/TimePersistence.h
    src/drivers/Pcf8563Driver.h
    src/drivers/Pcf8563Driver.cpp
    src/effects/auto/layers/CloudLayer.h
    src/effects/auto/layers/CloudLayer.cpp
    src/utils/DisplayCompositor.h
    src/system/HealthCheckManager.h
    src/system/HealthCheckManager.cpp
)

# Compile PIO Assembly File into C++ Header
pico_generate_pio_header(TinQa_RudraaX_V1 ${CMAKE_CURRENT_LIST_DIR}/src/drivers/ws2812.pio)

# Compile Options & Warning Suppressions
target_compile_options(TinQa_RudraaX_V1 PRIVATE 
    -Wno-builtin-macro-redefinition
)

# Program Metadata
pico_set_program_name(TinQa_RudraaX_V1 "TinQa_RudraaX_V1")
pico_set_program_version(TinQa_RudraaX_V1 "0.1")

# Stdio Configuration (USB CDC enabled, UART disabled)
pico_enable_stdio_uart(TinQa_RudraaX_V1 0)
pico_enable_stdio_usb(TinQa_RudraaX_V1 1)

# Include Header Search Directories
target_include_directories(TinQa_RudraaX_V1 PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}       # Enables generated #include "ws2812.pio.h"
)

# ====================================================================================
# Link Hardware & Networking SDK Libraries
# ====================================================================================
target_link_libraries(TinQa_RudraaX_V1
    pico_stdlib
    hardware_gpio
    hardware_dma
    hardware_pio
    hardware_adc
    hardware_timer
    hardware_rtc
    hardware_i2c
    pico_multicore
    
    # CYW43 Wi-Fi & lwIP Thread-Safe Background Architecture
    pico_cyw43_arch_lwip_threadsafe_background
)

# Generate UF2 / HEX / BIN outputs
pico_add_extra_outputs(TinQa_RudraaX_V1)
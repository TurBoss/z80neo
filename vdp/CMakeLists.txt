cmake_minimum_required(VERSION 3.13)

# Use the system pico-sdk (override with -DPICO_SDK_PATH=...)
if (NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{HOME}/Dev/PICO/pico-sdk)
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(VoidVDP C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_compile_options(-Wall)

# To change the default serial config for all apps, pass e.g.
#   cmake -DDVI_DEFAULT_SERIAL_CONFIG=pimoroni_demo_hdmi_cfg ..
# then rebuild. This is applied via target_compile_definitions on each app
# individually.
set(DVI_DEFAULT_SERIAL_CONFIG "pico_sock_cfg" CACHE STRING
	"Select a default pin configuration from common_dvi_pin_configs.h")

include_directories(
	lib/PicoDVI/software/include
	lib/PicoVGA
	)

add_subdirectory(lib/PicoDVI/software/libdvi)
add_subdirectory(lib/PicoVGA)

# USB keyboard host source:
#   OFF (default): the Pico's own USB port (native controller, OTG adapter)
#   ON           : the GPIO-wired port via Pico-PIO-USB (pins in src/pins.h)
option(USB_HOST_PIO "Keyboard host on GPIO pins via Pico-PIO-USB" OFF)
add_subdirectory(src)

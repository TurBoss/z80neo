set(INCLUDE_DIR "include")

add_subdirectory(libs/pico_fatfs)

add_executable(z80neo
 src/ssd1306_i2c.c
 src/usb_descriptors.c
 src/display.c
 src/memory.c
 src/i2c_ee.c
 src/cpm_disk.c
 src/hardfault.c
 src/z80bus_pio.c
 src/main.c
    )

target_include_directories(z80neo PRIVATE
	 ${INCLUDE_DIR}
	    )

# Use the SIO GPIO registers instead of the RP2350 GPIO coprocessor (mcr p0):
# the coprocessor faults with NOCP on this core/SDK combination.
target_compile_definitions(z80neo PRIVATE PICO_USE_GPIO_COPROCESSOR=0)

# PIO bus handler (full bus cycle capture)
pico_generate_pio_header(z80neo ${CMAKE_CURRENT_LIST_DIR}/src/z80bus_pio.pio)
# PIO clock generator (replaces PWM to free GPIO33)
pico_generate_pio_header(z80neo ${CMAKE_CURRENT_LIST_DIR}/src/z80clock.pio)

option(PSRAM_ENABLE "Enable PSRAM on RP2350 BB48R" OFF)
if(PSRAM_ENABLE)
  target_compile_definitions(z80neo PRIVATE
    PSRAM_ENABLE=1
    PICO_PSRAM_CS_PIN=8
    PICO_PSRAM_SIZE_BYTES=8388608
  )
  message(STATUS "PSRAM enabled (CS=GPIO8, 8MB)")
endif()

# pull in common dependencies
target_link_libraries(z80neo
 pico_stdlib
 pico_multicore
 hardware_psram
 pico_unique_id
 hardware_gpio
 hardware_i2c
 hardware_adc
 hardware_pwm
 hardware_spi
 hardware_uart
	 hardware_irq
	 hardware_pio
	 hardware_resets
 hardware_pll
 hardware_vreg
 hardware_watchdog
 hardware_xosc
 tinyusb_device
 tinyusb_board
 pico_fatfs
 )

# enable USB CDC
pico_enable_stdio_usb(z80neo 1)

# enable UART
pico_enable_stdio_uart(z80neo 1)

pico_add_extra_outputs(z80neo)

# create map/bin/hex file etc.
example_auto_set_url(z80neo)

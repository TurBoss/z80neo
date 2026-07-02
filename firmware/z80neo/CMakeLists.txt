set(INCLUDE_DIR "include")

add_subdirectory(libs/pico_fatfs)

add_executable(z80neo
 src/ssd1306_i2c.c
 src/usb_descriptors.c
 src/utils.c
 src/display.c
 src/memory.c
 src/main.c
    )

target_include_directories(z80neo PRIVATE
 ${INCLUDE_DIR}
    )



# pull in common dependencies
target_link_libraries(z80neo
 pico_stdlib
 pico_multicore
 pico_unique_id
 hardware_gpio
 hardware_i2c
 hardware_adc
 hardware_pwm
 hardware_spi
 hardware_pio
 hardware_dma
 hardware_uart
 hardware_irq
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
pico_add_extra_outputs(z80neo)

# add url via pico_set_program_url
example_auto_set_url(z80neo)

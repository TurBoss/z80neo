set(INCLUDE_DIR "include")

add_subdirectory(libs/pico_fatfs)

add_executable(turboram
	src/ssd1306_i2c.c
	src/u8x8_fonts.c
	src/usb_descriptors.c
	src/main.c
    )

target_include_directories(turboram PRIVATE
	${INCLUDE_DIR}
    )



# pull in common dependencies
target_link_libraries(turboram
	pico_stdlib
	hardware_i2c
	pico_multicore
	hardware_adc
	hardware_pwm
	hardware_spi
	hardware_pio
	hardware_dma
	pico_unique_id
	tinyusb_device
	tinyusb_board
	pico_fatfs
	)

# enable USB CDC
pico_enable_stdio_usb(turboram 1)

pico_add_extra_outputs(turboram)

# create map/bin/hex file etc.
pico_add_extra_outputs(turboram)


# add url via pico_set_program_url
example_auto_set_url(turboram)
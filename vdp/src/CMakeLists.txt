add_executable(VoidVDP
	main.c
        framebuf.c
        framebuf_vga.cpp
        framebuf_dvi.c
        font.c
        terminal.c
        keyboard.c
        keyboard_usb.c
        keyboard_ps2.c
        config.c
        xmodem.c
        flash.c
        serial.c
        serial_uart.c
        serial_cdc.c
        sound.c
	tmds_encode_font_2bpp.S
	tmds_encode_font_2bpp.h
)

target_compile_definitions(VoidVDP PRIVATE
	DVI_VERTICAL_REPEAT=1
	PICO_FLASH_SAFE_EXECUTE_PICO_SUPPORT_MULTICORE_LOCKOUT=1
	)

target_include_directories(VoidVDP PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

# With USB_HOST_PIO the keyboard host is on PIO-USB (rhport 1).  The SDK's
# tinyusb host base only builds the native rp2040 HCD, which is empty when
# rhport 0 is device-only, so compile the PIO-USB HCD too.
if(USB_HOST_PIO)
        target_compile_definitions(VoidVDP PRIVATE USB_HOST_PIO=1)
        target_sources(VoidVDP PRIVATE
                ${PICO_SDK_PATH}/lib/tinyusb/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
                )
endif()

# We have a lot in SRAM4 (particularly TMDS LUT) but don't need much stack on
# core 1. Probably even 256 bytes would be fine.
# Core 0 runs tinyusb (device + host) and PicoVGA setup; the SDK default core 0
# stack is only 0x800 (2K), which overflows and corrupts the RAM-resident code.
# PICO_USE_GPIO_COPROCESSOR=0: the RP2350 GPIO coprocessor (the mcr/branch
# sequence gpio_put() uses by default) hard-faults with UFSR NOCP on this
# SDK/core, e.g. the very first received serial character calling blink_led()
# kills core 0.  Same workaround as the z80neo firmware; use the SIO path.
target_compile_definitions(VoidVDP PRIVATE
	DVI_DEFAULT_SERIAL_CONFIG=${DVI_DEFAULT_SERIAL_CONFIG}
	PICO_STACK_SIZE=0xC00
	PICO_CORE1_STACK_SIZE=0x200
	PICO_USE_GPIO_COPROCESSOR=0
	)

target_link_libraries(VoidVDP
	pico_stdlib
	pico_multicore
	pico_flash
	hardware_flash
	libdvi
        tinyusb_host
        tinyusb_device
        PicoVGA
)
if(USB_HOST_PIO)
        # SDK 2.3.0 provides its own Pico-PIO-USB
        target_link_libraries(VoidVDP tinyusb_pico_pio_usb)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(VoidVDP)

#ifndef PINS_H
#define PINS_H

// vga output uses gpio pins 0,1,2,3,4,5,6,7,8
// dvi output uses gpio pins 12,13,14,15,16,17,18,19

#define PIN_BUZZER       9

// USB keyboard host source, selected at build time:
//   default        : the Pico's own USB port (native controller, OTG adapter)
//   -DUSB_HOST_PIO : the GPIO-wired port via Pico-PIO-USB (pins below)
// PIO-USB needs D+ and D- on adjacent GPIOs; set the pair here.
#ifdef USB_HOST_PIO
#define USB_HOST_DP_PIN 11
#define USB_HOST_DM_PIN 10
#endif

#define PIN_PS2_CLOCK   10
#define PIN_PS2_DATA    11
#define PIN_DEFAULTS    22
#define PIN_LED         25
#define PIN_HDMI_DETECT 28

#define PIN_UART_ID   uart1
#define PIN_UART_TX   20     // uart0: 0, 12, 16, 28  uart1: 4,  8, 20, 24
#define PIN_UART_RX   21     // uart0: 1, 13, 17, 29  uart1: 5,  9, 21, 25
#define PIN_UART_CTS  26     // uart0: 2, 14, 18      uart1: 6, 10, 22, 26
#define PIN_UART_RTS  27     // uart0: 3, 15, 19      uart1: 7, 11  23, 27

#endif

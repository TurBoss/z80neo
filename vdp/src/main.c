// -----------------------------------------------------------------------------
// VoidVDP - A versatile serial terminal (fork of VersaTerm)
// Copyright (C) 2022 David Hansel
// Copyright (C) 2026 turboss
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/bootrom.h"
#include "tusb.h"
#ifdef USB_HOST_PIO
#include "pio_usb.h"
#endif
#include "framebuf.h"
#include "serial.h"
#include "keyboard.h"
#include "terminal.h"
#include "config.h"
#include "font.h"
#include "pins.h"
#include "sound.h"


// see comment at start of main()
#define BOOTSEL_TIMEOUT_MS 1500
static absolute_time_t bootsel_timeout = 0;
static const uint32_t bootsel_magic[] = {0xf01681de, 0xbd729b29, 0xd359be7a};
static uint32_t __uninitialized_ram(bootsel_magic_ram)[count_of(bootsel_magic)];
static uint16_t ignore_key = HID_KEY_NONE;


void apply_settings()
{
  font_apply_settings();
  framebuf_apply_settings();
  keyboard_apply_settings();
  terminal_apply_settings();
  serial_apply_settings();
}


void wait(uint32_t milliseconds);

void run_tasks(bool processInput)
{
  // tinyusb tasks.  Use the non-blocking _ext(0,...) forms: since tinyusb
  // 0.16 / pico-sdk 2.x the plain tud_task()/tuh_task() wrappers pass
  // UINT32_MAX and block forever when no event is pending, which starves the
  // rest of run_tasks() (serial input, keyboard) whenever USB is idle.
  if( tud_inited() ) tud_task_ext(0, false);
  if( tuh_inited() ) tuh_task_ext(0, false);
  
  // process serial input
  serial_task(processInput);

  // handle bootsel mechanism timeout
  if( bootsel_timeout>0 && get_absolute_time()>=bootsel_timeout )
    {
      bootsel_timeout = 0;
      for(uint i=0; i<count_of(bootsel_magic); i++) 
        bootsel_magic_ram[i] = 0;
    }
  
  // process keyboard input
  keyboard_task();
  if( processInput && keyboard_num_keypress()>0 )
    {
      uint16_t key = keyboard_read_keypress();
      if( key!=ignore_key )
        {
          ignore_key = HID_KEY_NONE;

          if( key==HID_KEY_F12 )
            {
              if( config_menu() ) apply_settings();
            }
          else if( keyboard_ctrl_pressed(key) && (key&0xFF)==HID_KEY_F12 )
            {
              if( config_load(0xFF) ) apply_settings();
            }
          else if( key==HID_KEY_F11 )
            keyboard_macro_record_startstop();
          else if( keyboard_ctrl_pressed(key) && (key&0xFF)>=HID_KEY_F1 && (key&0xFF)<=HID_KEY_F10 )
            {
              uint8_t vol = config_get_audible_bell_volume();
              if( config_load((key&0xFF)-HID_KEY_F1) )
                {
                  apply_settings();
                  sound_play_tone(880, 50, vol, false);
                }
              else
                {
                  sound_play_tone(880, 50, vol, true); wait(50);
                  sound_play_tone(880, 50, vol, true); wait(50);
                  sound_play_tone(880, 50, vol, false); 
                }
            }
          else
            terminal_process_key(key);
        }
    }
}


void wait(uint32_t milliseconds)
{
  absolute_time_t timeout = make_timeout_time_ms(milliseconds);
  while( get_absolute_time()<timeout ) run_tasks(false);
}


void enable_fpu(void)
{
  // The RP2350 boot ROM's RCP setup leaves CPACR with CP10/CP11 disabled, so any
  // code compiled with -march=armv8-m.main+fp (this build is -mfloat-abi=softfp)
  // traps with UsageFault NOCP and escalates to a HardFault.  Same fix as the
  // z80neo firmware.  Per-core, so core1 must do it too.
  volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88u;
  *cpacr |= (0xFu << 20);   // CP10/CP11: full access
  __asm__ volatile("dsb\nisb" ::: "memory");
}


int main()
{
  enable_fpu();

  // The following mechanism is fundamentally the same as the pico_bootsel_via_double_reset
  // library but that library uses a busy wait until the maximum time for the double-tap
  // has expired. Implementing it ourselves here instead allows to use that wait time
  // for initialization.
  uint i;
  for(i=0; i<count_of(bootsel_magic) && bootsel_magic_ram[i]==bootsel_magic[i]; i++);

  if( i<count_of(bootsel_magic) )
    {
      // arm mechanism and set timeout
      for(i=0; i<count_of(bootsel_magic); i++) 
        bootsel_magic_ram[i] = bootsel_magic[i];
      bootsel_timeout = make_timeout_time_ms(BOOTSEL_TIMEOUT_MS);
    }
  else
    {
      // disarm our mechanism so pressing RESET in boot loader starts up normally
      for(i=0; i<count_of(bootsel_magic); i++) 
        bootsel_magic_ram[i] = 0;
      
      // boot into boot-loader using GPIO25 (on-board LED) as activity LED
      reset_usb_boot(1<<25, 0);
    }
  
  config_init();
  stdio_uart_init_full(PIN_UART_ID, 300, PIN_UART_TX, PIN_UART_RX);
  serial_init();

  // initialize USB (needed for keyboard)
  {
    const tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_FULL };
    const tusb_rhport_init_t host_init = { .role = TUSB_ROLE_HOST, .speed = TUSB_SPEED_FULL };

#ifdef USB_HOST_PIO
    // Keyboard host is on the GPIO-wired port: PIO-USB on rhport 1 (the display
    // owns PIO0, so use PIO1).  Native USB stays a CDC device.
    if( config_get_usb_mode()!=0 )
      tusb_rhport_init(0, &dev_init);
    {
      pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
      pio_cfg.pin_dp = USB_HOST_DP_PIN;
      pio_cfg.pinout = (USB_HOST_DM_PIN == USB_HOST_DP_PIN + 1)
                         ? PIO_USB_PINOUT_DPDM : PIO_USB_PINOUT_DMDP;
      pio_cfg.pio_tx_num = 1;
      pio_cfg.pio_rx_num = 1;
      tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);
      tusb_rhport_init(1, &host_init);
    }
#else
    // Keyboard host is the Pico's own USB port (OTG adapter).
    if( config_get_usb_mode()==1 )
      tusb_rhport_init(0, &dev_init);
    else if( config_get_usb_mode()==2 )
      tusb_rhport_init(0, &host_init);
    else if( config_get_usb_mode()==3 )
      {
        gpio_init(24);
        gpio_set_dir(24, false); // input
        tusb_rhport_init(0, gpio_get(24) ? &dev_init : &host_init);
      }
#endif
  }

  // initialize keyboard
  keyboard_init();

  // allow some time for keyboard(s) to initialize
  wait(tuh_inited() ? 1500 : 250);
  
  // if DEFAULTS button and CTRL key is pressed then force DVI
  if( !gpio_get(PIN_DEFAULTS) )
    framebuf_init((keyboard_get_current_modifiers() & (KEYBOARD_MODIFIER_LEFTCTRL|KEYBOARD_MODIFIER_RIGHTCTRL))!=0);
  else
    {
      // check F1-F10 keys for startup config
      while( keyboard_num_keypress()>0 )
        {
          uint16_t c = keyboard_read_keypress();
          if( (c & 0xFF)>=HID_KEY_F1 && (c & 0xFF)<=HID_KEY_F10 && config_load((c & 0xFF)-HID_KEY_F1) )
            {
              // apply settings (may have changed)
              keyboard_apply_settings();
              serial_apply_settings();
              // ignore further repeats of Fx key 
              ignore_key = c;
              break;
            }
        }

      framebuf_init(false);
    }
  
  terminal_init();
  sound_init();
  config_show_splash();

  while( true ) run_tasks(true);
}

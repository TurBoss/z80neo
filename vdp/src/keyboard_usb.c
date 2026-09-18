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

#include "keyboard.h"
#include "keyboard_usb.h"
#include "config.h"
#include "pico/time.h"
#include "tusb.h"
#include "config.h"
#include "keyboard.h"
#include <ctype.h>

//#define DEBUG

#ifdef DEBUG
#include "terminal.h"
#include <stdarg.h>
static void print(const char *format, ...)
{
  char buffer[101];
  va_list argptr;
  va_start(argptr, format);
  vsnprintf(buffer, 100, format, argptr);
  terminal_receive_string(buffer);
  va_end(argptr);
}
#else
#define print(...)
#endif


//--------------------------------------------------------------------+
// Keyboard functions
//--------------------------------------------------------------------+

static uint8_t keyboard_repeat_key = 0;
static absolute_time_t keyboard_repeat_timeout = 0;
static uint8_t s_leds, keyboard_dev_addr = 0xFF, keyboard_instance = 0xFF;
static uint8_t keyboard_report_offset = 0;   // skip leading report ID if present


void keyboard_usb_set_led_status(uint8_t leds)
{
  if( keyboard_dev_addr!=0xFF && keyboard_instance!=0xFF )
    {
      s_leds = leds;
      tuh_hid_set_report(keyboard_dev_addr, keyboard_instance, 0, HID_REPORT_TYPE_OUTPUT, &s_leds, sizeof(s_leds));
    }
}


// look up new key in previous keys
static inline bool find_key_in_report(const hid_keyboard_report_t *report, uint8_t keycode)
{
  for(uint8_t i=0; i<6; i++)
    if (report->keycode[i] == keycode)  
      return true;

  return false;
}


static void process_kbd_report(uint8_t dev_addr, uint8_t instance, hid_keyboard_report_t const *report)
{
  static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

  // transform modifier changes back into keypresses - the HID keyboard boot protocol keeps 
  // track of modifier keys but we need the raw keypresses (for mapping modifier keys)
  if( prev_report.modifier != report->modifier )
    {
      uint8_t mod1 = prev_report.modifier, mod2 = report->modifier;
      if( (mod1 & KEYBOARD_MODIFIER_LEFTSHIFT)!=(mod2 & KEYBOARD_MODIFIER_LEFTSHIFT) )
        keyboard_key_change(HID_KEY_SHIFT_LEFT, (mod2 & KEYBOARD_MODIFIER_LEFTSHIFT)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_RIGHTSHIFT)!=(mod2 & KEYBOARD_MODIFIER_RIGHTSHIFT) )
        keyboard_key_change(HID_KEY_SHIFT_RIGHT, (mod2 & KEYBOARD_MODIFIER_RIGHTSHIFT)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_LEFTCTRL)!=(mod2 & KEYBOARD_MODIFIER_LEFTCTRL) )
        keyboard_key_change(HID_KEY_CONTROL_LEFT, (mod2 & KEYBOARD_MODIFIER_LEFTCTRL)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_RIGHTCTRL)!=(mod2 & KEYBOARD_MODIFIER_RIGHTCTRL) )
        keyboard_key_change(HID_KEY_CONTROL_RIGHT, (mod2 & KEYBOARD_MODIFIER_RIGHTCTRL)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_LEFTALT)!=(mod2 & KEYBOARD_MODIFIER_LEFTALT) )
        keyboard_key_change(HID_KEY_ALT_LEFT, (mod2 & KEYBOARD_MODIFIER_LEFTALT)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_RIGHTALT)!=(mod2 & KEYBOARD_MODIFIER_RIGHTALT) )
        keyboard_key_change(HID_KEY_ALT_RIGHT, (mod2 & KEYBOARD_MODIFIER_RIGHTALT)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_LEFTGUI)!=(mod2 & KEYBOARD_MODIFIER_LEFTGUI) )
        keyboard_key_change(HID_KEY_GUI_LEFT, (mod2 & KEYBOARD_MODIFIER_LEFTGUI)!=0);
      if( (mod1 & KEYBOARD_MODIFIER_RIGHTGUI)!=(mod2 & KEYBOARD_MODIFIER_RIGHTGUI) )
        keyboard_key_change(HID_KEY_GUI_RIGHT, (mod2 & KEYBOARD_MODIFIER_RIGHTGUI)!=0);
    }

  for(uint8_t i=0; i<6; i++)
    if( report->keycode[i]!=HID_KEY_NONE && !find_key_in_report(&prev_report, report->keycode[i]) )
      {
        // key was newly pressed
        keyboard_key_change(report->keycode[i], true);
        keyboard_repeat_key = report->keycode[i];
        keyboard_repeat_timeout = make_timeout_time_ms(config_get_keyboard_repeat_delay_ms());
      }
  
  for(uint8_t i=0; i<6; i++)
    if( prev_report.keycode[i]!=HID_KEY_NONE && !find_key_in_report(report, prev_report.keycode[i]) )
      {
        // key was released
        keyboard_key_change(prev_report.keycode[i], false);
        if( prev_report.keycode[i] == keyboard_repeat_key )
          keyboard_repeat_key = HID_KEY_NONE;
      }

  prev_report = *report;
}


void keyboard_usb_task()
{
  if( keyboard_repeat_key!=HID_KEY_NONE && get_absolute_time() >= keyboard_repeat_timeout )
    {
      keyboard_key_change(keyboard_repeat_key, true);
      keyboard_repeat_timeout = make_timeout_time_ms(1000000/config_get_keyboard_repeat_rate_mHz());
    }
}


void keyboard_usb_apply_settings()
{
}


void keyboard_usb_init()
{
}



//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+


// Invoked when device with hid interface is mounted
// We use the boot protocol where the device supports it; otherwise we fall
// back to detecting a keyboard from the HID report descriptor, since many
// keyboards (NKRO, gaming, wireless dongles) only expose a non-boot interface.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  bool     is_keyboard = (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD);
  uint8_t  report_id   = 0;

  if( !is_keyboard && desc_report!=NULL && desc_len>0 )
    {
      tuh_hid_report_info_t info[8];
      uint8_t n = tuh_hid_parse_report_descriptor(info, 8, desc_report, desc_len);
      for(uint8_t i=0; i<n; i++)
        if( info[i].usage_page==HID_USAGE_PAGE_DESKTOP && info[i].usage==HID_USAGE_DESKTOP_KEYBOARD )
          {
            is_keyboard = true;
            report_id   = info[i].report_id;
            break;
          }
    }

  if( is_keyboard )
    {
      keyboard_dev_addr = dev_addr;
      keyboard_instance = instance;
      // boot reports have no report ID; report-protocol keyboards may prefix one
      keyboard_report_offset = (itf_protocol==HID_ITF_PROTOCOL_KEYBOARD || report_id==0) ? 0 : 1;
      keyboard_repeat_key = HID_KEY_NONE;
      keyboard_usb_set_led_status(keyboard_get_led_status());
    }

  // request to receive report
  tuh_hid_receive_report(dev_addr, instance);
}


void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
  if( dev_addr==keyboard_dev_addr && instance==keyboard_instance )
    {
      keyboard_dev_addr = 0xFF;
      keyboard_instance = 0xFF;
    }
}


// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
  if( dev_addr==keyboard_dev_addr && instance==keyboard_instance &&
      len >= (uint16_t) (8 + keyboard_report_offset) )
    process_kbd_report(dev_addr, instance, (hid_keyboard_report_t const*) (report + keyboard_report_offset));

  // continue to request to receive report
  tuh_hid_receive_report(dev_addr, instance);
}

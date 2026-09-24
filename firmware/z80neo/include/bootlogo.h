#ifndef BOOTLOGO_H
#define BOOTLOGO_H

// Return the VDP / sdl_client to text mode (a previous gfx program may have
// left the bitmap layer on) and clear the screen, before the boot banner.
void boot_console_reset(void);

// Draw the z80neo graphical boot logo on the VDP / sdl_client, then return the
// console to text mode.  Emits the VDP "ESC g" bitmap protocol on the console
// UART; the Z80 is still in reset when this runs.
void boot_logo_show(void);

#endif

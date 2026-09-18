# VoidVDP — z80neo VDP fork of VersaTerm (build notes)

**VoidVDP** is turboss's fork (2026) of David Hansel's
[VersaTerm](https://github.com/dhansel/VersaTerm), used as the VDP for z80neo.
VersaTerm is Copyright (C) 2022 David Hansel, GPLv3; see the source headers.
Notes that differ from the upstream instructions at the bottom of this file:

## pico-sdk
The builds use the **system** pico-sdk, not a bundled submodule:

    PICO_SDK_PATH=$HOME/Dev/PICO/pico-sdk      # 2.3.0

`build.sh` passes this and `CMakeLists.txt` defaults to it.

## Targets
`build.sh` takes an optional target (default `rp2350`):

* **RP2350 (Pico 2)** — default, builds with the fancier PicoDVI RP2350 preview
  and the ported VGA path (output in `build2350/`):

      ./build.sh            # same as ./build.sh rp2350

* **RP2040 (Pico)** — builds DVI *and* VGA (output in `build/`):

      ./build.sh rp2040

  RP2350 VGA notes: the RP2350 has no SIO hardware divider, so `vga_ctext.S`
  uses Cortex-M33 `udiv`/`mls`; the unused perspective/tile/gradient renderers
  are stubbed in `render/render_stubs.S`.  `util/overclock.cpp` uses the SDK's
  `PICO_PLL_VCO_MIN/MAX_FREQ_HZ` (RP2350 needs a VCO >= 750 MHz) — a hard-coded
  400 MHz minimum leaves an unlockable PLL and the VGA sync dies.

  RP2350 runtime notes (already set in `src/CMakeLists.txt`, no flags needed):
  * `PICO_USE_GPIO_COPROCESSOR=0` — the RP2350 GPIO coprocessor (`mcr`)
    hard-faults with `UFSR NOCP` on this SDK/core.  Without this the very first
    received serial character (which calls `blink_led()` -> `gpio_put()`) kills
    core 0, so serial input never displays.  Same workaround as the z80neo
    firmware.
  * `enable_fpu()` is called on core 0 (`main.c`) and core 1 (`framebuf_vga.cpp`)
    because the RP2350 boot ROM RCP setup leaves CPACR CP10/CP11 disabled while
    the build uses `-march=armv8-m.main+fp`.
  * `PICO_STACK_SIZE=0xC00` — the 2 K default is marginal for tinyusb + PicoVGA.

## USB keyboard
* Default: keyboard on the **native** USB port (OTG adapter), like upstream.
* `-DUSB_HOST_PIO=ON`: keyboard on the **GPIO-wired** port via Pico-PIO-USB
  (pins in `src/pins.h`); uses the SDK's `tinyusb_pico_pio_usb`.

## Console
Serial console is **UART1 on GP20 (TX) / GP21 (RX)**, default 9600 8N1
(configurable).  A Raspberry Pi Debug Probe with its UART bridged to those pins
appears as a CDC-ACM `/dev/ttyACM*` on the host.

## Flashing
    ./upload.sh                       # RP2350: build2350/src/VoidVDP.elf, target/rp2350.cfg
    ./upload.sh rp2040                # RP2040: build/src/VoidVDP.elf, target/rp2040.cfg

---

## Uploading the VersaTerm firmware to the Raspberry Pi Pico

Uploading firmware to the Raspberry Pi Pico is easy:
- Press and hold the button on the Raspberry Pi Pico (there is only one) 
- While holding the button, connect the Raspberry Pi Pico via its micro-USB port to your computer
- Release the button
- Your computer should recognize the Pico as a storage device (like a USB stick) and mount it as a drive
- Copy the [VersaTerm.uf2](VersaTerm.uf2) file to the drive mounted in the previous step

## Building the VersaTerm firmware from source

### Requirements
- CMake 3.12 or later
- GCC (cross-)compiler: arm-none-eabi-gcc

### Getting and building the source

```
git clone https://github.com/dhansel/VersaTerm.git
cd VersaTerm/software/lib
git submodule update --init
cd pico-sdk/lib
git submodule update --init
cd tinyusb
git checkout 86ad6e5
cd ../../../..
mkdir build
cd build
cmake .. -DPICO_SDK_PATH=../lib/pico-sdk -DPICO_COPY_TO_RAM=1
make
```

The `git checkout 86ad6e5` command updates TinyUSB to version 0.18 instead of version 0.12
which was included with the pico-sdk version used by VersaTerm. Version 0.12 has issues
with (some) USB hubs which are resolved in 0.18.

This should create file VersaTerm/software/build/src/VersaTerm.uf2<br>
Follow the "Uploading firmware to Raspberry Pi Pico" instructions above to upload the .uf2 file to the Pico.

The instructions above will use the of pico-sdk and PicoDVI versions that were
current when I wrote and tested VersaTerm. Building from those sources should result in
the same VerTerm.uf2 file as the one in VersaTerm/software.

If you feel adventurous you can build VersaTerm with the latest versions of the libraries:
```
git clone https://github.com/dhansel/VersaTerm.git
cd VersaTerm/software/lib
git submodule update --init
cd pico-sdk/lib
git submodule update --init
git submodule update --remote --merge
cd ../..
git submodule update --remote --merge
cd ..
mkdir build
cd build
cmake .. -DPICO_SDK_PATH=../lib/pico-sdk -DPICO_COPY_TO_RAM=1
make
```

## Some solutions to compile issues

A big thank you to user [unclouded](https://github.com/un-clouded) who reported a number of compile-time 
issues and their solutions:

When it said to me:

    arm-none-eabi-gcc: fatal error: cannot read spec file 'nosys.specs': No such file or directory

I replied:

    apt install libnewlib-arm-none-eabi

And when it said:

    fatal error: cassert: No such file or directory

I retorted:

    apt install libstdc++-arm-none-eabi-dev

And then it complained that:

    /usr/lib/gcc/arm-none-eabi/12.2.1/../../../arm-none-eabi/bin/ld: cannot find -lstdc++: No such file or directory

And I spake thusly:

    apt install libstdc++-arm-none-eabi-newlib

# z80neo LLVM-Z80 graphics example

A bare-metal VoidVDP bitmap demo for the z80neo, compiled with the
**LLVM-Z80** backend via z88dk's `-compiler=llvmz80`, using the in-tree
`+z80neo` classic-library target (see the repository `AGENTS.md`, section
*C programs (`zcc +z80neo`)*).

Unlike the `software/z80neo/demos/` programs (built with the default sccz80
compiler), this example is built with clang.  z88dk's `llvmz80` driver runs

```
clang --target=z80-unknown-none-z88dk -S -std=gnu23 <file.c>
```

to emit Z80 assembly, which `z88dk-z80asm` then assembles and links against
`z80neo_clib.lib`.  The compiler must therefore be a clang that provides the
`z80-unknown-none-z88dk` target **and** the z88dk classic-library ABI
(`sdcccall0` default plus the reversed argument order for the classic-library
aliases) — that is `ravn/llvm-z80`, branch `z88dk`.  The stock LLVM-Z80
release builds do not include that ABI.

## What it draws

The VoidVDP bitmap layer is `640x256` with 16 colours (CGA order).  The
program enables the layer and animates:

* sixteen hardware-filled colour bars (`z80neo_gfx_fill` + `z80neo_gfx_color`),
* a rotating 12-spoke line fan (`z80neo_gfx_line`),
* a box sliding along the bottom,
* a short per-pixel colour ramp (`z80neo_gfx_pixel`, one display colour per
  pixel; RP2350 VDP only).

Press `q` or `ESC` to leave bitmap mode and quit.

## Prerequisites

* The in-tree z88dk (`../../z88dk`), with `bin/zcc` rebuilt from source so it
  understands `-compiler=llvmz80`.
* A `z80-unknown-none-z88dk` clang.  Build one with:

  ```sh
  git clone --depth 1 -b z88dk https://github.com/ravn/llvm-z80.git llvm-z80
  cd llvm-z80
  cmake -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON -DLLVM_CCACHE_BUILD=ON \
        -C clang/cmake/caches/Z80.cmake -G Ninja -S llvm -B build
  ninja -C build -j"$(nproc)" clang
  ```

## Build

`build.sh` sets the z88dk environment (`PATH`, `ZCCCFG`) and `LLVMZ80EXE`,
then runs `make`:

```sh
./build.sh                 # -> build/vdpdemo.ihx
./build.sh bin             # -> build/vdpdemo (raw binary)
./build.sh OPT=-O0         # debug optimisation (changing OPT rebuilds)
./build.sh clean
```

It auto-detects the compiler (`llvmz80-clang` on `PATH`, or
`$HOME/Dev/llvm-z80/build/bin/clang`); override the locations from the
environment:

```sh
Z88DK=/path/to/z88dk LLVMZ80EXE=/path/to/clang ./build.sh
```

Equivalent plain make:

```sh
make LLVMZ80EXE=/path/to/llvm-z80/build/bin/clang
```

`OPT` overrides the optimisation level (default `-O2`; use `-O0` to debug).

## Run on the z80neo

1. Copy `build/vdpdemo.ihx` to the SD-card root.
2. Point the first program in `Z80NEO.INI` at it:

   ```
   cpmz80neo.hex   ->   vdpdemo.ihx
   ```

   (`BANK_PROG[0]`, the first filename after the six ADC thresholds.)
3. Reset the board; the demo runs and drives the VDP over the console.

## Notes

* The program must fit in page 0 (`0x0000-0x3FFF`); the Pico HEX loader
  replicates that page into all four banks.
* The example is self-contained except for the fixed-point sine/LCG helpers
  it reuses from `../demos/common.h`; only the standard `<stdint.h>`,
  `<stdio.h>` (`getk`) and `<z80neo.h>` headers are needed.

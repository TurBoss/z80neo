#!/bin/bash

export PATH=/usr/local/bin:/usr/bin:/bin
export PICO_SDK_PATH=/home/turboss/Dev/PICO/pico-sdk
export PICO_EXTRAS_PATH=/home/turboss/Dev/PICO/pico-extras

# ARM Cortex-M33 (both cores) — PSRAM works on ARM
# Board is the Olimex RP2350-PICO2-BB48 (RP2350B, 48 GPIOs); header in boards/.
cmake -B build -S . -DPICO_BOARD=z80neo_bb48 -DPSRAM_ENABLE=ON
# For RISC-V: cmake -B build -S . -DPICO_BOARD=z80neo_bb48 -DPICO_PLATFORM=rp2350-riscv -DPSRAM_ENABLE=ON

cmake --build build --parallel 1

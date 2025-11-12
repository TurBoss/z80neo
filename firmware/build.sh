#!/bin/bash
export PATH=/opt/riscv/bin:$PATH 
export PICO_SDK_PATH=/home/turboss/Dev/PICO/pico-sdk
export PICO_EXTRAS_PATH=/home/turboss/Dev/PICO/pico-extras

cmake  -B build -S . -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-riscv &&
cmake --build build --parallel 1 &&

openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program build/z80neo/turboram.elf verify reset exit"

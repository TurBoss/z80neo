#!/bin/bash


##
#  configure riscv gnu toolchain
#
# ./configure --prefix=/opt/riscv32 --with-arch=rv32ima_zicsr_zifencei_zba_zbb_zbs_zbkb_zca_zcb --with-abi=ilp32 --with-multilib-generator="rv32ima_zicsr_zifencei_zba_zbb_zbs_zbkb_zca_zcb-ilp32--;rv32imac_zicsr_zifencei_zba_zbb_zbs_zbkb-ilp32--"
#
##


export PATH=/opt/riscv32/bin:$PATH 

export PICO_SDK_PATH=/home/turboss/Dev/PICO/pico-sdk
export PICO_EXTRAS_PATH=/home/turboss/Dev/PICO/pico-extras


cmake  -B build -S . -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-riscv &&
 
cmake --build build --parallel 1


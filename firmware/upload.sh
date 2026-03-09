#!/bin/bash

openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "adapter speed 5000" -c "program build/z80neo/z80neo.elf verify reset exit"

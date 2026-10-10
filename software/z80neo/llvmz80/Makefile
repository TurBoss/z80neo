# z80neo LLVM-Z80 graphics example.
#
# Builds a bare-metal VoidVDP bitmap demo with the llvm-z80 clang backend
# through the in-tree z88dk `+z80neo` target:
#
#   make LLVMZ80EXE=/path/to/llvm-z80/build/bin/clang
#
# `LLVMZ80EXE` must point at a clang with the `z80-unknown-none-z88dk` target
# and the z88dk classic-library ABI (ravn/llvm-z80, branch `z88dk`).
#
# Output: build/vdpdemo.ihx  (Intel HEX, for Z80NEO.INI BANK_PROG[0])

Z88DK      ?= $(abspath $(CURDIR)/../../z88dk)
ZCC        := $(Z88DK)/bin/zcc
LLVMZ80EXE ?= llvmz80-clang
OPT        ?= -O2

export PATH       := $(Z88DK)/bin:$(PATH)
export ZCCCFG     := $(Z88DK)/lib/config
export LLVMZ80EXE

BUILD := build
SRC   := vdpdemo.c
OUT   := $(BUILD)/vdpdemo
OPTFILE := $(BUILD)/.opt

all: $(OUT).ihx

$(BUILD):
	mkdir -p $(BUILD)

# Record the optimisation flags so a changed OPT retriggers the build.
$(OPTFILE): FORCE | $(BUILD)
	@printf '%s\n' '$(OPT)' > $@.new
	@if [ -f $@ ] && cmp -s $@.new $@; then rm -f $@.new; else mv -f $@.new $@; fi

$(OUT).ihx: $(SRC) ../demos/common.h $(OPTFILE) | $(BUILD)
	$(ZCC) +z80neo -compiler=llvmz80 $(OPT) $(SRC) -o $(OUT) -create-app

# Raw binary (no Intel HEX wrapper)
bin: $(SRC) ../demos/common.h | $(BUILD)
	$(ZCC) +z80neo -compiler=llvmz80 $(OPT) -subtype=bin $(SRC) -o $(OUT)

clean:
	rm -rf $(BUILD)

FORCE:

.PHONY: all bin clean FORCE

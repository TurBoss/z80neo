# z80neo machine monitor / debugger
#
# Built with the in-tree z88dk and its `+z80neo` target (bare-metal Z80,
# linked at 0x0000, console on ports 0x80/0x81).  Override the toolchain
# location with:  make Z88DK=/path/to/z88dk
#
# Output: build/zmon.ihx  (Intel HEX, load via Z80NEO.INI BANK_PROG[0])

Z88DK   ?= $(abspath $(CURDIR)/../../z88dk)
ZCC     := $(Z88DK)/bin/zcc

export PATH   := $(Z88DK)/bin:$(PATH)
export ZCCCFG := $(Z88DK)/lib/config

BUILD_DIR := build
SRC       := src/monitor.c src/disasm.c src/mon_asm.asm
OUT       := $(BUILD_DIR)/zmon

all: $(OUT).ihx

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUT).ihx: $(SRC) | $(BUILD_DIR)
	$(ZCC) +z80neo $(SRC) -o $(OUT) -create-app

# Raw binary (no Intel HEX wrapper)
bin: $(SRC) | $(BUILD_DIR)
	$(ZCC) +z80neo -subtype=bin $(SRC) -o $(OUT)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all bin clean

# z80neo SD-card MIDI player
#
# Built with the in-tree z88dk and its `+z80neo` target.  Override the
# toolchain location with:  make Z88DK=/path/to/z88dk
#
# Output: build/midiplayer.ihx (Intel HEX for Z80NEO.INI PROG1)

Z88DK   ?= $(abspath $(CURDIR)/../../z88dk)
ZCC     := $(Z88DK)/bin/zcc

export PATH   := $(Z88DK)/bin:$(PATH)
export ZCCCFG := $(Z88DK)/lib/config

BUILD_DIR := build
SRC       := midiplayer.c midi_delay.asm
OUT       := $(BUILD_DIR)/midiplayer

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

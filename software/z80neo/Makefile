# z80neo bare-metal C example.
#
# Uses the in-tree z88dk (`../z88dk`) and its `+z80neo` target.  Override the
# toolchain location with:  make Z88DK=/path/to/z88dk
#
# Output: build/demo.ihx  (Intel HEX, for Z80NEO.INI BANK_PROG[0])

Z88DK   ?= $(abspath $(CURDIR)/../z88dk)
ZCC     := $(Z88DK)/bin/zcc

export PATH   := $(Z88DK)/bin:$(PATH)
export ZCCCFG := $(Z88DK)/lib/config

BUILD_DIR := build
SRC       := demo.c
OUT       := $(BUILD_DIR)/demo

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

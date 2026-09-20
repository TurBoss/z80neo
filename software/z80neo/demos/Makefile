# z80neo bare-metal graphics demos
#
# Uses the in-tree z88dk `+z80neo` target.  Override the toolchain location
# with:  make Z88DK=/path/to/z88dk
#
# Output: build/<name>.ihx  (Intel HEX, for Z80NEO.INI BANK_PROG[0])

Z88DK   ?= $(abspath $(CURDIR)/../../z88dk)
ZCC     := $(Z88DK)/bin/zcc

export PATH   := $(Z88DK)/bin:$(PATH)
export ZCCCFG := $(Z88DK)/lib/config

DEMOS   := stars cube lissajous wave balls bars
BUILD   := build

all: $(addprefix $(BUILD)/,$(addsuffix .ihx,$(DEMOS)))

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.ihx: %.c common.h | $(BUILD)
	$(ZCC) +z80neo $< -o $(BUILD)/$* -create-app

clean:
	rm -rf $(BUILD)

.PHONY: all clean

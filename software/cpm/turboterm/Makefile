# z80neo CP/M UART1 terminal client
#
# Built with the in-tree z88dk and its `+cpm` target.  Override the toolchain
# location with:  make Z88DK=/path/to/z88dk
#
# Output: build/TTERM.COM (CP/M transient, run from the A: prompt)
#
# CP/M names are 8.3, so the binary is TTERM (the project dir stays turboterm).
# Override the 8.3 stem with:  make NAME=FOO

Z88DK   ?= $(abspath $(CURDIR)/../../z88dk)
ZCC     := $(Z88DK)/bin/zcc

export PATH   := $(Z88DK)/bin:$(PATH)
export ZCCCFG := $(Z88DK)/lib/config

BUILD_DIR := build
SRC       := turboterm.c
NAME      ?= TTERM
OUT       := $(BUILD_DIR)/$(NAME)
COM       := $(BUILD_DIR)/$(NAME).COM

all: $(COM)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(COM): $(SRC) | $(BUILD_DIR)
	$(ZCC) +cpm $(SRC) -o $(OUT) -create-app
	$(RM) $(OUT)
	@echo "=== built $(COM) ==="
	@ls -la $(COM)

clean:
	$(RM) -rf $(BUILD_DIR)

.PHONY: all clean

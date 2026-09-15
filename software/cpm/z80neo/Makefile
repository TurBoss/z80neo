BUILD_DIR = build
AS        = z88dk-z80asm
ASFLAGS   = -mz80 -b

all: $(BUILD_DIR)/cpmz80neo.hex

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/boot.bin: boot.asm z80neo.inc | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -o$@ boot.asm

$(BUILD_DIR)/ccp_bdos.bin: ccp_bdos.asm z80neo.inc cpm22.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -o$@ ccp_bdos.asm

$(BUILD_DIR)/bios_only.bin: bios_only.asm z80neo.inc bios.asm console.asm disk.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -o$@ bios_only.asm

$(BUILD_DIR)/cpmz80neo.hex: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/ccp_bdos.bin $(BUILD_DIR)/bios_only.bin
	python3 patch_cpm.py $(BUILD_DIR)/ccp_bdos.bin
	srec_cat \
		$(BUILD_DIR)/boot.bin       -binary -offset=0x0000 \
		$(BUILD_DIR)/ccp_bdos.bin   -binary -offset=0xD400 \
		$(BUILD_DIR)/bios_only.bin  -binary -offset=0xEB00 \
		-o $@ -Intel -address-length=2 -obs=16
	@echo "=== CP/M 2.2 for z80neo built ==="
	@ls -la $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

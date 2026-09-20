PROG = mmu
TARGET = main

SRC_DIR = src
BUILD_DIR = build

SRC = $(SRC_DIR)/$(TARGET).asm
OUTPUT = $(BUILD_DIR)/$(PROG).bin
HEX = $(BUILD_DIR)/$(PROG).hex

ASSEMBLER = z88dk-z80asm
ASFLAGS = -mz80 -Iinclude -b



all: clean $(OUTPUT) $(HEX)


$(OUTPUT): $(SRC)
	$(ASSEMBLER) $(ASFLAGS) -o$(OUTPUT)  $(SRC)


$(HEX): $(OUTPUT)
	bin2hex.py $(OUTPUT) $(HEX)


# srec_cat $(BUILD_DIR)/echo_CODE.bin -binary -offset=0x0000 \
#          -output=$(HEX) -Intel -address-length=2 -obs=16


upload: $(HEX)
	cp $(HEX) /var/run/media/turboss/Z80NEO/$(PROG).hex


clean:
	rm -f $(BUILD_DIR)/$(TARGET).o $(BUILD_DIR)/$(PROG).bin $(BUILD_DIR)/$(PROG).hex

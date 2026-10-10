# NullVDP - FPGA VDP for z80neo
# Sipeed Tang Primer 20K (GW2A-LV18PG256C8/I7) + Dock
#
# Forked from the `dummy_terminal` example in
#   https://github.com/mikewolak/tangprimer20k-oss-examples
# (working 80x30 HDMI text terminal) and extended toward the z80neo
# VDP protocol (doc/vdp_protocol.md, common/z80neo_protocol.h).
#
# PREREQUISITE: OSS CAD Suite in PATH
#   source ~/Apps/oss-cad-suite/environment      (or ./build.sh)
#
# Targets: all (default), synth, pnr, pack, program, clean

YOSYS := yosys
NEXTPNR := nextpnr-himbaechel
GOWIN_PACK := gowin_pack
OPENFPGALOADER := openFPGALoader

# Project settings
PROJECT := nullvdp
TOP_MODULE := nullvdp_top
SEED ?= 7
DEVICE := GW2A-LV18PG256C8/I7
FAMILY := GW2A-18
SYNTH_FAMILY := gw2a
BOARD := tangprimer20k

# Sources
SRC_DIR := src
VERILOG_SRCS := $(SRC_DIR)/nullvdp_top.v \
                $(SRC_DIR)/hdmi_pll.v \
                $(SRC_DIR)/pll480.v \
                $(SRC_DIR)/tmds_serializer.v \
                $(SRC_DIR)/tmds_clock_serializer.v \
                $(SRC_DIR)/text_controller.v \
                $(SRC_DIR)/font_rom.v \
                $(SRC_DIR)/char_buffer.v \
                $(SRC_DIR)/attr_buffer.v \
                $(SRC_DIR)/gfx_buffer.v \
                $(SRC_DIR)/gfx_engine.v \
                $(SRC_DIR)/palette.v \
                $(SRC_DIR)/terminal_handler.v \
                $(SRC_DIR)/menu.v \
                $(SRC_DIR)/menu_rom.v \
                $(SRC_DIR)/overlay_ram.v \
                $(SRC_DIR)/ps2_host.v \
                $(SRC_DIR)/ps2_keys.v \
                $(SRC_DIR)/ps2_ascii.v \
                $(SRC_DIR)/ulpi_link.v \
                $(SRC_DIR)/ulpi_host.v \
                $(SRC_DIR)/usb_key_cdc.v \
                $(SRC_DIR)/hid_expand.v \
                $(SRC_DIR)/spi_slave.v \
                $(SRC_DIR)/sync_cursor.v

VERILOG_SV_SRCS := $(SRC_DIR)/tmds_channel.v

CONSTRAINTS := $(SRC_DIR)/nullvdp.cst

JSON := $(PROJECT).json
PNR_JSON := $(PROJECT)_pnr.json
BITSTREAM := $(PROJECT).fs

.PHONY: all synth pnr pack program ident ident-program spitest-program sim sim-menu sim-ps2 sim-usbkbd sim-ulpi sim-ulpi-host sim-spi sim-gfx sim-spi-gfx clean help

all: $(BITSTREAM)

synth: $(JSON)

$(JSON): $(VERILOG_SRCS)
	@echo "=== Synthesis with Yosys ==="
	$(YOSYS) -p "read_verilog $(VERILOG_SRCS); \
	             $(if $(VERILOG_SV_SRCS),read_verilog -sv $(VERILOG_SV_SRCS);) \
	             synth_gowin -top $(TOP_MODULE) -json $@ -family $(SYNTH_FAMILY)"

pnr: $(PNR_JSON)

$(PNR_JSON): $(JSON) $(CONSTRAINTS)
	@echo "=== Place and Route with nextpnr-himbaechel ==="
	$(NEXTPNR) --timing-allow-fail --seed $(SEED) --json $(JSON) \
	           --write $@ \
	           --device $(DEVICE) \
	           --vopt family=$(FAMILY) \
	           --vopt cst=$(CONSTRAINTS)

pack: $(BITSTREAM)

$(BITSTREAM): $(PNR_JSON)
	@echo "=== Packing bitstream ==="
	$(GOWIN_PACK) -d $(FAMILY) -o $@ $(PNR_JSON)

program: $(BITSTREAM)
	@echo "=== Programming FPGA ==="
	$(OPENFPGALOADER) -b $(BOARD) $(BITSTREAM)

# Wire-identification bitstream (drives the four SPI balls with distinct slow
# square waves; no HDMI).  Program it, use the z80neo/LEDs/meter to locate the
# balls, then re-program the normal bitstream.
ident:
	$(MAKE) PROJECT=nullvdp_ident TOP_MODULE=nullvdp_ident \
		VERILOG_SRCS="$(SRC_DIR)/nullvdp_ident.v" VERILOG_SV_SRCS="" \
		CONSTRAINTS="$(SRC_DIR)/nullvdp_ident.cst" all

ident-program:
	$(MAKE) PROJECT=nullvdp_ident TOP_MODULE=nullvdp_ident \
		VERILOG_SRCS="$(SRC_DIR)/nullvdp_ident.v" VERILOG_SV_SRCS="" \
		CONSTRAINTS="$(SRC_DIR)/nullvdp_ident.cst" program

# Minimal SPI-slave-only build (isolates the slave + wiring).
spitest-program:
	$(MAKE) PROJECT=nullvdp_spitest TOP_MODULE=nullvdp_spitest \
		VERILOG_SRCS="$(SRC_DIR)/nullvdp_spitest.v $(SRC_DIR)/spi_slave.v" \
		VERILOG_SV_SRCS="" CONSTRAINTS="$(SRC_DIR)/nullvdp_ident.cst" program

# Logic simulation of the terminal handler (requires iverilog)
sim:
	@echo "=== Simulating terminal_handler ==="
	iverilog -g2005 -o tb_terminal sim/tb_terminal.v $(SRC_DIR)/terminal_handler.v
	vvp tb_terminal
	@rm -f tb_terminal

sim-menu:
	@echo "=== Simulating the menu overlay ==="
	iverilog -g2005 -o tb_menu sim/tb_menu.v $(SRC_DIR)/menu.v $(SRC_DIR)/menu_rom.v $(SRC_DIR)/overlay_ram.v
	vvp tb_menu
	@rm -f tb_menu

sim-ps2:
	@echo "=== Simulating PS/2 keyboard decode ==="
	iverilog -g2005 -o tb_ps2 sim/tb_ps2.v $(SRC_DIR)/ps2_host.v $(SRC_DIR)/ps2_keys.v $(SRC_DIR)/ps2_ascii.v
	vvp tb_ps2
	@rm -f tb_ps2

sim-usbkbd:
	@echo "=== Simulating USB HID keyboard decode ==="
	iverilog -g2005 -o tb_usbkbd sim/tb_usbkbd.v $(SRC_DIR)/usb_key_cdc.v $(SRC_DIR)/hid_expand.v
	vvp tb_usbkbd
	@rm -f tb_usbkbd

sim-ulpi:
	@echo "=== Simulating ULPI register read/write ==="
	iverilog -g2005 -I src -o tb_ulpi sim/ulpi_phy_model.v sim/tb_ulpi_reg.v $(SRC_DIR)/ulpi_link.v
	vvp tb_ulpi
	@rm -f tb_ulpi

sim-ulpi-host:
	@echo "=== Simulating ULPI host packet generation ==="
	iverilog -g2005 -I src -o tb_uh sim/tb_ulpi_host.v sim/ulpi_phy_model.v $(SRC_DIR)/ulpi_link.v $(SRC_DIR)/ulpi_host.v
	vvp tb_uh
	@rm -f tb_uh

sim-spi:
	@echo "=== Simulating the SPI link slave ==="
	iverilog -g2005 -o tb_spi sim/tb_spi.v $(SRC_DIR)/spi_slave.v
	vvp tb_spi
	@rm -f tb_spi

sim-gfx:
	@echo "=== Simulating the ESC g graphics path ==="
	iverilog -g2005 -o tb_gfx sim/tb_gfx.v \
	    $(SRC_DIR)/terminal_handler.v $(SRC_DIR)/gfx_engine.v $(SRC_DIR)/gfx_buffer.v
	vvp tb_gfx
	@rm -f tb_gfx

sim-spi-gfx:
	@echo "=== Simulating SPI to ESC g graphics path ==="
	iverilog -g2005 -o tb_spi_gfx sim/tb_spi_gfx.v \
	    $(SRC_DIR)/spi_slave.v $(SRC_DIR)/terminal_handler.v \
	    $(SRC_DIR)/gfx_engine.v $(SRC_DIR)/gfx_buffer.v $(SRC_DIR)/char_buffer.v
	@echo "Provide /tmp/opencode/stream.hex to replay a byte stream"
	vvp tb_spi_gfx
	@rm -f tb_spi_gfx

clean:
	rm -f $(JSON) $(PNR_JSON) $(BITSTREAM) tb_menu tb_usbkbd
	rm -f *.rpt *.json abc.history

help:
	@echo "NullVDP - Tang Primer 20K open-source build"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build complete bitstream (default)"
	@echo "  synth    - Synthesis only"
	@echo "  pnr      - Place and route only"
	@echo "  pack     - Pack bitstream only"
	@echo "  program  - Program the FPGA (openFPGALoader)"
	@echo "  sim      - Simulate terminal_handler with iverilog"
	@echo "  clean    - Remove build artifacts"
	@echo ""
	@echo "Device: $(DEVICE)   Family: $(FAMILY)   Board: $(BOARD)"

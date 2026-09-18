// display.c
// Z80NEO Firmware — Display & UI module
// TurBoss 2026

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pico/stdlib.h>
#include <pico/time.h>
#include <hardware/adc.h>
#include <hardware/gpio.h>

#include "ff.h"
#include "tf_card.h"

#include "display.h"
#include "memory.h"
#include "logo.h"
#include "z80neo_logo.h"
#include "cpm_disk.h"
#include "z80bus_pio.h"

// ===========================================================================
// Globals
// ===========================================================================

struct render_area frame_area = {
    .start_col  = 0,
    .end_col    = SSD1306_WIDTH - 1,
    .start_page = 0,
    .end_page   = SSD1306_NUM_PAGES - 1
};

// Display buffer (also used as SD work buffer for DMA alignment)
uint32_t buf32[(BUF_SIZE + 3) / 4];
uint8_t *buf = (uint8_t *)buf32;

// Aliases — all point to buf32
uint8_t *disp_buf_0 = (uint8_t *)buf32;
uint8_t *disp_buf_1 = (uint8_t *)buf32;
uint8_t *disp_buf_2 = (uint8_t *)buf32;
uint8_t *disp_buf_3 = (uint8_t *)buf32;
uint8_t *disp_buf_4 = (uint8_t *)buf32;
uint8_t *disp_buf_5 = (uint8_t *)buf32;
uint8_t *disp_buf_6 = (uint8_t *)buf32;
uint8_t *disp_buf_7 = (uint8_t *)buf32;

// Text lines (screen buffer)
char line1[TEXT_BUFFER_SIZE];
char line2[TEXT_BUFFER_SIZE];
char line3[TEXT_BUFFER_SIZE];
char line4[TEXT_BUFFER_SIZE];
char line5[TEXT_BUFFER_SIZE];
char line6[TEXT_BUFFER_SIZE];
char line7[TEXT_BUFFER_SIZE];
char line8[TEXT_BUFFER_SIZE];

char *screen[LINES] = {line1, line2, line3, line4, line5, line6, line7, line8};

char text_buffer[TEXT_BUFFER_SIZE];
char serial_text_buffer[TEXT_BUFFER_SIZE];

char line_buffer0[24];
char line_buffer1[24];
char line_buffer2[24];
char line_buffer3[24];
char line_buffer4[24];
char line_buffer5[24];
char line_buffer6[24];
char line_buffer7[24];

char tbmon_text_buffer[8][32];

const char *hexStringChar[] = {"0", "1", "2", "3", "4", "5", "6", "7",
                                "8", "9", "a", "b", "C", "d", "E", "F"};

char *load_chars = "|/-\\-|-//";
uint8_t load_char_index = 0;

display_line file;

volatile bool tbmon        = false;
volatile bool tbmon_loaded = false;
uint16_t tbmon_idx         = 0;

volatile bool confirmed = false;

// ===========================================================================
// Static display buffer used during main() init
// ===========================================================================
static uint8_t display_buf[SSD1306_BUF_LEN];

// ===========================================================================
// Helpers
// ===========================================================================

int center_string(char *string) {
    int n = strlen(string);
    int l = 8 - (n / 2);
    return l < 0 ? 0 : l;
}

// ===========================================================================
// Screen clearing
// ===========================================================================

void clear_screen(void) {
    memset(buf, 0, SSD1306_BUF_LEN);
    render(buf, &frame_area);

    memset(line1, 0, TEXT_BUFFER_SIZE);
    memset(line2, 0, TEXT_BUFFER_SIZE);
    memset(line3, 0, TEXT_BUFFER_SIZE);
    memset(line4, 0, TEXT_BUFFER_SIZE);
    memset(line5, 0, TEXT_BUFFER_SIZE);
    memset(line6, 0, TEXT_BUFFER_SIZE);
    memset(line7, 0, TEXT_BUFFER_SIZE);
    memset(line8, 0, TEXT_BUFFER_SIZE);
}

void clear_screen0(void) {
    memset(buf, 0, SSD1306_BUF_LEN);

    memset(line1, 0, TEXT_BUFFER_SIZE);
    memset(line2, 0, TEXT_BUFFER_SIZE);
    memset(line3, 0, TEXT_BUFFER_SIZE);
    memset(line4, 0, TEXT_BUFFER_SIZE);
    memset(line5, 0, TEXT_BUFFER_SIZE);
    memset(line6, 0, TEXT_BUFFER_SIZE);
    memset(line7, 0, TEXT_BUFFER_SIZE);
    memset(line8, 0, TEXT_BUFFER_SIZE);
}

void clear_line0(int line) {
    if (line < 0 || line >= LINES) return;
    memset(screen[line], 0, TEXT_BUFFER_SIZE);
    strcpy(screen[line], "                ");
    WriteString(buf, 0, line * 8, screen[line]);
}

void clear_line(int line) {
    clear_line0(line);
    render(buf, &frame_area);
}

// ===========================================================================
// String output
// ===========================================================================

void print_string0(int x, int y, char *text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
    strcpy(screen[y], text_buffer);
    WriteString(buf, x * 8, y * 8, text_buffer);
    va_end(args);
}

void print_string(int x, int y, char *text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
    strcpy(screen[y], text_buffer);
    WriteString(buf, x * 8, y * 8, text_buffer);
    render(buf, &frame_area);
}

void print_line(int x, char *text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
    char *screen0 = screen[0];
    for (int i = 0; i < 7; i++) {
        screen[i] = screen[i + 1];
        WriteString(buf, x * 8, i * 8, screen[i]);
    }
    screen[7] = screen0;
    strcpy(screen[7], text_buffer);
    WriteString(buf, x * 8, 7 * 8, text_buffer);
    render(buf, &frame_area);
    va_end(args);
}

void print_line0(int x, char *text, ...) {
    va_list args;
    va_start(args, text);
    vsnprintf(text_buffer, TEXT_BUFFER_SIZE, text, args);
    char *screen0 = screen[0];
    for (int i = 0; i < 3; i++) {
        screen[i] = screen[i + 1];
        WriteString(buf, x * 8, i * 8, screen[i]);
    }
    screen[3] = screen0;
    strcpy(screen[3], text_buffer);
    WriteString(buf, x * 8, 3 * 8, text_buffer);
    va_end(args);
}

// ===========================================================================
// Character output
// ===========================================================================

void print_char0(int x, int y, char c) {
    text_buffer[0] = c;
    text_buffer[1] = 0;
    WriteString(buf, x * 8, y * 8, text_buffer);
}

void print_char(int x, int y, char c) {
    text_buffer[0] = c;
    text_buffer[1] = 0;
    WriteString(buf, x * 8, y * 8, text_buffer);
    render(buf, &frame_area);
}

// ===========================================================================
// Pixel plotting
// ===========================================================================

void disp_plot0(int x, int y) { SetPixel(buf, x, y, true); }

void plot_pixel(int x, int y) {
    SetPixel(buf, x, y, true);
    render(buf, &frame_area);
}

void unplot_pixel(int x, int y) {
    SetPixel(buf, x, y, false);
    render(buf, &frame_area);
}

// ===========================================================================
// Line drawing
// ===========================================================================

void disp_line0(int x1, int y1, int x2, int y2) {
    DrawLine(buf, x1, y1, x2, y2, true);
}

void disp_line(int x1, int y1, int x2, int y2) {
    DrawLine(buf, x1, y1, x2, y2, true);
    render(buf, &frame_area);
}

void unplot_line(int x1, int y1, int x2, int y2) {
    DrawLine(buf, x1, y1, x2, y2, false);
    render(buf, &frame_area);
}

void render_display(void) { render(buf, &frame_area); }

// ===========================================================================
// RAM viewer
// ===========================================================================

void display_ram_viewer(void) {
    for (int line = 0; line < LINES; line++) {
        int offset = tbmon_idx + (line * BYTES_PER_ROW);
        char byte_data[8];

        sprintf(tbmon_text_buffer[line], "%04x ", offset & 0xFFFF);

        for (int col = 0; col < BYTES_PER_ROW; col++) {
            uint8_t v = ram[cur_bank][(offset + col) & 0x3FFF];
            if (col < 3)
                sprintf(byte_data, "%02x:", v);
            else
                sprintf(byte_data, "%02x", v);
            strcat(tbmon_text_buffer[line], byte_data);
        }

        print_string(0, line, tbmon_text_buffer[line]);
    }
}

// ===========================================================================
// Button I/O
// ===========================================================================

button_state read_button_state(void) {
    adc_select_input(0);
    uint16_t adc = adc_read();

    if (adc <= UP_ADC) {
        return UP;
    } else if (adc <= DOWN_ADC) {
        return DOWN;
    } else if (adc <= BACK_ADC) {
        return BACK;
    } else if (adc <= OK_ADC) {
        return OK;
    } else if (adc <= CANCEL_ADC) {
        return CANCEL;
    } else {
        return NONE;
    }
}

// Debounced read for the main-loop trigger.  A single noisy ADC sample must
// never reset the Z80, so require the same non-NONE state on two reads ~10 ms
// apart.
button_state read_button_state_debounced(void) {
    button_state a = read_button_state();
    if (a == NONE) return NONE;
    sleep_ms(10);
    return (read_button_state() == a) ? a : NONE;
}

bool wait_for_button_release(void) {
    uint64_t last = time_us_64();
    while (read_button_state() != NONE) {
    }
    return (time_us_64() - last > LONG_BUTTON_DELAY);
}

void wait_for_button(void) {
    while (read_button_state() == NONE) {
    }
    wait_for_button_release();
    return;
}

bool wait_for_yes_no_button(void) {
    button_state button;
    while (true) {
        button = read_button_state();
        if (button == OK) {
            wait_for_button_release();
            return true;
        } else if (button == CANCEL || button == CANCEL2) {
            wait_for_button_release();
            return false;
        }
    }
}

// ===========================================================================
// Menu / pages (core 1)
//
// The Z80 keeps running while the menu is open, so the pages read live globals.
// Destructive actions (bank select/clear, file load) pause it first via the
// disabled/confirmed handshake so core 0 stops the bus and clock.
// ===========================================================================

typedef enum {
    PAGE_SYSTEM = 0, PAGE_CPU, PAGE_BUS, PAGE_MEM,
    PAGE_DISK, PAGE_DIAG, PAGE_FILES
} page_id_t;

static const char *const menu_labels[] = {
    "System", "CPU / Clock", "Bus / Timing", "Memory & Banks",
    "Disk Drives", "Diagnostics", "Files / Load"
};
#define MENU_ITEMS   ((int)(sizeof(menu_labels) / sizeof(menu_labels[0])))
#define MENU_VISIBLE 6
#define UI_TICK      8

static bool ui_home = true;      // idle: show the text logo, not the menu
static int  ui_menu_sel = 0, ui_menu_top = 0;
static int  ui_page = -1, ui_page_sel = 0;
static bool ui_arm_clear = false;
static button_state ui_last_btn = NONE;
static int  ui_file_idx = 1, ui_file_count = 0;

static void ui_z80_pause(void) {
    reset_hold();
    confirmed = false;
    disabled = true;
    while (!confirmed) tight_loop_contents();
}

static void ui_z80_resume(void) {
    disabled = false;
    reset_release();
}

static void ui_render(char lines[LINES][24]) {
    clear_screen0();
    for (int i = 0; i < LINES; i++)
        if (lines[i][0]) WriteString(buf, 0, i * 8, lines[i]);
    render(buf, &frame_area);
}

static void ui_draw_logo(void) {
    // Full-screen 128x64 bitmap generated by tools/img_to_array.py
    // (source: tools/z80neo.svg), installed as include/z80neo.h.
    render(z80neo, &frame_area);
}

static void ui_draw_menu(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== Z80 %lukHz ==", (unsigned long)(CPU_SPEED / 1000.0f));
    for (int i = 0; i < MENU_VISIBLE; i++) {
        int idx = ui_menu_top + i;
        if (idx >= MENU_ITEMS) break;
        snprintf(l[i + 1], 24, "%c%-15.15s",
                 idx == ui_menu_sel ? '>' : ' ', menu_labels[idx]);
    }
    snprintf(l[7], 24, "OK:open  C:close");
    ui_render(l);
}

static void ui_draw_system(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== SYSTEM ==");
    snprintf(l[1], 24, "%s", VERSION);
    snprintf(l[2], 24, "%.16s", MACHINE);
    snprintf(l[3], 24, "BANKS %dx16K", MAX_BANKS);
    snprintf(l[4], 24, "PSRAM %s", psram_base ? "OK" : "none");
    snprintf(l[5], 24, "CLK %lu kHz", (unsigned long)(CPU_SPEED / 1000.0f));
    snprintf(l[6], 24, "DUTY %lu%%", (unsigned long)(CPU_DUTY * 100.0f + 0.5f));
    snprintf(l[7], 24, "OK/BACK: menu");
    ui_render(l);
}

static void ui_draw_cpu(void) {
    char l[LINES][24], s[24];
    memset(l, 0, sizeof(l));
    z80bus_pio_settle_str(s, sizeof(s));
    snprintf(l[0], 24, "== CPU / CLOCK ==");
    snprintf(l[1], 24, "FREQ %lu Hz", (unsigned long)CPU_SPEED);
    snprintf(l[2], 24, "DUTY %lu%%", (unsigned long)(CPU_DUTY * 100.0f + 0.5f));
    snprintf(l[3], 24, "SET %s", s);
    snprintf(l[4], 24, "MAX %lu us", (unsigned long)diag_max_service_us);
    snprintf(l[5], 24, "CYC %lu", (unsigned long)pio_cycles_total);
    snprintf(l[6], 24, "IO %lu", (unsigned long)pio_cycles_io);
    snprintf(l[7], 24, "OK/BACK: menu");
    ui_render(l);
}

static void ui_draw_bus(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== BUS / TIMING ==");
    snprintf(l[1], 24, "TOT %lu", (unsigned long)pio_cycles_total);
    snprintf(l[2], 24, "RD %lu", (unsigned long)pio_cycles_read);
    snprintf(l[3], 24, "WR %lu", (unsigned long)pio_cycles_write);
    snprintf(l[4], 24, "IO %lu", (unsigned long)pio_cycles_io);
    snprintf(l[5], 24, "REF %lu", (unsigned long)pio_cycles_refresh);
    snprintf(l[6], 24, "TO %lu/%lu", (unsigned long)diag_mem_timeouts,
             (unsigned long)diag_io_timeouts);
    snprintf(l[7], 24, "OK/BACK: menu");
    ui_render(l);
}

static void ui_draw_mem(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== MEMORY/BANKS ==");
    for (int i = 0; i < MAX_BANKS; i++)
        snprintf(l[i + 1], 24, "%c B%d %.11s",
                 (i == ui_page_sel) ? '>' : ' ', i, BANK_PROG[i]);
    snprintf(l[5], 24, "SEL B%d", cur_bank);
    if (ui_arm_clear)
        snprintf(l[6], 24, "BACK=CLR B%d!", ui_page_sel);
    else
        snprintf(l[6], 24, "OK:set B%d", ui_page_sel);
    snprintf(l[7], 24, "C:menu BACK:clr");
    ui_render(l);
}

static void ui_draw_disk(void) {
    char l[LINES][24];
    uint8_t drv, trk, sec;
    memset(l, 0, sizeof(l));
    cpm_disk_state(&drv, &trk, &sec);
    snprintf(l[0], 24, "== DISK DRIVES ==");
    snprintf(l[1], 24, "MOUNT %u", (unsigned)cpm_disk_count());
    snprintf(l[2], 24, "DRIVE %c", (drv < 26) ? ('A' + drv) : '?');
    snprintf(l[3], 24, "TRK %03u SEC %02u", trk, sec);
    snprintf(l[4], 24, "GEOM 254x26 1K");
    snprintf(l[5], 24, "IMG CPMDISK0-14");
    snprintf(l[6], 24, "C%u/%u drives", (unsigned)cpm_disk_count(), (unsigned)CPM_DRIVES);
    snprintf(l[7], 24, "OK/BACK: menu");
    ui_render(l);
}

static void ui_draw_diag(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== DIAGNOSTICS ==");
    snprintf(l[1], 24, "%cRAM viewer", ui_page_sel == 0 ? '>' : ' ');
    snprintf(l[2], 24, "%cDump bus trace", ui_page_sel == 1 ? '>' : ' ');
    snprintf(l[3], 24, "%cReset counters", ui_page_sel == 2 ? '>' : ' ');
    snprintf(l[6], 24, "UP/DN OK:run");
    snprintf(l[7], 24, "CANCEL: menu");
    ui_render(l);
}

static void ui_draw_files(void) {
    char l[LINES][24];
    memset(l, 0, sizeof(l));
    snprintf(l[0], 24, "== FILES / LOAD ==");
    if (ui_file_count) {
        snprintf(l[1], 24, "%d / %d", ui_file_idx, ui_file_count);
        snprintf(l[2], 24, "%.16s", file);
    } else {
        snprintf(l[1], 24, "No HEX/IHX files");
        snprintf(l[2], 24, "-");
    }
    snprintf(l[3], 24, "OK: load");
    snprintf(l[4], 24, "UP/DN: pick");
    snprintf(l[5], 24, "BACK: rescan");
    snprintf(l[6], 24, "CANCEL: menu");
    ui_render(l);
}

static void ui_draw_page(void) {
    if (ui_home) { ui_draw_logo(); return; }
    switch (ui_page) {
    case PAGE_SYSTEM: ui_draw_system(); break;
    case PAGE_CPU:    ui_draw_cpu();    break;
    case PAGE_BUS:    ui_draw_bus();    break;
    case PAGE_MEM:    ui_draw_mem();    break;
    case PAGE_DISK:   ui_draw_disk();   break;
    case PAGE_DIAG:   ui_draw_diag();   break;
    case PAGE_FILES:  ui_draw_files();  break;
    default:          ui_draw_menu();   break;
    }
}

static void ui_menu_move(int d) {
    ui_menu_sel += d;
    if (ui_menu_sel < 0) ui_menu_sel = MENU_ITEMS - 1;
    if (ui_menu_sel >= MENU_ITEMS) ui_menu_sel = 0;
    if (ui_menu_sel < ui_menu_top) ui_menu_top = ui_menu_sel;
    if (ui_menu_sel >= ui_menu_top + MENU_VISIBLE)
        ui_menu_top = ui_menu_sel - MENU_VISIBLE + 1;
}

static void ui_files_scan(void) {
    ui_z80_pause();
    ui_file_count = count_files();
    if (ui_file_idx > ui_file_count) ui_file_idx = ui_file_count ? ui_file_count : 1;
    if (ui_file_idx < 1) ui_file_idx = 1;
    if (ui_file_count) select_file_no(ui_file_idx);
    cpm_disk_init();          // browsing re-mounts the volume; restore cpm handles
    ui_z80_resume();
}

static void ui_files_step(int d) {
    if (!ui_file_count) return;
    ui_file_idx += d;
    if (ui_file_idx < 1) ui_file_idx = ui_file_count;
    if (ui_file_idx > ui_file_count) ui_file_idx = 1;
    ui_z80_pause();
    select_file_no(ui_file_idx);
    cpm_disk_init();
    ui_z80_resume();
}

static void ui_files_load(void) {
    if (!ui_file_count) return;
    ui_z80_pause();
    if (select_file_no(ui_file_idx) == ui_file_idx) {
        load_file(false);
        cpm_disk_init();                 // load_file() unmounted "0:"
    }
    ui_z80_resume();
}

static void ui_mem_action(button_state b) {
    if (b == UP) {
        ui_page_sel = (ui_page_sel + MAX_BANKS - 1) % MAX_BANKS;
        ui_arm_clear = false;
    } else if (b == DOWN) {
        ui_page_sel = (ui_page_sel + 1) % MAX_BANKS;
        ui_arm_clear = false;
    } else if (b == OK) {
        ui_z80_pause();
        cur_bank = ui_page_sel;
        ui_z80_resume();
        ui_arm_clear = false;
    } else if (b == BACK) {
        if (!ui_arm_clear) {
            ui_arm_clear = true;
        } else {
            ui_z80_pause();
            clear_bank(ui_page_sel);
            ui_z80_resume();
            ui_arm_clear = false;
        }
    } else {
        ui_arm_clear = false;
    }
    ui_draw_mem();
}

static void ui_diag_action(button_state b) {
    if (b == UP) {
        ui_page_sel = (ui_page_sel + 2) % 3;
    } else if (b == DOWN) {
        ui_page_sel = (ui_page_sel + 1) % 3;
    } else if (b == OK) {
        switch (ui_page_sel) {
        case 0:
            tbmon = true;
            tbmon_idx = 0;
            tbmon_loaded = false;
            display_ram_viewer();
            return;
        case 1: z80bus_diag_dump = true; break;
        case 2: z80bus_diag_reset = true; break;
        }
    }
    ui_draw_diag();
}

static void ui_open_page(int page) {
    ui_page = page;
    ui_page_sel = 0;
    ui_arm_clear = false;
    if (page == PAGE_FILES) ui_files_scan();
    ui_draw_page();
}

static void ui_page_action(button_state b) {
    if (b == CANCEL) {
        ui_page = -1;
        ui_arm_clear = false;
        ui_draw_menu();
        return;
    }
    switch (ui_page) {
    case PAGE_MEM:
        ui_mem_action(b);
        return;
    case PAGE_DIAG:
        ui_diag_action(b);
        return;
    case PAGE_FILES:
        if (b == UP)        ui_files_step(-1);
        else if (b == DOWN) ui_files_step(+1);
        else if (b == OK)   ui_files_load();
        else if (b == BACK) ui_files_scan();
        ui_draw_files();
        return;
    default:
        if (b == OK || b == BACK) {
            ui_page = -1;
            ui_draw_menu();
        }
        return;
    }
}

static void ui_handle_button(button_state b) {
    if (b == CANCEL2) b = CANCEL;
    if (b == NONE || b == ui_last_btn) {
        ui_last_btn = b;
        return;
    }

    if (ui_home) {
        // Any button opens the menu from the idle logo screen.
        ui_home = false;
        ui_page = -1;
        ui_draw_menu();
    } else if (tbmon) {
        if (b == UP) {
            tbmon_idx = (tbmon_idx >= BYTES_PER_ROW)
                        ? tbmon_idx - BYTES_PER_ROW : 0;
            display_ram_viewer();
        } else if (b == DOWN) {
            tbmon_idx += BYTES_PER_ROW;
            if (tbmon_idx > RAM_SIZE - (BYTES_PER_ROW * LINES)) tbmon_idx = 0;
            display_ram_viewer();
        } else {
            tbmon = false;
            tbmon_loaded = false;
            tbmon_idx = 0;
            ui_page = -1;
            ui_draw_menu();
        }
    } else if (ui_page < 0) {
        switch (b) {
        case UP:   ui_menu_move(-1); ui_draw_menu(); break;
        case DOWN: ui_menu_move(+1); ui_draw_menu(); break;
        case OK:   ui_open_page(ui_menu_sel); break;
        case CANCEL: ui_home = true; ui_draw_logo(); break;
        default: break;
        }
    } else {
        ui_page_action(b);
    }
    ui_last_btn = b;
}

// ===========================================================================
// Main display loop (runs on core 1)
// ===========================================================================

void display_loop(void) {
    enable_fpu();
    button_state buttons = NONE;
    bool buttons_enabled = false;   // keypad verified to rest at NONE
    bool buttons_checked = false;

    // ADC debug mode
    if (DEBUG_ADC) {
        reset_hold();
        clear_screen();
        uint16_t adc = adc_read();

        while (true) {
            adc_select_input(0);
            print_string(0, 1, "ADC:%03x       ", adc_read());
            sleep_ms(10);
        }
    }

    while (true) {
        static uint8_t fast_frame = 0;

        // The menu is the whole UI.  Redraw the current view periodically so the
        // live pages stay fresh; the RAM viewer refreshes on scroll only.
        if (!tbmon && ((fast_frame & (UI_TICK - 1)) == 0))
            ui_draw_page();

        fast_frame++;

        // Once boot is done, confirm the keypad actually rests at NONE before
        // trusting it.  A disconnected or floating ADC input reads as a button,
        // which would reset the Z80 every iteration and keep it in a reboot
        // loop; in that case disable button handling entirely.
        if (system_up && !buttons_checked) {
            buttons_checked = true;
            int none_reads = 0;
            for (int i = 0; i < 16; i++) {
                if (read_button_state() == NONE) none_reads++;
                sleep_ms(5);
            }
            buttons_enabled = (none_reads == 16);
        }

        buttons = read_button_state_debounced();

        // Only handle buttons once boot has finished and released the Z80, and
        // only if the keypad is trustworthy.  ui_handle_button() is
        // non-blocking: it acts on the press edge and returns.
        if (system_up && buttons_enabled)
            ui_handle_button(buttons);
        else
            ui_last_btn = buttons;
    }
}

// ===========================================================================
// Boot / info screens
// ===========================================================================

void show_logo(void) {
    clear_screen();
    render(logo, &frame_area);
}

void boot_screen(void) {
    clear_screen();
    print_string(center_string("--------------"), 0, "--------------");
    print_string(center_string("Z80NEO"), 2, "Z80NEO");
    print_string(center_string(VERSION), 3, VERSION);
    print_string(center_string(MACHINE), 4, MACHINE);
    print_string(center_string("TURBOSS - 2026"), 5, "TURBOSS - 2026");
    print_string(center_string("--------------"), 7, "--------------");
}

void show_info(void) {
    clear_screen();
    print_string(0, 0, "%s", BANK_PROG[cur_bank]);
    print_string(0, 1, "BANKS:%d SIZE:%04x", MAX_BANKS, RAM_SIZE);
    print_string(0, 2, "CPU:%s", MACHINE);
    print_string(0, 3, "BANK:%d", cur_bank);
}

// ===========================================================================
// Error screens
// ===========================================================================

void show_error_and_halt(char *err) {
    clear_screen();
    print_string(0, 0, err);
    while (true)
        ;
}

void show_error(int b, int a, char *err) {
    clear_screen();
    print_string(a, b, err);
    sleep_ms(DISPLAY_DELAY_LONG);
    sleep_ms(DISPLAY_DELAY_LONG);
    return;
}

void show_error_wait_for_button(char *err) {
    show_error(0, 0, err);
    wait_for_button_release();
    return;
}

// ===========================================================================
// File selector UI
// ===========================================================================

void clear_file_buffer(void) {
    for (int i = 0; i <= 16; i++)
        file[i] = 0;
}

int count_files(void) {
    int count = 0;
    char const *p_dir;

    p_dir = init_and_mount_sd_card();

    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);

    FRESULT fr = f_opendir(&dj, p_dir);
    if (FR_OK != fr) {
        show_error(0, 0, "Count Files ERR");
        return 0;
    }

    while (f_readdir(&dj, &fno) == FR_OK && fno.fname[0]) {
        if (!(fno.fattrib & AM_DIR) && is_prog_file(fno.fname))
            count++;
    }

    f_closedir(&dj);
    return count;
}

int select_file_no(int no) {
    int count = 0;
    clear_file_buffer();

    char const *p_dir;
    p_dir = init_and_mount_sd_card();

    DIR dj;
    FILINFO fno;
    memset(&dj, 0, sizeof dj);
    memset(&fno, 0, sizeof fno);

    FRESULT fr = f_opendir(&dj, p_dir);
    if (FR_OK != fr) {
        show_error(0, 0, "File Sel ERR");
        return 0;
    }

    while (f_readdir(&dj, &fno) == FR_OK && fno.fname[0]) {
        if (!(fno.fattrib & AM_DIR) && is_prog_file(fno.fname)) {
            count++;
            if (count == no) {
                strcpy(file, fno.fname);
                return count;
            }
        }
    }

    f_closedir(&dj);
    return 0;
}

int select_file(void) {
    clear_screen();
    int no = 1;
    int count = count_files();
    select_file_no(no);

    uint64_t last = time_us_64();
    bool blink = false;

    wait_for_button_release();

    while (read_button_state() != OK) {
        print_string(0, 0, "Load %2d of %2d", no, count);
        print_string(0, 3, "CANCEL OR OK");

        if (time_us_64() - last > BLINK_DELAY) {
            last = time_us_64();
            blink = !blink;

            if (blink)
                print_string(0, 1, "                ");
            else
                print_string(0, 1, file);
        }

        switch (read_button_state()) {
        case UP:
            wait_for_button_release();
            if (no < count)
                no = select_file_no(no + 1);
            else
                no = select_file_no(1);
            break;
        case DOWN:
            wait_for_button_release();
            if (no > 1)
                no = select_file_no(no - 1);
            else
                no = select_file_no(count);
            break;
        case CANCEL:
        case CANCEL2:
            wait_for_button_release();
            return -1;
        default:
            break;
        }
    }

    wait_for_button_release();
    return 0;
}

int create_name(void) {
    clear_screen();
    wait_for_button_release();
    clear_file_buffer();

    file[0] = 'A';
    file[1] = 0;

    int cursor = 0;
    uint64_t last = time_us_64();
    bool blink = false;

    read_button_state();

    while (read_button_state() != OK) {
        print_string(0, 0, "Save HEX - Name:");
        print_string(0, 3, "CANCEL OR OK");

        if (time_us_64() - last > BLINK_DELAY) {
            last = time_us_64();
            print_string0(0, 1, file);
            blink = !blink;

            if (blink)
                print_char(cursor, 1, '_');
            else
                print_char(cursor, 1, file[cursor]);
        }

        switch (read_button_state()) {
        case UP:
            if (file[cursor] < 127) {
                file[cursor]++;
                wait_for_button_release();
            }
            break;
        case DOWN:
            if (file[cursor] > 32) {
                file[cursor]--;
                wait_for_button_release();
            }
            break;
        case BACK:
            if (!wait_for_button_release()) {
                if (cursor < 7) {
                    cursor++;
                    if (!file[cursor])
                        file[cursor] = file[cursor - 1];
                }
            } else {
                if (cursor > 0) {
                    cursor--;
                }
            }
            break;
        case CANCEL:
        case CANCEL2:
            wait_for_button_release();
            return -1;
        default:
            break;
        }
    }

    cursor++;
    file[cursor++] = '.';
    file[cursor++] = 'H';
    file[cursor++] = 'E';
    file[cursor++] = 'X';
    file[cursor++] = 0;

    clear_screen();
    wait_for_button_release();
    return 0;
}

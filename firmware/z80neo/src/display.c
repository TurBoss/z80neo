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
// Main display loop (runs on core 1)
// ===========================================================================

void display_loop(void) {
    disp_mode cur_disp_mode = ON;
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

    // Partial render area for the fast-updating monitor lines (pages 4-7).
    struct render_area status_area = {
        .start_col = 0,
        .end_col = SSD1306_WIDTH - 1,
        .start_page = 4,
        .end_page = 7
    };
    calc_render_area_buflen(&status_area);

    while (true) {
        static uint8_t fast_frame = 0;
        if (cur_disp_mode != OFF) {
            // Main page.  Slow lines refresh on the full frame; the fast lines
            // (pages 4-7) render every iteration via status_area.  16-char limit.
            if ((fast_frame & 0x1F) == 0) {
                uint8_t drv, trk, sec;
                cpm_disk_state(&drv, &trk, &sec);

                sprintf(line_buffer0, "Z80NEO %3lukHz",
                        (unsigned long)(CPU_SPEED / 1000.0f));
                WriteString(buf, 0, 0 * 8, line_buffer0);

                sprintf(line_buffer1, "R%04lx W%04lx I%03lx",
                        dr_op & 0xFFFF, dw_op & 0xFFFF, io_op & 0xFFF);
                WriteString(buf, 0, 1 * 8, line_buffer1);

                sprintf(line_buffer2, "BNK %d DSK %c",
                        cur_bank, (drv < 26) ? ('A' + drv) : '?');
                WriteString(buf, 0, 2 * 8, line_buffer2);

                sprintf(line_buffer3, "TRK %03u SEC %02u", trk, sec);
                WriteString(buf, 0, 3 * 8, line_buffer3);
            }

            sprintf(line_buffer4, "RX %u TX %u", rx_count, tx_count);
            WriteString(buf, 0, 4 * 8, line_buffer4);

            sprintf(line_buffer5, "TO %lu MAX %luus",
                    (unsigned long)diag_mem_timeouts,
                    (unsigned long)diag_max_service_us);
            WriteString(buf, 0, 5 * 8, line_buffer5);

            sprintf(line_buffer6, "MRD %02x MWR %02x", mem_r_op, mem_w_op);
            WriteString(buf, 0, 6 * 8, line_buffer6);

            sprintf(line_buffer7, "IOR %02x IOW %02x", io_r_op, io_w_op);
            WriteString(buf, 0, 7 * 8, line_buffer7);

            if ((fast_frame & 0x1F) == 0)
                render(buf, &frame_area);
            else
                render(buf, &status_area);
        }

        if ((cur_disp_mode == OFF) & (tbmon == false)) {

            // Slow-changing lines (full frame every 32 iterations)
            if ((fast_frame & 0x1F) == 0) {  // every 32 iterations
                sprintf(line_buffer0, "Z80 %3lu kHz", (unsigned long)(CPU_SPEED / 1000.0f));
                WriteString(buf, 0, 0 * 8, line_buffer0);

                sprintf(line_buffer1, "BNK %d REF %lx",
                        cur_bank, (unsigned long)(pio_cycles_refresh & 0xFFFF));
                WriteString(buf, 0, 1 * 8, line_buffer1);

                sprintf(line_buffer2, "TO %lu MAX %luus",
                        (unsigned long)diag_mem_timeouts,
                        (unsigned long)diag_max_service_us);
                WriteString(buf, 0, 2 * 8, line_buffer2);

                sprintf(line_buffer3, "R%04lx W%04lx",
                        dr_op & 0xFFFF, dw_op & 0xFFFF);
                WriteString(buf, 0, 3 * 8, line_buffer3);
            }

            // Fast-updating monitor lines (every iteration)
            sprintf(line_buffer4, "ADDR %04x", m_adr);
            WriteString(buf, 0, 4 * 8, line_buffer4);

            sprintf(line_buffer5, "MRD %02x MWR %02x", mem_r_op, mem_w_op);
            WriteString(buf, 0, 5 * 8, line_buffer5);

            sprintf(line_buffer6, "IOR %02x IOW %02x", io_r_op, io_w_op);
            WriteString(buf, 0, 6 * 8, line_buffer6);

            sprintf(line_buffer7, "RX %u TX %u", rx_count, tx_count);
            WriteString(buf, 0, 7 * 8, line_buffer7);

            // Render only status pages (4-7) for speed; full frame every 32 iterations
            if ((fast_frame & 0x1F) == 0)
                render(buf, &frame_area);
            else
                render(buf, &status_area);
        }

        // Shared by both screens so their full/partial render cadence is the same.
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

        // Only handle buttons once the main loop has finished booting and
        // released the Z80.  Before that, the ADC can read a spurious value
        // (floating input during init) that would release the Z80 reset early
        // and deadlock the main loop's boot sequence.
        if (system_up && buttons_enabled && buttons != NONE) {
            reset_hold();
            confirmed = false;
            disabled = true;

            while (!confirmed) {
            };

            switch (buttons) {
            case UP:
                if (!tbmon_loaded) {
                    load();
                    sleep_ms(DISPLAY_DELAY);
                    sleep_ms(DISPLAY_DELAY);
                }
                if (tbmon) {
                    if (tbmon_loaded) {
                        tbmon_idx = (tbmon_idx >= BYTES_PER_ROW)
                                    ? tbmon_idx - BYTES_PER_ROW : 0;
                    }
                    tbmon_loaded = true;
                    display_ram_viewer();
                } else {
                    if (cur_disp_mode == ON)
                        show_info();
                    else
                        clear_screen();
                }
                break;

            case DOWN:
                if (tbmon) {
                    if (tbmon_loaded) {
                        tbmon_idx += BYTES_PER_ROW;
                        if (tbmon_idx > RAM_SIZE - (BYTES_PER_ROW * LINES))
                            tbmon_idx = 0;
                    }
                    tbmon_loaded = true;
                    display_ram_viewer();
                } else {
                    if (cur_disp_mode == ON)
                        show_info();
                    else
                        clear_screen();
                }
                break;

            case BACK:
                cur_bank = (cur_bank + 1) % (MAX_BANKS);
                clear_screen();
                sprintf(text_buffer, "BANK #%02d", cur_bank);
                WriteString(buf, 0, 0, text_buffer);
                render(buf, &frame_area);
                sleep_ms(DISPLAY_DELAY);
                sleep_ms(DISPLAY_DELAY);
                if (cur_disp_mode == ON)
                    show_info();
                else
                    clear_screen();
                break;

            case OK:
                clear_screen();
                if (cur_disp_mode == OFF) {
                    tbmon = true;
                    wait_for_button_release();
                    print_string(0, 2, "*    TB-MON    *");
                    print_string(0, 3, "*     v 0.1    *");
                    print_string(0, 4, "*    TurBoos   *");
                    print_string(0, 5, "*     2025     *");
                } else {
                    sprintf(text_buffer, "CLEAR BANK #%02d?", cur_bank);
                    WriteString(buf, 0, 0, text_buffer);
                    render(buf, &frame_area);
                    wait_for_button_release();

                    if (wait_for_yes_no_button()) {
                        clear_bank(cur_bank);
                        print_string(0, 3, "CLEARED!");
                    } else
                        print_string(0, 3, "CANCELED!");

                    sleep_ms(DISPLAY_DELAY);
                    sleep_ms(DISPLAY_DELAY);
                    if (cur_disp_mode == ON)
                        show_info();
                    else
                        clear_screen();
                }
                break;

            case CANCEL:
                if (tbmon) {
                    tbmon = false;
                    tbmon_loaded = false;
                    tbmon_idx = 0;
                }
                if (cur_disp_mode == OFF)
                    cur_disp_mode = ON;
                else
                    cur_disp_mode = OFF;

                sleep_ms(DISPLAY_DELAY);
                sleep_ms(DISPLAY_DELAY);

                if (cur_disp_mode == ON)
                    show_info();
                else
                    clear_screen();
                break;
            }

            disabled = false;
            reset_release();
        }
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

    FRESULT fr = f_findfirst(&dj, &fno, p_dir, FILE_EXT);
    if (FR_OK != fr) {
        show_error(0, 0, "Count Files ERR");
        return 0;
    }

    while (fr == FR_OK && fno.fname[0]) {
        if (fno.fattrib & AM_DIR) {
            // directory
        } else {
            count++;
        }
        fr = f_findnext(&dj, &fno);
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

    FRESULT fr = f_findfirst(&dj, &fno, p_dir, FILE_EXT);
    if (FR_OK != fr) {
        show_error(0, 0, "File Sel ERR");
        return 0;
    }

    while (fr == FR_OK && fno.fname[0]) {
        if (fno.fattrib & AM_DIR) {
            // directory
        } else {
            count++;
            if (count == no) {
                strcpy(file, fno.fname);
                return count;
            }
        }
        fr = f_findnext(&dj, &fno);
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

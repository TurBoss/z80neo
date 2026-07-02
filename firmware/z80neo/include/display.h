#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "ssd1306_i2c.h"

// ---------------------------------------------------------------------------
// Display-related constants
// ---------------------------------------------------------------------------

#define DISPLAY_DELAY       200
#define DISPLAY_DELAY_LONG  500
#define DISPLAY_DELAY_SHORT 100

#define BLINK_DELAY         (100 * 1000)
#define LONG_BUTTON_DELAY   (400 * 1000)

#define TEXT_BUFFER_SIZE 512
#define BYTES_PER_ROW    8
#define BYTES_PER_LINE   16
#define LINES            8

// ---------------------------------------------------------------------------
// Display line type (17 chars for filename display)
// ---------------------------------------------------------------------------
typedef char display_line[17];

// ---------------------------------------------------------------------------
// Button state enum
// ---------------------------------------------------------------------------
typedef enum {
    NONE,
    UP,
    DOWN,
    BACK,
    OK,
    CANCEL,
    CANCEL2
} button_state;

// ---------------------------------------------------------------------------
// Display mode enum
// ---------------------------------------------------------------------------
typedef enum {
    OFF,
    ON
} disp_mode;

// ---------------------------------------------------------------------------
// External globals — declared here so memory.c can reference them
// ---------------------------------------------------------------------------

extern struct render_area frame_area;

// Display buffer (also used as SD work buffer for DMA alignment)
#define BUF_SIZE 8192
extern uint32_t buf32[(BUF_SIZE + 3) / 4];
extern uint8_t *buf;

// Screen text lines
extern char line1[TEXT_BUFFER_SIZE];
extern char line2[TEXT_BUFFER_SIZE];
extern char line3[TEXT_BUFFER_SIZE];
extern char line4[TEXT_BUFFER_SIZE];
extern char line5[TEXT_BUFFER_SIZE];
extern char line6[TEXT_BUFFER_SIZE];
extern char line7[TEXT_BUFFER_SIZE];
extern char line8[TEXT_BUFFER_SIZE];
extern char *screen[LINES];

extern char text_buffer[TEXT_BUFFER_SIZE];
extern char line_buffer0[24];
extern char line_buffer1[24];
extern char line_buffer2[24];
extern char line_buffer3[24];
extern char line_buffer4[24];
extern char line_buffer5[24];
extern char line_buffer6[24];
extern char line_buffer7[24];

extern char tbmon_text_buffer[8][17];

extern const char *hexStringChar[];

extern char *load_chars;
extern uint8_t load_char_index;

extern display_line file;

// TBMON state
extern volatile bool tbmon;
extern volatile bool tbmon_loaded;
extern uint16_t tbmon_idx;

extern volatile bool confirmed;

// ---- Also used from memory.c, declared here for visibility ----
extern char serial_text_buffer[TEXT_BUFFER_SIZE];

// ---------------------------------------------------------------------------
// Function prototypes
// ---------------------------------------------------------------------------

// Text / string helpers
int  center_string(char *string);

// Screen clearing
void clear_screen(void);
void clear_screen0(void);
void clear_line0(int line);
void clear_line(int line);

// String output
void print_string0(int x, int y, char *text, ...);
void print_string(int x, int y, char *text, ...);
void print_line(int x, char *text, ...);
void print_line0(int x, char *text, ...);

// Character output
void print_char0(int x, int y, char c);
void print_char(int x, int y, char c);

// Pixel plotting
void disp_plot0(int x, int y);
void plot_pixel(int x, int y);
void unplot_pixel(int x, int y);

// Line drawing
void disp_line0(int x1, int y1, int x2, int y2);
void disp_line(int x1, int y1, int x2, int y2);
void unplot_line(int x1, int y1, int x2, int y2);

// Render
void render_display(void);

// RAM viewer
void display_ram_viewer(void);

// Button I/O
button_state read_button_state(void);
bool wait_for_button_release(void);
void wait_for_button(void);
bool wait_for_yes_no_button(void);

// Main display / UI loop (runs on core 1)
void display_loop(void);

// Boot / info screens
void show_logo(void);
void boot_screen(void);
void show_info(void);

// Error screens
void show_error_and_halt(char *err);
void show_error(int b, int a, char *err);
void show_error_wait_for_button(char *err);

// File selector UI
void clear_file_buffer(void);
int  count_files(void);
int  select_file_no(int no);
int  select_file(void);
int  create_name(void);

#endif // DISPLAY_H

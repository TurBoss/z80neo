// -----------------------------------------------------------------------------
// VoidVDP - A versatile serial terminal (fork of VersaTerm)
// Copyright (C) 2022 David Hansel
// Copyright (C) 2026 turboss
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
// -----------------------------------------------------------------------------

#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "hardware/uart.h"

#include "pins.h"
#include "font.h"
#include "config.h"
#include "sound.h"
#include "keyboard.h"
#include "framebuf.h"
#include "framebuf_dvi.h"
#include "framebuf_vga.h"

// defined in main.c
void wait(uint32_t milliseconds);

static __attribute__((aligned(4))) uint8_t framebuf_data[80*60*4];  // shared data buffer
static __attribute__((aligned(4))) uint8_t framebuf_rowattr[60];    // row attributes
int16_t framebuf_flash_counter = 0;
uint8_t framebuf_flash_color = 0;


static bool screen_inverted = false, double_size_chars = false;
static uint8_t num_rows = 0, num_cols = 0, xborder = 0, yborder = 0;
static bool is_dvi = true;
static uint8_t color_map_inv[256];
static uint16_t scroll_delay = 0;

#define MKIDX(x, y) (((x)+xborder) + (((y)+yborder) * MAX_COLS))


static uint8_t mapcolor(uint8_t color16)
{
  return config_get_screen_color(color16, is_dvi);
}


static uint8_t mapcolor_inv(uint8_t fullcolor)
{
  return color_map_inv[fullcolor];
}



static void charmemset(uint32_t idx, uint8_t c, uint8_t a, uint8_t fg, uint8_t bg, size_t n)
{
  if( config_get_screen_monochrome() )
    {
      fg = (a & ATTR_BOLD) ? config_get_screen_monochrome_textcolor_bold(is_dvi) : config_get_screen_monochrome_textcolor_normal(is_dvi);
      bg = config_get_screen_monochrome_backgroundcolor(is_dvi);
    }
  else
    {
      fg = mapcolor(fg);
      bg = mapcolor(bg);
    }
  
  if( screen_inverted )
    { uint8_t c = fg; fg = bg; bg = c; }

  if( is_dvi )
    return framebuf_dvi_charmemset(idx, c, a, fg, bg, n);
  else
    return framebuf_vga_charmemset(idx, c, a, fg, bg, n);
}


static void charmemmove(uint32_t toidx, uint32_t fromidx, size_t n)
{
  if( is_dvi )
    return framebuf_dvi_charmemmove(toidx, fromidx, n);
  else
    return framebuf_vga_charmemmove(toidx, fromidx, n);
}


static void set_char_and_attr(uint32_t idx, uint32_t c)
{
  if( is_dvi )
    framebuf_dvi_set_char_and_attr(idx, c);
  else
    framebuf_vga_set_char_and_attr(idx, c);
}


static uint32_t get_char_and_attr(uint32_t idx)
{
  if( is_dvi )
    return framebuf_dvi_get_char_and_attr(idx);
  else
    return framebuf_vga_get_char_and_attr(idx);
}


static void set_char(uint32_t idx, uint8_t c)
{
  if( is_dvi )
    framebuf_dvi_set_char(idx, c);
  else
    framebuf_vga_set_char(idx, c);
}


static uint8_t get_char(uint32_t idx)
{
  if( is_dvi )
    return framebuf_dvi_get_char(idx);
  else
    return framebuf_vga_get_char(idx);
}


static uint8_t get_attr(uint32_t idx)
{
  if( is_dvi )
    return framebuf_dvi_get_attr(idx);
  else
    return framebuf_vga_get_attr(idx);
}


static void set_fullcolor(uint32_t idx, uint8_t fg, uint8_t bg)
{
  bool char_inverted = (get_attr(idx) & ATTR_INVERSE)!=0;
  if( char_inverted != screen_inverted )
    { uint8_t c = fg; fg = bg; bg = c; }

  if( is_dvi )
    return framebuf_dvi_set_color(idx, fg, bg);
  else
    return framebuf_vga_set_color(idx, fg, bg);
}


static void get_fullcolor(uint32_t idx, uint8_t *fg, uint8_t *bg)
{
  bool char_inverted = (get_attr(idx) & ATTR_INVERSE)!=0;
  if( char_inverted != screen_inverted )
    { uint8_t *c = fg; fg = bg; bg = c; }
  
  if( is_dvi )
    framebuf_dvi_get_color(idx, fg, bg);
  else
    framebuf_vga_get_color(idx, fg, bg);
}


static void set_attr(uint32_t idx, uint8_t attr)
{
  uint8_t prev_attr = get_attr(idx);
  
  if( !font_have_boldfont() && (config_get_terminal_type()!=CFG_TTYPE_PETSCII) && (attr & ATTR_BOLD) != (prev_attr & ATTR_BOLD) )
    {
      // "bold" setting has changed and we are emulating bold via bright color setting
      if( config_get_screen_monochrome() )
        {
          if( attr & ATTR_BOLD )
            set_fullcolor(idx, config_get_screen_monochrome_textcolor_bold(is_dvi), config_get_screen_monochrome_backgroundcolor(is_dvi));
          else
            set_fullcolor(idx, config_get_screen_monochrome_textcolor_normal(is_dvi), config_get_screen_monochrome_backgroundcolor(is_dvi));
        }
      else
        {
          uint8_t fg, bg;
          get_fullcolor(idx, &fg, &bg);
          if( attr & ATTR_BOLD )
            set_fullcolor(idx, mapcolor(mapcolor_inv(fg) | 8), bg);
          else
            set_fullcolor(idx, mapcolor(mapcolor_inv(fg) & 7), bg);
        }
    }
  
  if( (attr & ATTR_INVERSE) != (prev_attr & ATTR_INVERSE) )
    {
      // "inverse" setting has changed => swap fg/bg colors
      uint8_t fg, bg;
      get_fullcolor(idx, &fg, &bg);
      set_fullcolor(idx,  bg,  fg);
    }
      
  if( is_dvi )
    framebuf_dvi_set_attr(idx, attr);
  else
    framebuf_vga_set_attr(idx, attr);
}


static void set_color(uint32_t idx, uint8_t fg, uint8_t bg)
{
  if( config_get_screen_monochrome() )
    {
      if( get_attr(idx) & ATTR_BOLD )
        set_fullcolor(idx, config_get_screen_monochrome_textcolor_bold(is_dvi), config_get_screen_monochrome_backgroundcolor(is_dvi));
      else
        set_fullcolor(idx, config_get_screen_monochrome_textcolor_normal(is_dvi), config_get_screen_monochrome_backgroundcolor(is_dvi));
    }
  else
    {
      if( font_have_boldfont() || config_get_terminal_type()==CFG_TTYPE_PETSCII )
        set_fullcolor(idx, mapcolor(fg), mapcolor(bg));
      else
        {
          uint8_t bold = (get_attr(idx) & ATTR_BOLD) ? 8 : 0;
          set_fullcolor(idx, mapcolor((fg & 7) | bold), mapcolor(bg & 7));
        }
    }
}


uint8_t framebuf_get_nrows()
{
  return double_size_chars ? num_rows/2 : num_rows;
}


uint8_t framebuf_get_ncols(int row)
{
  if( row>=0 && row<framebuf_get_nrows() && !double_size_chars && (framebuf_rowattr[row+yborder] & ROW_ATTR_DBL_WIDTH)!=0 )
    return num_cols / 2;
  else
    return num_cols;
}


void framebuf_set_char(uint8_t x, uint8_t y, uint8_t c)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      set_char(MKIDX(x, y), c);
      if( double_size_chars ) set_char(MKIDX(x, y+1), c);
    }
}


uint8_t framebuf_get_char(uint8_t x, uint8_t y)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    return get_char(MKIDX(x, y));
  else
    return 0;
}


void framebuf_set_attr(uint8_t x, uint8_t y, uint8_t attr)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      set_attr(MKIDX(x, y), attr);
      if( double_size_chars ) set_attr(MKIDX(x, y+1), attr);
    }
}


uint8_t framebuf_get_attr(uint8_t x, uint8_t y)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    return get_attr(MKIDX(x, y));
  else
    return 0;
}


void framebuf_set_row_attr(uint8_t row, uint8_t attr)
{
  if( !double_size_chars && row<framebuf_get_nrows() && framebuf_rowattr[row+yborder]!=attr )
    framebuf_rowattr[row+yborder] = attr;
}


uint8_t framebuf_get_row_attr(uint8_t y)
{
  return y<num_rows ? framebuf_rowattr[y+yborder] : 0;
}


void framebuf_set_color(uint8_t x, uint8_t y, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      set_color(MKIDX(x, y), fg, bg);
      if( double_size_chars ) set_color(MKIDX(x, y+1), fg, bg);
    }
}


void framebuf_set_fullcolor(uint8_t x, uint8_t y, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      set_fullcolor(MKIDX(x, y), fg, bg);
      if( double_size_chars ) set_fullcolor(MKIDX(x, y+1), fg, bg);
    }
}


void framebuf_fill_screen(char character, uint8_t fg, uint8_t bg)
{
  framebuf_fill_region(0, 0, framebuf_get_ncols(-1)-1, framebuf_get_nrows()-1, character, fg, bg);
}


void framebuf_fill_region(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye, char c, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) { ys=ys*2; ye=ye*2+1; }
  if( ys < num_rows && xs < framebuf_get_ncols(ys) && ye < num_rows && xe < framebuf_get_ncols(ye) )
    {
      if( xs>0 )
        {
          charmemset(MKIDX(xs, ys), c, config_get_terminal_default_attr(), fg, bg, num_cols-xs);
          ys++;
        }

      if( xe<framebuf_get_ncols(ye)-1 )
        {
          charmemset(MKIDX(0, ye), c, config_get_terminal_default_attr(), fg, bg, xe+1);
          if( ye>0 )
            ye--;
          else
            return;
        }
      
      while( ys<=ye )
        {
          charmemset(MKIDX(0, ys), c, config_get_terminal_default_attr(), fg, bg, num_cols);
          ys++;
        }
    }
}


void framebuf_scroll_screen(int8_t n, uint8_t fg, uint8_t bg)
{
  framebuf_scroll_region(0, framebuf_get_nrows()-1, n, fg, bg);
}


void framebuf_scroll_region(uint8_t start, uint8_t end, int8_t n, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) {start *= 2; end=end*2+1; n *= 2; }
  if( n!=0 && start<num_rows && end<num_rows )
    {
      if( config_get_keyboard_scroll_lock() && (keyboard_get_led_status() & KEYBOARD_LED_SCROLLLOCK)!=0 )
        {
          size_t n = keyboard_num_keypress();
          while( (keyboard_get_led_status() & KEYBOARD_LED_SCROLLLOCK)!=0  )
            { 
              wait(10); 
              if( keyboard_num_keypress()>n ) { sound_play_tone(880, 50, config_get_audible_bell_volume(), false); n=keyboard_num_keypress(); }
            }
        }

      if( scroll_delay>0 ) wait(scroll_delay);

      if( n>0 )
        {
          // scrolling up
          if( n <= end-start )
            {
              if( !double_size_chars ) memmove(framebuf_rowattr+yborder+start, framebuf_rowattr+start+yborder+n, end-start+1-n);
              for(int y=start; y<=end-n; y++)
                charmemmove(MKIDX(0, y), MKIDX(0, y+n), MAX_COLS-xborder*2);
            }
          
          if( n>end-start+1 ) n = end-start+1;
          if( !double_size_chars ) memset(framebuf_rowattr+(end+yborder+1-n), 0, n);
          for(int y=0; y<n; y++)
            charmemset(MKIDX(0, end+y+1-n), ' ', config_get_terminal_default_attr(), fg, bg, num_cols);
        }
      else if( n<0 )
        {
          // scrolling down
          n = -n;
          if( n <= end-start )
            {
              if( !double_size_chars ) memmove(framebuf_rowattr+start+yborder+n, framebuf_rowattr+start+yborder, end-start+1-n);
              for(int y=end-n; y>=start; y--)
                charmemmove(MKIDX(0, y+n), MKIDX(0, y), MAX_COLS-xborder*2);
            }
          
          if( n>end-start+1 ) n = end-start+1;
          if( !double_size_chars ) memset(framebuf_rowattr+start+yborder, 0, n);
          for(int i=0; i<n; i++)
            charmemset(MKIDX(0, start+i), ' ', config_get_terminal_default_attr(), fg, bg, num_cols);
        }
    }
}


void framebuf_insert(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      for(int i=0; i<((int) num_cols)-(x+n); i++)
        {
          int col = num_cols-i-1;
          set_char_and_attr(MKIDX(col, y), get_char_and_attr(MKIDX(col-n, y)));
          if( double_size_chars ) set_char_and_attr(MKIDX(col, y+1), get_char_and_attr(MKIDX(col-n, y+1)));
        }
      
      for(int i=0; i<n && x+i<num_cols; i++)
        {
          uint32_t idx = MKIDX(x+i, y);
          set_char(idx, ' ');
          set_attr(idx, 0);
          set_color(idx, fg, bg);
          if( double_size_chars )
            {
              idx += MAX_COLS;
              set_char(idx, ' ');
              set_attr(idx, 0);
              set_color(idx, fg, bg);
            }
        }
    }
}


void framebuf_delete(uint8_t x, uint8_t y, uint8_t n, uint8_t fg, uint8_t bg)
{
  if( double_size_chars ) y *= 2;
  if( y < num_rows && x < framebuf_get_ncols(y) )
    {
      for(int i=0; i<((int) num_cols)-(x+n); i++)
        {
          set_char_and_attr(MKIDX(x+i, y), get_char_and_attr(MKIDX(x+n+i, y)));
          if( double_size_chars ) set_char_and_attr(MKIDX(x+i, y+1), get_char_and_attr(MKIDX(x+n+i, y+1)));
        }
      
      for(int i=0; i<n && i<num_cols-x; i++)
        {
          uint32_t idx = MKIDX(num_cols-1-i, y);
          set_char(idx, ' ');
          set_attr(idx, 0);
          set_color(idx, fg, bg);
          if( double_size_chars )
            {
              idx += MAX_COLS;
              set_char(idx, ' ');
              set_attr(idx, 0);
              set_color(idx, fg, bg);
            }
        }
    }
}


void framebuf_set_screen_size(uint8_t ncols, uint8_t nrows)
{
  if( nrows>MAX_ROWS ) nrows = MAX_ROWS;
  if( ncols>MAX_COLS ) ncols = MAX_COLS;

  if( num_rows!=nrows || num_cols!=ncols )
    {
      screen_inverted = false;
      charmemset(0, ' ', config_get_terminal_default_attr(), config_get_terminal_default_fg(), config_get_terminal_default_bg(), MAX_ROWS * MAX_COLS);
      memset(framebuf_rowattr, 0, MAX_ROWS);

      double_size_chars = (ncols*8*2)<=FRAME_WIDTH && (nrows*font_get_char_height()*2)<=FRAME_HEIGHT && config_get_screen_dblchars();
      if( double_size_chars )
        {
          num_rows = nrows*2;
          num_cols = ncols;
          xborder = (MAX_COLS-ncols*2)/4;
          yborder = (MAX_ROWS-nrows*2)/2;
          for(int i=0; i<num_rows; i++)
            framebuf_rowattr[i+yborder] = ROW_ATTR_DBL_WIDTH | ((i&1) ? ROW_ATTR_DBL_HEIGHT_BOT : ROW_ATTR_DBL_HEIGHT_TOP);
        }
      else
        {
          num_rows = nrows;
          num_cols = ncols;
          xborder = (MAX_COLS-ncols)/2;
          yborder = (MAX_ROWS-nrows)/2;
        }
    }
}


void framebuf_set_screen_inverted(bool invert)
{
  if( invert != screen_inverted )
    {
      if( is_dvi )
	framebuf_dvi_invert();
      else
	framebuf_vga_invert();
      
      screen_inverted = invert;
    }
}


void framebuf_set_scroll_delay(uint16_t ms)
{
  scroll_delay = ms;
}


void framebuf_flash_screen(uint8_t color, uint8_t nframes)
{
  if( framebuf_flash_counter>0 ) 
    framebuf_flash_counter += nframes;
  else
    {
      framebuf_flash_counter = -nframes;
      framebuf_flash_color   = color;
    }
}


bool framebuf_is_dvi()
{
  return is_dvi;
}


void framebuf_apply_settings()
{
  font_apply_settings();
  memset(framebuf_data, 0, sizeof(framebuf_data));
  framebuf_set_screen_size(config_get_screen_cols(), config_get_screen_rows());
  for(int i=0; i<256; i++) color_map_inv[i] = 0;
  for(int i=0; i<16; i++)  color_map_inv[mapcolor(i)] = i;
  scroll_delay = 0;
}


void framebuf_init(bool forceDVI)
{
  gpio_init(PIN_HDMI_DETECT);
  gpio_set_dir(PIN_HDMI_DETECT, false); // input

  if( forceDVI )
    is_dvi = true;
  else if( config_get_screen_display()==0 )
    {
      wait(100); // wait a short time for voltage to stabilize before reading
      is_dvi = gpio_get(PIN_HDMI_DETECT);
    }
  else
    is_dvi = config_get_screen_display()==1;
  
  font_init();
  memset(framebuf_data, 0, sizeof(framebuf_data));
  screen_inverted = false;

  if( is_dvi )
    framebuf_dvi_init(framebuf_data, framebuf_rowattr);
  else
    framebuf_vga_init(framebuf_data, framebuf_rowattr);

  framebuf_apply_settings();

  // must re-initialize serial baud rate after changing system clock
  uart_set_baudrate(PIN_UART_ID, config_get_serial_baud());
}


// ===========================================================================
// Monochrome bitmap graphics layer
// ===========================================================================

bool    framebuf_gfx_active = false;
uint8_t framebuf_gfx_buf[GFX_STRIDE * GFX_HEIGHT];
static uint8_t gfx_fg = 15, gfx_bg = 0;


bool framebuf_gfx_enabled()
{
  return framebuf_gfx_active;
}


void framebuf_gfx_get_colors(uint8_t *fg, uint8_t *bg)
{
  if( fg ) *fg = gfx_fg;
  if( bg ) *bg = gfx_bg;
}


// Set the fg/bg colours of the character cell that contains pixel (x,y).  A
// cell can show two colours (fg = set pixels, bg = clear pixels).
static void gfx_apply_cell(int x, int y)
{
  if( x<0 || x>=GFX_WIDTH || y<0 || y>=GFX_HEIGHT ) return;

  int cx = x>>3;
  int cy = y / font_get_char_height();
  if( cx < (int) MAX_COLS && cy < (int) MAX_ROWS )
    set_color(cy*MAX_COLS+cx, gfx_fg, gfx_bg);
}


bool framebuf_gfx_enable(bool on)
{
  if( on == framebuf_gfx_active ) return framebuf_gfx_active;

  uint32_t rows = MAX_ROWS, cols = MAX_COLS;

  if( on )
    {
      // In graphics mode each cell's character code is its column index, so
      // the DVI encoder's font lookup reads the bitmap byte for that column
      // from framebuf_gfx_buf instead of a glyph.
      memset(framebuf_gfx_buf, 0, sizeof(framebuf_gfx_buf));
      for(uint32_t r=0; r<rows; r++)
        for(uint32_t c=0; c<cols; c++)
          {
            set_char(r*cols+c, (uint8_t) c);
            set_attr(r*cols+c, 0);
            set_color(r*cols+c, gfx_fg, gfx_bg);
          }
    }
  else
    {
      // back to text: blank the character codes again
      for(uint32_t r=0; r<rows; r++)
        for(uint32_t c=0; c<cols; c++)
          set_char(r*cols+c, ' ');
    }

  framebuf_gfx_active = on;
  return on;
}


void framebuf_gfx_set_colors(uint8_t fg, uint8_t bg)
{
  gfx_fg = fg & 0x0F;
  gfx_bg = bg & 0x0F;
}


void framebuf_gfx_clear()
{
  uint32_t rows = MAX_ROWS, cols = MAX_COLS;
  memset(framebuf_gfx_buf, 0, sizeof(framebuf_gfx_buf));
  for(uint32_t r=0; r<rows; r++)
    for(uint32_t c=0; c<cols; c++)
      set_color(r*cols+c, gfx_fg, gfx_bg);
}


void framebuf_gfx_pixel(int x, int y, bool on)
{
  if( x<0 || x>=GFX_WIDTH || y<0 || y>=GFX_HEIGHT ) return;

  uint8_t *p = &framebuf_gfx_buf[y*GFX_STRIDE + (x>>3)];
  uint8_t  m = (uint8_t) (1 << (x & 7));
  if( on ) *p |= m; else *p &= ~m;

  gfx_apply_cell(x, y);
}


bool framebuf_gfx_get_pixel(int x, int y)
{
  if( x<0 || x>=GFX_WIDTH || y<0 || y>=GFX_HEIGHT ) return false;
  return (framebuf_gfx_buf[y*GFX_STRIDE + (x>>3)] & (1 << (x & 7))) != 0;
}


void framebuf_gfx_line(int x0, int y0, int x1, int y1, bool on)
{
  int dx = x1>x0 ? x1-x0 : x0-x1;
  int dy = y1>y0 ? y1-y0 : y0-y1;
  int sx = x0<x1 ? 1 : -1;
  int sy = y0<y1 ? 1 : -1;
  int err = dx-dy;

  while( true )
    {
      framebuf_gfx_pixel(x0, y0, on);
      if( x0==x1 && y0==y1 ) break;
      int e2 = 2*err;
      if( e2 > -dy ) { err -= dy; x0 += sx; }
      if( e2 <  dx ) { err += dx; y0 += sy; }
    }
}


void framebuf_gfx_fill(int x, int y, int w, int h, bool on)
{
  for(int j=0; j<h; j++)
    for(int i=0; i<w; i++)
      framebuf_gfx_pixel(x+i, y+j, on);
}


// Blit one glyph of the currently selected font into the bitmap.  The font
// bitmap is MSB-first; gfx buffer bits are LSB-first, so bits are reversed.
void framebuf_gfx_draw_char(int col, int row, uint8_t ch)
{
  uint8_t        fontId = config_get_screen_font_normal();
  const uint8_t *bmp    = font_get_bmpdata(fontId);
  uint32_t       bw=0, bh=0;
  uint8_t        chh=0;

  if( bmp==NULL || !font_get_font_info(fontId, &bw, &bh, &chh, NULL) || chh==0 || bw<8 )
    return;

  uint32_t gcols = bw/8;          // glyph columns in the font bitmap
  uint32_t grows = bh/chh;        // glyph rows
  if( (uint32_t) ch >= gcols*grows )
    return;

  uint32_t gc = ch % gcols;
  uint32_t gr = ch / gcols;
  int      x0 = col*8;
  int      y0 = row*chh;

  for(uint32_t r=0; r<chh; r++)
    {
      uint8_t bits = bmp[(gr*chh + r)*gcols + gc];
      for(int x=0; x<8; x++)
        framebuf_gfx_pixel(x0+x, y0+(int) r, (bits >> (7-x)) & 1);
    }
}

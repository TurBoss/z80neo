#ifndef FILEIO_H
#define FILEIO_H

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Z80 SD-card file API (ports 0xA0-0xA7)
//
// Lets a bare-metal Z80 program (e.g. the +z80neo MIDI player) read files from
// the SD card the firmware already has mounted for CP/M and the boot loader.
// The firmware owns FatFS; the Z80 only sees a byte stream.  One file is open
// at a time and reads are buffered (512 bytes) so a byte costs one port read.
//
// Register map:
//
//   0xA0  W  command (FILE_OPEN .. FILE_SEEK)
//   0xA0  R  status  (FILE_ST_*)
//   0xA1  W  append one byte to the filename buffer
//   0xA1  R  successive bytes of the current filename (0 = end), for DIRNEXT
//   0xA2  R  next data byte (auto-advances)
//   0xA3  W  clear the filename buffer
//   0xA4  W  argument byte 0 (seek offset, little-endian)
//   0xA5  W  argument byte 1
//   0xA6  W  argument byte 2
//   0xA7  W  argument byte 3
//   0xA4  R  result byte 0 (file size, little-endian)
//   0xA5  R  result byte 1
//   0xA6  R  result byte 2
//   0xA7  R  result byte 3
//
// Typical open/read:
//     out (0x92),10 / out (0x93),3      ; (UART1, not here)
//     out (0xA3),0                      ; name reset (optional)
//     ld hl,name ; loop: out (0xA1),a   ; append name bytes
//     ld a,1 ; out (0xA0),a             ; FILE_OPEN
//   read loop:
//     in a,(0xA0) ; bit0 set -> byte ready, bit1 set -> EOF
//     in a,(0xA2)                       ; byte
//
// The firmware's WAIT line holds the Z80 for the whole port service, so a read
// that has to refill from the card stalls the Z80 but never loses a byte.
// ---------------------------------------------------------------------------

#define FILE_CMD   0xA0   // W: command; R: status
#define FILE_NAME  0xA1   // W: append filename byte
#define FILE_DATA  0xA2   // R: next data byte
#define FILE_RST   0xA3   // W: clear filename buffer
#define FILE_ARG0  0xA4   // W/R: 32-bit argument/result (0xA4-0xA7)

// Commands written to FILE_CMD.
#define FILE_OPEN      1  // open the name in the buffer (FA_READ)
#define FILE_CLOSE     2
#define FILE_REWIND    3  // seek to offset 0
#define FILE_SIZE      4  // result = file size (32-bit LE)
#define FILE_DIRRESET  5  // scan the directory in the name buffer (empty = root)
#define FILE_DIRNEXT   6  // load the next entry (file or dir) into the buffer
#define FILE_SEEK      7  // seek to the 32-bit LE offset in FILE_ARG0-3
#define FILE_OPEN_MIDI 8  // open/parse an SMF and stream records (see midi.h)
#define FILE_REMOUNT_CPM 9  // hand the SD volume back to the CP/M disk layer
#define FILE_REMOUNT_FUZIX 10 // hand the SD volume back to the Fuzix block device

// Status bits returned by reading FILE_CMD.
#define FILE_ST_READY   0x01  // a data byte is available
#define FILE_ST_EOF     0x02  // no more data
#define FILE_ST_ERR     0x04  // last command failed
#define FILE_ST_OPEN    0x08  // a file is open
#define FILE_ST_NAME    0x10  // FILE_DIRNEXT produced a name
#define FILE_ST_NAMEFULL 0x20 // filename buffer is full
#define FILE_ST_DIR     0x40  // the FILE_DIRNEXT name is a directory

// Port handlers (called from memory.c io_read_port / io_write_port).
uint8_t fileio_status_read(void);
void    fileio_cmd_write(uint8_t cmd);
void    fileio_name_write(uint8_t c);
uint8_t fileio_name_read(void);   // successive bytes of the name buffer, 0 = end
void    fileio_name_reset(void);
uint8_t fileio_data_read(void);
void    fileio_arg_write(uint8_t index, uint8_t data);
uint8_t fileio_arg_read(uint8_t index);

// Reset the whole API (called at boot).
void fileio_reset(void);

// Ensure the FatFS volume is mounted (used by the MIDI streamer too).
bool fileio_ensure_mount(void);

#endif

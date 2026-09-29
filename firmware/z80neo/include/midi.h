#ifndef MIDI_H
#define MIDI_H

#include <stdbool.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Firmware-side Standard MIDI File parser/streamer (ports 0xA0-0xA7).
//
// The Z80 cannot keep up with parsing SMF in C, so the Pico parses the file
// and hands the Z80 a pre-merged stream of compact records.  One record is a
// three-byte header followed by raw MIDI bytes on the file data port (0xA2):
//
//     [0] delay low byte   } milliseconds to wait before this record
//     [1] delay high byte  }
//     [2] byte count      0 = delay only, 0xFF = end, else raw bytes follow
//     [3..]                byte count raw MIDI message bytes, forwarded as-is
//
// Every channel message sharing a tick is coalesced into one record, so the
// Z80 waits once per tick and then blits the tick's bytes from a tight copy
// loop.  A tick that does not fit in one record is split; the extra records
// carry delay 0.  All MTrk chunks are merged by absolute tick, so type 0 and
// type 1 files play together.  Meta/SysEx events are consumed for their timing
// only.  Files are streamed from FatFS with a per-track buffer, so there is no
// size limit.
// ---------------------------------------------------------------------------

bool    midi_open(const char *name);
void    midi_close(void);
bool    midi_is_open(void);

// Optional speed trim (tenths of a millisecond, subtracted from each record's
// delay; 0 disables).  The scheduler uses the RP2350 clock, so these are a
// deliberate bias, not required calibration.  Overridable from Z80NEO.INI; see
// src/midi.c for how they are applied.
extern uint32_t midi_credit_group_x10;
extern uint32_t midi_credit_byte_x10;

// Next byte of the record stream; after the end of the song it keeps returning
// the 0xFF sentinel record.
uint8_t midi_read_byte(void);

// ---------------------------------------------------------------------------
// Console channel monitor.
//
// While a file is open the firmware keeps a per-channel "current note" table
// (16 rows, updated from the same messages it streams) and paints it on the
// console with VT102 cursor positioning.  The Z80 never draws or parses for the
// panel, so playback timing is untouched; the panel is refreshed from the main
// loop by midi_panel_poll().  Enabled by default; Z80NEO.INI MIDI_PANEL=0 (or
// MIDI_PANEL=OFF) turns it off.
// ---------------------------------------------------------------------------
void midi_panel_poll(void);
void midi_panel_set_enabled(bool on);
bool midi_panel_enabled(void);

#endif

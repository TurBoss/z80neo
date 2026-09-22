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

// Z80 replay-overhead calibration (tenths of a millisecond).  Overridable from
// Z80NEO.INI; see src/midi.c for how they are applied.
extern uint32_t midi_credit_group_x10;
extern uint32_t midi_credit_byte_x10;

// Next byte of the record stream; after the end of the song it keeps returning
// the 0xFF sentinel record.
uint8_t midi_read_byte(void);

#endif

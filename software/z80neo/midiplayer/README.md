# z80neo MIDI player

A bare-metal `+z80neo` program that plays a Standard MIDI File (`.mid`) out
**UART1 on ports `0x90` (data) / `0x91` (status)** at the MIDI rate — **31250
baud 8N1**.  UART1 is the firmware's second UART (GPIO36 = TX, GPIO37 = RX);
wire TX to a MIDI DIN socket's driver (or a 3.3 V synth's MIDI-in through the
usual current-loop/opto arrangement).

The player retunes UART1 itself: it selects firmware baud **code 10 (31250)**
on port `0x92` and format `0x03` (8N1) on `0x93`, overriding whatever
`UART1_BAUD` / `UART1_FORMAT` say in `Z80NEO.INI`.

**The firmware parses the file, the Z80 just streams it.**  At 128 kHz the Z80
cannot afford to parse SMF in C — an on-Z80 parser cost 100–230 ms per event
and made dense files ~15x slow — so the Pico parses the SMF, merges all `MTrk`
chunks by absolute tick, and hands the Z80 a stream of compact grouped
records.  The Z80 waits and emits from a hand-written assembly loop.
Measured on the board: byte-exact output at **1.00x** the file's tempo for the
8-track test file (63.55 s vs 63.58 s).

## Reading MIDI files

The Z80 cannot touch the SD card directly, so the firmware exposes a small
read-only API on ports **`0xA0`–`0xA7`** (see `firmware/z80neo/include/fileio.h`).
The player lists files with the directory commands, then opens the chosen file
with **`FILE_OPEN_MIDI` (command 8)**, which turns the data port `0xA2` into a
stream of grouped records (see `firmware/z80neo/include/midi.h`):

```
[0] delay lo    } milliseconds to wait before this record
[1] delay hi    }
[2] byte count  0 = delay only, 0xFF = end of song, else raw bytes follow
[3..]           byte count raw MIDI message bytes, forwarded as-is
```

Every channel message sharing a tick is coalesced into one record, so the Z80
waits once per tick and then blits that tick's bytes from a tight copy loop.
This is what keeps dense files (SNES rips with tens of thousands of events) from
outrunning the 128 kHz Z80: the per-record delay/console cost is paid once per
tick, not once per event.  A tick whose messages exceed `MIDI_GROUP_MAX` (255)
bytes is split; the extra records get delay 0.

Meta and SysEx events are consumed for their timing only; channel messages
(note on/off, program, control, pitch bend, aftertouch) are passed through
verbatim.  Gap delays longer than 65535 ms are split across delay-only records.

The firmware loads the whole file into a heap buffer at open (falling back to a
per-track streaming buffer if it does not fit), because a random `f_lseek` +
512-byte refill on this card can cost seconds.

The player looks in **`/MIDI`** first and falls back to the card **root** if
that directory is missing or empty.  `MIDI_DIR` in `midiplayer.c` sets the
directory; the firmware's `DIR_RESET` scans whatever directory path is in its
filename buffer.

## Build

```sh
make                 # -> build/midiplayer.ihx
make bin             # -> build/midiplayer (raw binary)
make Z88DK=/path/to/z88dk
```

Firmware: rebuild and flash (`firmware/build.sh`, `firmware/upload.sh`) —
`src/midi.c` must be present (it is listed in `CMakeLists.txt`).

## Deploy

1. Create a `MIDI` directory on the SD root (or use the root itself) and copy
   your `.mid` files there.
2. Copy `build/midiplayer.ihx` to the SD root.
3. Point `Z80NEO.INI` `PROG1` at `midiplayer.ihx`.
4. Reset.  The console lists the files found in `/MIDI` (or the root); type a
   number, a filename, or just press Enter for the first file.  Press `q` or
   `ESC` to stop the song: before returning, the player sends a **MIDI panic**
   (All Sound Off `CC120`, All Notes Off `CC123`, Reset All Controllers `CC121`,
   value 0) on all 16 channels so the synth does not leave notes hanging.  When a
   song stops (or a file fails to open) the list is shown again so another can be
   chosen without resetting; at the prompt `q` (or `Q`) exits the player.

## Timing / limits

- The replay loop is assembly (`midi_play` in `midi_delay.asm`); an `sccz80` C
  loop spent ~14 ms per record on instruction fetches alone and ran ~2x slow.
- The Z80 still costs a roughly fixed amount per record (instruction fetch +
  port access with `WAIT`), which is not part of the musical timeline.  Instead
  of modelling that cost with fixed constants, the firmware schedules every
  record against an **absolute playback clock** (the RP2350 microsecond timer):
  each tick has a due time, and the delay handed to the Z80 is recomputed as
  `due - now` on every record.  The Z80's replay cost shows up as elapsed time
  before it asks for the next record, so it is measured and absorbed rather than
  compensated by a tuned constant — a long song cannot drift, and the delay loop
  does not need recalibrating.  A gap longer than the 16-bit millisecond field
  is split into delay-only records so the bytes are never sent early.
  Re-measure only if the *absolute* timing is wrong (e.g. a wrong tempo map).

### Optional speed trim

Two optional trims remain for deliberate bias; they are **0** by default and
normally need no tuning.  They are read from `Z80NEO.INI` (no firmware rebuild
needed):

```ini
MIDI_GROUP_X10=0
MIDI_BYTE_X10=0
```

Each unit is a tenth of a millisecond **subtracted** from every record's delay
(so raising them plays faster): `MIDI_GROUP_X10` per record, `MIDI_BYTE_X10` per
forwarded byte.  Use them only to nudge a stream that is still slightly off after
the absolute scheduler; the defaults are correct for a clean build.

- The delay loop (`midi_delay_ms`) is calibrated for `CPU_SPEED` ~128 kHz
  **including** the Pico's per-access bus service (~9.6 µs), so `ld de,2`
  (a T-state-only loop ran ~25% long).  The absolute scheduler tolerates a
  miscalibrated loop (its error is bounded per record, not cumulative), so this
  is only a comfort setting now.
- Only the first 16 `MTrk` chunks are merged (`MIDI_MAX_TRACKS`); extra tracks
  are ignored.  SMF type 2 (independent sequences) is rejected, since a
  by-tick merge would scramble it.
- The menu lists up to `MAXF` (16) files and keeps up to 39 characters per
  name, so a number selection opens the exact file.  For a directory with more
  files, type a filename (relative to the directory in use) instead of a number.
- The Z80 program is ~3.8 KB and must fit in the 16 KB page-0 window.

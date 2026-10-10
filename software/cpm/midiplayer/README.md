# z80neo CP/M MIDI player

A CP/M 2.2 transient (`MIDI.COM`) that plays a Standard MIDI File (`.mid`) out
**UART1 on ports `0x90` (data) / `0x91` (status)** at the MIDI rate — **31250
baud 8N1** (GPIO38 = TX, GPIO39 = RX).  It is the CP/M twin of the bare-metal
`software/z80neo/midiplayer`; the two share the replay loop
(`software/z80neo/midiplayer/midi_delay.asm`).

**The firmware parses the file, the Z80 just streams it.**  `FILE_OPEN_MIDI`
(command 8 on port `0xA0`) makes the Pico parse the SMF, merge all `MTrk` by
absolute tick, and hand the Z80 a stream of grouped records on port `0xA2` (see
`firmware/z80neo/include/midi.h`):

```
[0] delay lo    } milliseconds to wait before this record
[1] delay hi    }
[2] byte count  0 = delay only, 0xFF = end of song, else raw bytes follow
[3..]           byte count raw MIDI message bytes, forwarded as-is
```

Every channel message sharing a tick is coalesced into one record, so the Z80
waits once per tick and then blits that tick's bytes.  Meta/SysEx events are
consumed for their timing only.  The firmware schedules each record against the
RP2350 clock (absolute due time, recomputed on every record), so `MIDI.COM` is
tempo-accurate without any tuned constants.

## Usage

```
A>MIDI [file.mid]
```

- With no argument it starts in **`/MIDI`** on the SD card (falling back to the
  card root) and walks the tree: directories are shown with a trailing `/`, a
  number opens a file or descends into a directory, `..` goes up, `n`/`p` page
  (20 entries per page, no cap), and `q` quits.  Any other input is a filename
  relative to the current directory.
- Playback modes at the prompt: **`a`** plays every file in the current
  directory in order, **`A`** loops it, **`r`** plays them in random order, and
  **`R`** loops the random order.  All four recurse into subdirectories
  (depth-first, so the whole subtree under the current directory is covered);
  random order shuffles each directory independently with the same
  memory-free coprime-stride permutation.  Unreadable files are passed over.
  `q`/`ESC` stops during playback.
- With an argument it plays that file directly and exits: a bare name
  (`SONG.MID`), a subdirectory path (`MIDI/SONG.MID`) or an absolute path
  (`/MIDI/SONG.MID`).
- While a song plays, press **`q`** or **`ESC`** to stop.  On stop the player
  sends a MIDI panic (All Sound Off `CC120`, All Notes Off `CC123`, Reset All
  Controllers `CC121`, value 0, on all 16 channels) so the synth does not leave
  notes hanging.

`MIDI.COM` retunes UART1 to 31250 8N1 itself and **restores the previous
settings on exit** (it saves ports `0x92`/`0x93` at start).

## Console MIDI monitor

While a song plays the **firmware** clears the console and paints a full-screen
MIDI monitor: a header with the file name, elapsed / total time, tempo (BPM) and
track count, then one row per MIDI channel (current note, velocity bar, General
MIDI instrument name, volume, pan), colour-coded.  `MIDI.COM` does not parse or
draw for it — the firmware already sees every channel message as it streams the
file — so playback timing is unaffected, and the bare-metal
`software/z80neo/midiplayer` gets the identical monitor from the same code.
Rows are rewritten in place (absolute VT102 cursor positioning, nothing scrolls)
only when a channel's state changes and are shown at the record's due time, so
the monitor never runs ahead of the music.  `Z80NEO.INI` `MIDI_PANEL=0` (or
`OFF`) disables it for a raw, non-VT102 terminal.

## Ports

| Port | Direction | Meaning |
|------|-----------|---------|
| `0x90` | out | UART1 data (MIDI bytes) |
| `0x91` | in  | UART1 status: bit0 = RX ready, bit1 = TX ready |
| `0x92`/`0x93` | w/r | UART1 baud code / format byte (saved and restored) |
| `0xA0` | w | file command (`FILE_OPEN_MIDI` 8, `FILE_CLOSE` 2, `FILE_REMOUNT_CPM` 9) |
| `0xA0` | r | file status |
| `0xA1` | w | append filename byte |
| `0xA1` | r | successive bytes of a `DIRNEXT` name (0 = end) |
| `0xA2` | r | record/data stream |
| `0xA3` | w | clear the filename buffer |

The stop key is polled straight off console ports `0x80`/`0x81` (the same ones
the BIOS uses), so no BDOS call sits in the playback loop.

## SD volume care (needs firmware command 9)

The firmware's file API and the CP/M disk emulation share the same FatFS volume,
so any file or directory access (opening a `.mid`, scanning `/MIDI`) mounts the
file API's object and **displaces the CP/M disk layer's mount**, invalidating
its open `CPMDISK*.IMG` handles.  Before returning to CP/M `MIDI.COM` issues
**`FILE_REMOUNT_CPM` (command 9)**, which drops the file API's cached mount and
re-opens the disk images so CCP disk I/O works again.

Command 9 is a firmware addition (`firmware/z80neo/src/fileio.c`,
`include/fileio.h`).  On older firmware it is ignored and CP/M disk I/O will
fail after the player exits — reset to recover.

## Build

```sh
make                 # -> build/MIDI.COM
make Z88DK=/path/to/z88dk
```

## Deploy

CP/M transients live on the emulated disks, not the FAT root.  Copy `MIDI.COM`
into an image with `cpmtools`:

```sh
cpmcp -f z80neo CPMDISK0.IMG build/MIDI.COM 0:MIDI.COM
```

(Or drag it into `tools/TBCPMTools`.)  Put the `.mid` files on the FAT side —
the card root or `/MIDI` — since the firmware reads those directly.

## Limits / notes

- Only the first 16 `MTrk` chunks are merged; type 2 (independent sequences) is
  rejected by the firmware.
- Playback shows the firmware's channel monitor (see above); the Z80 itself only
  prints the file list and the open/stop lines.
- The Z80 clock is 128 kHz (`Z80_KHZ` / `CPM` boot); the shared delay loop is
  calibrated for it.  If `CPU_SPEED` or the bus settle times change, retune
  `software/z80neo/midiplayer/midi_delay.asm`.
- `a`/`r` recurse, so the directory walk is depth-first and uses the CP/M stack
  (each level costs a `NAME_MAX` name plus a `NAME_MAX + 16` path, ~272 bytes).
  Nesting deeper than a few dozen levels could exhaust the stack.

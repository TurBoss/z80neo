;****************************************************************************
; disk.asm — Disk I/O routines for z80neo CP/M BIOS
;****************************************************************************

    INCLUDE "z80neo.inc"

; ── BIOS disk state variables ──────────────────────────────────────────

disk_track:     DEFW    0           ; current track (BC for SETTRK)
disk_sector:    DEFW    0           ; current sector
disk_dma:       DEFW    0080H       ; current DMA address (default 0x80)
disk_drive:     DEFB    0           ; currently selected drive (0=A)

; ── Public interface (called by BIOS jump table) ───────────────────────

;──────────────────────────────────────────────────────────────────────────
; disk_home — move to track 0
;──────────────────────────────────────────────────────────────────────────
disk_home:
    LD      BC, 0
    LD      (disk_track), BC
    XOR     A
    LD      (disk_sector), A
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_seldsk — select disk drive
;   entry: C = drive number (0=A, 1=B, ...)
;   exit:  HL = address of DPH (Disk Parameter Header), or 0 if invalid
;──────────────────────────────────────────────────────────────────────────
disk_seldsk:
    ; C = drive (0=A..).  Validate it, tell the Pico which image to use, and
    ; return that drive's DPH.
    LD      A, C
    CP      DISK_DRIVES
    JR      NC, seldsk_bad
    LD      (disk_drive), A
    OUT     (DISK_SELECT), A    ; Pico switches its active image
    LD      HL, dph_table       ; HL = dph_table + drive*16
    LD      B, 0
    LD      C, A
    SLA     C
    SLA     C
    SLA     C
    SLA     C
    ADD     HL, BC
    RET
seldsk_bad:
    LD      HL, 0               ; invalid drive
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_settrk — set track number
;   entry: BC = track number
;──────────────────────────────────────────────────────────────────────────
disk_settrk:
    LD      (disk_track), BC
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_setsec — set sector number
;   entry: BC = sector number
;──────────────────────────────────────────────────────────────────────────
disk_setsec:
    LD      A, C                ; sector is only 8-bit for floppy
    LD      (disk_sector), A
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_setdma — set DMA address for sector transfers
;   entry: BC = DMA address (must be 128-byte boundary)
;──────────────────────────────────────────────────────────────────────────
disk_setdma:
    LD      (disk_dma), BC
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_read — read one 128-byte sector into DMA buffer
;   exit: A = 0 on success, non-zero on error
;
; Uses OTIR to blast 128 bytes into the DMA buffer.
;──────────────────────────────────────────────────────────────────────────
disk_read:
    ; Issue READ command: track, sector, then CMD_READ
    LD      A, (disk_track)
    OUT     (DISK_TRACK), A
    LD      A, (disk_sector)
    OUT     (DISK_SECTOR), A
    XOR     A
    OUT     (DISK_CMD_DATA), A

    ; Check status
    IN      A, (DISK_STATUS)
    OR      A
    JR      NZ, disk_read_err

    ; Read 128 bytes
    LD      HL, (disk_dma)
    LD      B, SEC_SIZE
    LD      C, DISK_CMD_DATA
    INIR

    XOR     A
    RET

disk_read_err:
    LD      A, 1
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_write — write one 128-byte sector from DMA buffer
;   exit: A = 0 on success, non-zero on error
;──────────────────────────────────────────────────────────────────────────
disk_write:
    LD      A, (disk_track)
    OUT     (DISK_TRACK), A
    LD      A, (disk_sector)
    OUT     (DISK_SECTOR), A
    LD      A, DISK_CMD_WRITE
    OUT     (DISK_CMD_DATA), A
    LD      HL, (disk_dma)
    LD      B, SEC_SIZE
    LD      C, DISK_CMD_DATA
    OTIR
    IN      A, (DISK_STATUS)
    OR      A
    JR      NZ, disk_write_err
    XOR     A
    RET

disk_write_err:
    LD      A, 1
    RET

;──────────────────────────────────────────────────────────────────────────
; disk_sectrn — sector translation (no-op for our linear mapping)
;   entry: BC = logical sector, DE = translate table address
;   exit:  HL = physical sector
;──────────────────────────────────────────────────────────────────────────
disk_sectrn:
    LD      H, B
    LD      L, C
    RET

;──────────────────────────────────────────────────────────────────────────
; DISK PARAMETER BLOCKS
;
; CP/M 2.2 uses these structures:
;   DPB (Disk Parameter Block) — drive geometry
;   DPH (Disk Parameter Header) — per-drive pointers + DPB pointer
;
; Standard 8" SSSD: 77 tracks × 26 sectors × 128 bytes = 256,256 bytes
; Block size = 1024 bytes (BSH=3, BLM=7)
; Directory entries = 64 (max), allocation blocks = 243
;──────────────────────────────────────────────────────────────────────────

; DPB for drive A.  Must match diskdefs (tracks 254, 26 sects/track, 2K blocks,
; maxdir 128, boottrk 2) or the BDOS block/directory mapping disagrees with the
; image and reads the wrong sectors.  Data area = (254-2)*26*128/2048 = 409
; blocks; directory = 128 entries = 2 blocks (4 KB).
;
; 2K blocks are required here: with DSM > 255 CP/M uses 16-bit block numbers, so
; a directory entry holds 8 blocks.  At 1K blocks that is only 64 records, while
; the BDOS's GETBLOCK computes extent*16 + record/8 (16 blocks/extent) and would
; read records 64-127 past the entry.  At 2K blocks each entry is exactly one
; 16K extent (128 records) and everything lines up.  (cpmtools also mishandles
; the 1K/DSM>255 combination, dropping the first 8K of files on write.)
dpb_table:
    DEFW    SECS_PER_TRACK       ; SPT — sectors per track (26)
    DEFB    4                    ; BSH — block shift factor (2K blocks)
    DEFB    15                   ; BLM — block mask (2K→0x0F)
    DEFB    0                    ; EXM — extent mask
    DEFW    408                  ; DSM — total data blocks - 1 (409 × 2K blocks)
    DEFW    127                  ; DRM — directory entries - 1 (128 entries)
    DEFB    0C0H                 ; AL0 — dir blocks 0-1 allocated (2 blocks = 4 KB)
    DEFB    0                    ; AL1
    DEFW    32                   ; CKS — directory check size (DRM+1)/4 = 128/4
    DEFW    2                    ; OFF — track offset (2 reserved tracks)

; DPH per drive (16 bytes each), one per drive A..O (15).  All drives share the
; same geometry (dpb_table) and directory buffer; CSV/ALV are per-drive because
; the BDOS keeps allocation and checksum state there.  Order: XLT, 3× scratch,
; DIRBUF, DPB, CSV, ALV.  Count must match DISK_DRIVES.
dph_table:
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_0,  alv_0
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_1,  alv_1
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_2,  alv_2
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_3,  alv_3
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_4,  alv_4
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_5,  alv_5
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_6,  alv_6
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_7,  alv_7
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_8,  alv_8
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_9,  alv_9
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_10, alv_10
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_11, alv_11
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_12, alv_12
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_13, alv_13
    DEFW    0, 0, 0, 0, dir_buf, dpb_table, csv_14, alv_14

; Directory buffer (128 bytes, shared across drives)
dir_buf:
    DEFS    128

; Per-drive ALV (128 bytes) and CSV (128 bytes), zero-filled
alv_0:  DEFS    128, 0
csv_0:  DEFS    128, 0
alv_1:  DEFS    128, 0
csv_1:  DEFS    128, 0
alv_2:  DEFS    128, 0
csv_2:  DEFS    128, 0
alv_3:  DEFS    128, 0
csv_3:  DEFS    128, 0
alv_4:  DEFS    128, 0
csv_4:  DEFS    128, 0
alv_5:  DEFS    128, 0
csv_5:  DEFS    128, 0
alv_6:  DEFS    128, 0
csv_6:  DEFS    128, 0
alv_7:  DEFS    128, 0
csv_7:  DEFS    128, 0
alv_8:  DEFS    128, 0
csv_8:  DEFS    128, 0
alv_9:  DEFS    128, 0
csv_9:  DEFS    128, 0
alv_10: DEFS    128, 0
csv_10: DEFS    128, 0
alv_11: DEFS    128, 0
csv_11: DEFS    128, 0
alv_12: DEFS    128, 0
csv_12: DEFS    128, 0
alv_13: DEFS    128, 0
csv_13: DEFS    128, 0
alv_14: DEFS    128, 0
csv_14: DEFS    128, 0

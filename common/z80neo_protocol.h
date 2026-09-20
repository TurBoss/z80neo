/*
 * z80neo_protocol.h - console protocol shared by the VDP firmware and the PC
 * sdl_client.
 *
 * This is the single definition of the VDP "ESC g" bitmap-graphics protocol;
 * both sides include it so the command bytes and payload lengths cannot
 * drift.  The ANSI/VT102 text subset they share is documented in
 * doc/vdp_protocol.md.
 *
 * Frame:  ESC 'g' <cmd> <payload...>
 * The payload is raw bytes; coordinate words are little-endian (lo, hi).
 */
#ifndef Z80NEO_PROTOCOL_H
#define Z80NEO_PROTOCOL_H

#define Z80NEO_ESC 27
#define Z80NEO_GFX_ID 'g'

/* cmd            payload                                                bytes */
#define Z80NEO_GFX_MODE 0x01     /* <on:1=bitmap,0=text>                       1 */
#define Z80NEO_GFX_CLEAR 0x02    /* <0>                                        1 */
#define Z80NEO_GFX_COLOR 0x03    /* <fg> <bg>                                  2 */
#define Z80NEO_GFX_PIXEL 0x04    /* <x lo> <hi> <y lo> <hi> <on>               5 */
#define Z80NEO_GFX_LINE 0x05     /* <x0 lo hi> <y0 lo hi> <x1 lo hi> <y1 lo hi> <on>  9 */
#define Z80NEO_GFX_FILL 0x06     /* <x lo hi> <y lo hi> <w lo hi> <h lo hi> <on>      9 */
#define Z80NEO_GFX_PIXEL_C 0x07  /* <x lo> <hi> <y lo> <hi> <colour 0-15>       5 */

/* Number of payload bytes for a graphics command, or -1 if it is not one. */
static inline int z80neo_gfx_payload_len(unsigned char cmd)
{
	switch (cmd) {
	case Z80NEO_GFX_MODE:
	case Z80NEO_GFX_CLEAR:
		return 1;
	case Z80NEO_GFX_COLOR:
		return 2;
	case Z80NEO_GFX_PIXEL:
	case Z80NEO_GFX_PIXEL_C:
		return 5;
	case Z80NEO_GFX_LINE:
	case Z80NEO_GFX_FILL:
		return 9;
	default:
		return -1;
	}
}

#endif

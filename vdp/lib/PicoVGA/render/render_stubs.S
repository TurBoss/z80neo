// -----------------------------------------------------------------------------
// render_stubs.S - placeholders for PicoVGA renderers that this board does not
// use (perspective / tile / gradient-text / mono+attribute text).
//
// The originals drive the RP2040 SIO hardware divider, which does not exist on
// the RP2350, so they are excluded from the build.  The vga_render.S jump table
// still references their entry points, so provide no-op stubs here to keep the
// link working.  Only the text modes used by this terminal are real:
//   RenderColor (vga_color.S) and RenderCText (vga_ctext.S).
// -----------------------------------------------------------------------------

	.syntax unified
	.thumb
	.text

.macro RENDER_STUB name
	.globl	\name
	.thumb_func
	.type	\name, %function
	.p2align 1
\name:
	bx	lr
	.size	\name, .-\name
.endm

	RENDER_STUB RenderAText
	RENDER_STUB RenderDText
	RENDER_STUB RenderFText
	RENDER_STUB RenderGText
	RENDER_STUB RenderMText
	RENDER_STUB RenderGraph8Persp
	RENDER_STUB RenderPersp
	RENDER_STUB RenderPersp2
	RENDER_STUB RenderTile
	RENDER_STUB RenderTile2
	RENDER_STUB RenderTilePersp
	RENDER_STUB RenderTilePersp15
	RENDER_STUB RenderTilePersp2
	RENDER_STUB RenderTilePersp3
	RENDER_STUB RenderTilePersp4

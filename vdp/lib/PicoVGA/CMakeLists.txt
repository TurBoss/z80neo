# PicoVGA - VGA output library for Raspberry Pi Pico
# Ported to pico-sdk 2.x

add_library(PicoVGA INTERFACE)

set(_picovga_dir ${CMAKE_CURRENT_LIST_DIR})

# Core + renderers that don't touch the RP2040 SIO hardware divider.
set(_picovga_common
        ${_picovga_dir}/vga.cpp
        ${_picovga_dir}/vga_layer.cpp
        ${_picovga_dir}/vga_pal.cpp
        ${_picovga_dir}/vga_screen.cpp
        ${_picovga_dir}/vga_util.cpp
        ${_picovga_dir}/vga_vmode.cpp
        ${_picovga_dir}/vga_blitkey.S
        ${_picovga_dir}/vga_render.S
        ${_picovga_dir}/util/overclock.cpp
        ${_picovga_dir}/render/vga_attrib8.S
        ${_picovga_dir}/render/vga_color.S
        ${_picovga_dir}/render/vga_fastsprite.S
        ${_picovga_dir}/render/vga_graph1.S
        ${_picovga_dir}/render/vga_graph2.S
        ${_picovga_dir}/render/vga_graph4.S
        ${_picovga_dir}/render/vga_graph8.S
        ${_picovga_dir}/render/vga_graph8mat.S
        ${_picovga_dir}/render/vga_level.S
        ${_picovga_dir}/render/vga_levelgrad.S
        ${_picovga_dir}/render/vga_oscil.S
        ${_picovga_dir}/render/vga_oscline.S
        ${_picovga_dir}/render/vga_plane2.S
        ${_picovga_dir}/render/vga_progress.S
        ${_picovga_dir}/render/vga_sprite.S
        )

# Renderers that drive the RP2040 SIO hardware divider.
set(_picovga_divider
        ${_picovga_dir}/render/vga_atext.S
        ${_picovga_dir}/render/vga_ctext.S
        ${_picovga_dir}/render/vga_dtext.S
        ${_picovga_dir}/render/vga_ftext.S
        ${_picovga_dir}/render/vga_graph8persp.S
        ${_picovga_dir}/render/vga_gtext.S
        ${_picovga_dir}/render/vga_mtext.S
        ${_picovga_dir}/render/vga_persp.S
        ${_picovga_dir}/render/vga_persp2.S
        ${_picovga_dir}/render/vga_tile.S
        ${_picovga_dir}/render/vga_tile2.S
        ${_picovga_dir}/render/vga_tilepersp.S
        ${_picovga_dir}/render/vga_tilepersp15.S
        ${_picovga_dir}/render/vga_tilepersp2.S
        ${_picovga_dir}/render/vga_tilepersp3.S
        ${_picovga_dir}/render/vga_tilepersp4.S
        )

if(PICO_RP2350)
    # RP2350 has no SIO hardware divider.  vga_ctext.S is ported to udiv/mls;
    # the perspective/tile/gradient-text renderers are not used by this board,
    # so their jump-table slots are filled by no-op stubs (render_stubs.S).
    target_sources(PicoVGA INTERFACE
            ${_picovga_common}
            ${_picovga_dir}/render/vga_ctext.S
            ${_picovga_dir}/render/render_stubs.S
            )
else()
    target_sources(PicoVGA INTERFACE ${_picovga_common} ${_picovga_divider})
endif()

target_include_directories(PicoVGA INTERFACE ${_picovga_dir})

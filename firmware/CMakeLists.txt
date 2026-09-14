cmake_minimum_required(VERSION 3.25)

# Board: Olimex RP2350-PICO2-BB48 (RP2350B, 48 GPIOs, 16 MB flash).
# Must be set before pico_sdk_init() so the SDK resolves our board header.
set(PICO_BOARD z80neo_bb48)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

set(PROJECT_NAME "z80neo" C CXX ASM)


project(${PROJECT_NAME})

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


set(PICO_EXAMPLES_PATH ${PROJECT_SOURCE_DIR})

# Larger stack for the deep FatFS/SPI disk-I/O call chain in the Z80 bus handler
# (max 0x1000 without a custom linker script — SCRATCH_X/Y regions are 4k each)
add_compile_definitions(PICO_STACK_SIZE=0x1000)

pico_sdk_init()


include(example_auto_set_url.cmake)

add_subdirectory(z80neo)



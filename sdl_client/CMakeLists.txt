cmake_minimum_required(VERSION 3.16)
project(z80neo_sdl_client C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)

add_executable(z80neo_client
	src/main.c
	src/terminal.c
	src/render.c
	src/serial.c
)

target_include_directories(z80neo_client PRIVATE src)
target_link_libraries(z80neo_client PRIVATE PkgConfig::SDL3 m)
target_compile_options(z80neo_client PRIVATE -Wall -Wextra)

install(TARGETS z80neo_client RUNTIME DESTINATION bin)

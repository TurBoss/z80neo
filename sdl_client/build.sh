#!/bin/sh
set -e
cd "$(dirname "$0")"
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

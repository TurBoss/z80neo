#!/bin/bash
# Launch the Z80NEO Loader GUI.  Uses a bundled venv if one exists, otherwise
# the system Python (which needs PySide6 and pyserial).
cd "$(dirname "$0")" || exit 1
if [ -x venv/bin/python ]; then
    exec venv/bin/python z80neo_loader.py "$@"
fi
exec python3 z80neo_loader.py "$@"

#!/bin/bash
# Launch TurBoCPMTools using the bundled virtualenv.
cd "$(dirname "$0")" || exit 1
source venv/bin/activate
exec python turbocpmtools.py "$@"

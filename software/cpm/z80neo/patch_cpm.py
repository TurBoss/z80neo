#!/usr/bin/env python3
"""patch_cpm.py — currently disabled: RESDSK call left intact"""
import sys
fn = sys.argv[1] if len(sys.argv) > 1 else "build/ccp_bdos.bin"
d = bytearray(open(fn, "rb").read())
# RESDSK kept active for disk testing
# d[0x0371] = 0; d[0x0372] = 0; d[0x0373] = 0
open(fn, "wb").write(d)
print("Patched: no changes (RESDSK active)")

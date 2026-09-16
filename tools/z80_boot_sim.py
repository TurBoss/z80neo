#!/usr/bin/env python3
"""z80_boot_sim.py — Z80 emulator + mock z80neo firmware ports.

Runs the CP/M 2.2 z80neo image (cpmz80neo.hex) from reset with a faithful
mock of the Pico firmware's I/O port behaviour (including the A0-stuck-HIGH
aliasing described in the firmware) and serves real sector data from
CPMDISK.IMG so we can see exactly where the cold boot goes wrong.
"""
import sys, os, collections

# ---------------------------------------------------------------------------
# Intel HEX load (plain 16-bit, like the firmware's hex loader for this file)
# ---------------------------------------------------------------------------
def load_hex(path, mem, base=0):
    for line in open(path):
        line = line.strip()
        if not line or line[0] != ':':
            continue
        n = int(line[1:3], 16)
        addr = int(line[3:7], 16)
        typ = int(line[7:9], 16)
        data = bytes(int(line[9+i*2:11+i*2], 16) for i in range(n))
        if typ == 0:
            for j, b in enumerate(data):
                a = base + addr + j
                if a < 0x4000:
                    for bk in range(4):
                        mem[bk*0x4000 + (a & 0x3FFF)] = b
                else:
                    page = (a >> 14) & 3
                    mem[page*0x4000 + (a & 0x3FFF)] = b
        elif typ == 1:
            break

# ---------------------------------------------------------------------------
# Z80 core (8080 + the Z80 instructions CP/M 2.2 uses)
# ---------------------------------------------------------------------------
class Z80:
    def __init__(self, mem, io_in, io_out):
        self.mem = mem
        self.io_in = io_in
        self.io_out = io_out
        self.A = self.F = 0
        self.BC = self.DE = self.HL = 0
        self.IX = self.IY = 0
        self.SP = 0
        self.PC = 0
        self.alt_af = self.alt_bc = self.alt_de = self.alt_hl = 0
        self.I = self.R = 0
        self.IFF1 = self.IFF2 = 0
        self.halted = False
        self.cycles = 0

    # flag helpers
    def _s(self, v): return 0x80 if (v & 0x80) else 0
    def _z(self, v): return 0x40 if (v & 0xFF) == 0 else 0
    def _p(self, v):
        # parity of v & 0xFF
        return 0x04 if (bin(v & 0xFF).count('1') & 1) == 0 else 0
    def _h_add(self, a, b): return 0x10 if ((a & 0xF) + (b & 0xF)) > 0xF else 0
    def _h_sub(self, a, b): return 0x10 if ((a & 0xF) - (b & 0xF)) < 0 else 0
    def _c(self, v): return 0x01 if (v & 0x100) else 0

    def flags_8(self, v, carry=0, half=0, add=True):
        self.F = self._s(v) | self._z(v) | self._p(v) | carry | half
        return v & 0xFF

    def rd8(self, a): return self.mem[a & 0xFFFF]
    def wr8(self, a, v): self.mem[a & 0xFFFF] = v & 0xFF
    def rd16(self, a): return self.mem[a & 0xFFFF] | (self.mem[(a+1) & 0xFFFF] << 8)
    def wr16(self, a, v): self.mem[a & 0xFFFF] = v & 0xFF; self.mem[(a+1) & 0xFFFF] = (v >> 8) & 0xFF

    def push(self, v): self.SP = (self.SP - 1) & 0xFFFF; self.wr8(self.SP, (v >> 8) & 0xFF); self.SP = (self.SP - 1) & 0xFFFF; self.wr8(self.SP, v & 0xFF)
    def pop(self): v = self.rd8(self.SP); self.SP = (self.SP + 1) & 0xFFFF; v |= self.rd8(self.SP) << 8; self.SP = (self.SP + 1) & 0xFFFF; return v & 0xFFFF

    def step(self):
        if self.halted:
            return
        pc = self.PC
        op = self.rd8(pc); self.PC = (pc + 1) & 0xFFFF
        self.cycles += 1
        f = self.F
        s = self.SP

        # ---- 8080 main table ----
        if op == 0x00: pass
        elif op == 0x01: self.BC = self.rd16(self.PC); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x02: self.wr8(self.BC, self.A)
        elif op == 0x03: self.BC = (self.BC + 1) & 0xFFFF
        elif op == 0x04: self.B = self.inc8(self.B)
        elif op == 0x05: self.B = self.dec8(self.B)
        elif op == 0x06: self.B = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x07: self.A = ((self.A << 1) | (self.A >> 7)) & 0xFF; self.F = (self.F & 0xC4) | (0x01 if self.A & 1 else 0)  # RLC (approximation ok for CCP use)
        elif op == 0x08:  # EX AF,AF'
            a, f = self.A, self.F
            self.A = (self.alt_af >> 8) & 0xFF
            self.F = self.alt_af & 0xFF
            self.alt_af = (a << 8) | f
        elif op == 0x09: self.HL = (self.HL + self.BC) & 0xFFFF; self.F = (self.F & 0xC4) | (0x01 if self.HL < self.BC else 0) | 0x10
        elif op == 0x0A: self.A = self.rd8(self.BC)
        elif op == 0x0B: self.BC = (self.BC - 1) & 0xFFFF
        elif op == 0x0C: self.BC = (self.BC & 0xFF00) | self.inc8(self.BC & 0xFF)
        elif op == 0x0D: self.BC = (self.BC & 0xFF00) | self.dec8(self.BC & 0xFF)
        elif op == 0x0E: self.BC = (self.BC & 0xFF00) | self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x0F: self.A = ((self.A >> 1) | (self.A << 7)) & 0xFF; self.F = (self.F & 0xC4) | (0x01 if self.A & 0x80 else 0)  # RRC
        elif op == 0x11: self.DE = self.rd16(self.PC); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x12: self.wr8(self.DE, self.A)
        elif op == 0x13: self.DE = (self.DE + 1) & 0xFFFF
        elif op == 0x14: self.D = self.inc8(self.D)
        elif op == 0x15: self.D = self.dec8(self.D)
        elif op == 0x16: self.D = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x17: c = 1 if self.F & 1 else 0; n = (self.A >> 7) & 1; self.A = ((self.A << 1) | c) & 0xFF; self.F = (self.F & 0xC4) | n  # RAL
        elif op == 0x18: self.PC = (self.PC + self.signed(self.rd8(self.PC))) & 0xFFFF; self.PC = (self.PC + 1) & 0xFFFF  # JR
        elif op == 0x19: self.HL = (self.HL + self.DE) & 0xFFFF; self.F = (self.F & 0xC4) | (0x01 if self.HL < self.DE else 0) | 0x10
        elif op == 0x1A: self.A = self.rd8(self.DE)
        elif op == 0x1B: self.DE = (self.DE - 1) & 0xFFFF
        elif op == 0x1C: self.DE = (self.DE & 0xFF00) | self.inc8(self.DE & 0xFF)
        elif op == 0x1D: self.DE = (self.DE & 0xFF00) | self.dec8(self.DE & 0xFF)
        elif op == 0x1E: self.DE = (self.DE & 0xFF00) | self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x1F: c = 1 if self.F & 1 else 0; n = self.A & 1; self.A = ((self.A >> 1) | (c << 7)) & 0xFF; self.F = (self.F & 0xC4) | n  # RAR
        elif op == 0x20:  # JR NZ
            if not (self.F & 0x40): self.PC = (self.PC + self.signed(self.rd8(self.PC))) & 0xFFFF
            self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x21: self.HL = self.rd16(self.PC); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x22: self.wr16(self.rd16(self.PC), self.HL); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x23: self.HL = (self.HL + 1) & 0xFFFF
        elif op == 0x24: self.H = self.inc8(self.H)
        elif op == 0x25: self.H = self.dec8(self.H)
        elif op == 0x26: self.H = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x27: self.daa()
        elif op == 0x28:  # JR Z
            if self.F & 0x40: self.PC = (self.PC + self.signed(self.rd8(self.PC))) & 0xFFFF
            self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x29: self.HL = (self.HL + self.HL) & 0xFFFF; self.F = (self.F & 0xC4) | (0x01 if self.HL & 0x8000 else 0) | 0x10
        elif op == 0x2A: self.HL = self.rd16(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x2B: self.HL = (self.HL - 1) & 0xFFFF
        elif op == 0x2C: self.HL = (self.HL & 0xFF00) | self.inc8(self.HL & 0xFF)
        elif op == 0x2D: self.HL = (self.HL & 0xFF00) | self.dec8(self.HL & 0xFF)
        elif op == 0x2E: self.HL = (self.HL & 0xFF00) | self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x2F: self.A = (~self.A) & 0xFF; self.F = (self.F & 0xC5) | 0x10  # CPL
        elif op == 0x30:  # JR NC
            if not (self.F & 1): self.PC = (self.PC + self.signed(self.rd8(self.PC))) & 0xFFFF
            self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x31: self.SP = self.rd16(self.PC); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x32: self.wr8(self.rd16(self.PC), self.A); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x33: self.SP = (self.SP + 1) & 0xFFFF
        elif op == 0x34: self.inc8_m(self.HL)
        elif op == 0x35: self.dec8_m(self.HL)
        elif op == 0x36: self.wr8(self.HL, self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x37: self.F |= 0x01  # SCF
        elif op == 0x38:  # JR C
            if self.F & 1: self.PC = (self.PC + self.signed(self.rd8(self.PC))) & 0xFFFF
            self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x39: self.HL = (self.HL + self.SP) & 0xFFFF; self.F = (self.F & 0xC4) | (0x01 if self.HL < self.SP else 0) | 0x10
        elif op == 0x3A: self.A = self.rd8(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op == 0x3B: self.SP = (self.SP - 1) & 0xFFFF
        elif op == 0x3C: self.A = self.inc8(self.A)
        elif op == 0x3D: self.A = self.dec8(self.A)
        elif op == 0x3E: self.A = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0x3F: self.F = (self.F & 0xFE) | ((self.F >> 4) & 1)  # CCF approx
        elif 0x40 <= op <= 0x6F:
            src = (op & 7)
            dst = (op >> 3) & 7
            # source register for group 6 is (HL)
            if src == 6:
                val = self.rd8(self.HL)
            else:
                val = [self.B, self.C, self.D, self.E, self.H, self.L, 0, self.A][src]
            # 0x40-0x47: LD B,r ; 0x48-0x4F: LD C,r ; 0x50-0x57: LD D,r ;
            # 0x58-0x5F: LD E,r ; 0x60-0x67: LD H,r ; 0x68-0x6F: LD L,r ;
            if dst == 0: self.B = val
            elif dst == 1: self.C = val
            elif dst == 2: self.D = val
            elif dst == 3: self.E = val
            elif dst == 4: self.H = val
            elif dst == 5: self.L = val
            # dst == 6 would be LD (HL),r which is handled in 0x70-0x77
        elif op == 0x70: self.wr8(self.HL, self.B)
        elif op == 0x71: self.wr8(self.HL, self.C)
        elif op == 0x72: self.wr8(self.HL, self.D)
        elif op == 0x73: self.wr8(self.HL, self.E)
        elif op == 0x74: self.wr8(self.HL, self.H)
        elif op == 0x75: self.wr8(self.HL, self.L)
        elif op == 0x76: self.halted = True
        elif op == 0x77: self.wr8(self.HL, self.A)
        elif 0x78 <= op <= 0x7F:  # LD A,r
            src = op & 7
            if src == 6:
                self.A = self.rd8(self.HL)
            else:
                self.A = [self.B, self.C, self.D, self.E, self.H, self.L, 0, self.A][src]
        elif op == 0x80: self.add8(self.B)
        elif op == 0x81: self.add8(self.C)
        elif op == 0x82: self.add8(self.D)
        elif op == 0x83: self.add8(self.E)
        elif op == 0x84: self.add8(self.H)
        elif op == 0x85: self.add8(self.L)
        elif op == 0x86: self.add8(self.rd8(self.HL))
        elif op == 0x87: self.add8(self.A)
        elif op == 0x88: self.adc8(self.B)
        elif op == 0x89: self.adc8(self.C)
        elif op == 0x8A: self.adc8(self.D)
        elif op == 0x8B: self.adc8(self.E)
        elif op == 0x8C: self.adc8(self.H)
        elif op == 0x8D: self.adc8(self.L)
        elif op == 0x8E: self.adc8(self.rd8(self.HL))
        elif op == 0x8F: self.adc8(self.A)
        elif op == 0x90: self.sub8(self.B)
        elif op == 0x91: self.sub8(self.C)
        elif op == 0x92: self.sub8(self.D)
        elif op == 0x93: self.sub8(self.E)
        elif op == 0x94: self.sub8(self.H)
        elif op == 0x95: self.sub8(self.L)
        elif op == 0x96: self.sub8(self.rd8(self.HL))
        elif op == 0x97: self.sub8(self.A)
        elif op == 0x98: self.sbc8(self.B)
        elif op == 0x99: self.sbc8(self.C)
        elif op == 0x9A: self.sbc8(self.D)
        elif op == 0x9B: self.sbc8(self.E)
        elif op == 0x9C: self.sbc8(self.H)
        elif op == 0x9D: self.sbc8(self.L)
        elif op == 0x9E: self.sbc8(self.rd8(self.HL))
        elif op == 0x9F: self.sbc8(self.A)
        elif op == 0xA0: self.and8(self.B)
        elif op == 0xA1: self.and8(self.C)
        elif op == 0xA2: self.and8(self.D)
        elif op == 0xA3: self.and8(self.E)
        elif op == 0xA4: self.and8(self.H)
        elif op == 0xA5: self.and8(self.L)
        elif op == 0xA6: self.and8(self.rd8(self.HL))
        elif op == 0xA7: self.and8(self.A)
        elif op == 0xA8: self.xor8(self.B)
        elif op == 0xA9: self.xor8(self.C)
        elif op == 0xAA: self.xor8(self.D)
        elif op == 0xAB: self.xor8(self.E)
        elif op == 0xAC: self.xor8(self.H)
        elif op == 0xAD: self.xor8(self.L)
        elif op == 0xAE: self.xor8(self.rd8(self.HL))
        elif op == 0xAF: self.xor8(self.A)
        elif op == 0xB0: self.or8(self.B)
        elif op == 0xB1: self.or8(self.C)
        elif op == 0xB2: self.or8(self.D)
        elif op == 0xB3: self.or8(self.E)
        elif op == 0xB4: self.or8(self.H)
        elif op == 0xB5: self.or8(self.L)
        elif op == 0xB6: self.or8(self.rd8(self.HL))
        elif op == 0xB7: self.or8(self.A)
        elif op == 0xB8: self.cp8(self.B)
        elif op == 0xB9: self.cp8(self.C)
        elif op == 0xBA: self.cp8(self.D)
        elif op == 0xBB: self.cp8(self.E)
        elif op == 0xBC: self.cp8(self.H)
        elif op == 0xBD: self.cp8(self.L)
        elif op == 0xBE: self.cp8(self.rd8(self.HL))
        elif op == 0xBF: self.cp8(self.A)
        elif op == 0xC0: self.ret_cc(0x40, False)
        elif op == 0xC1: self.BC = self.pop()
        elif op == 0xC2: self.jp_cc(0x40, False)
        elif op == 0xC3: self.PC = self.rd16(self.PC)
        elif op == 0xC4: self.call_cc(0x40, False)
        elif op == 0xC5: self.push(self.BC)
        elif op == 0xC6: self.add8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xC7: self.rst(0x00)
        elif op == 0xC8: self.ret_cc(0x40, True)
        elif op == 0xC9: self.PC = self.pop()
        elif op == 0xCA: self.jp_cc(0x40, True)
        elif op == 0xCB: self.cb_op()
        elif op == 0xCC: self.call_cc(0x40, True)
        elif op == 0xCD: self.call(self.rd16(self.PC))
        elif op == 0xCE: self.adc8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xCF: self.rst(0x08)
        elif op == 0xD0: self.ret_cc(0x01, False)
        elif op == 0xD1: self.DE = self.pop()
        elif op == 0xD2: self.jp_cc(0x01, False)
        elif op == 0xD3: self.io_out(self.rd8(self.PC), self.A); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xD4: self.call_cc(0x01, False)
        elif op == 0xD5: self.push(self.DE)
        elif op == 0xD6: self.sub8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xD7: self.rst(0x10)
        elif op == 0xD8: self.ret_cc(0x01, True)
        elif op == 0xD9: self.alt_bc, self.BC = self.BC, self.alt_bc; self.alt_de, self.DE = self.DE, self.alt_de; self.alt_hl, self.HL = self.HL, self.alt_hl  # EXX
        elif op == 0xDA: self.jp_cc(0x01, True)
        elif op == 0xDB: self.A = self.io_in(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xDC: self.call_cc(0x01, True)
        elif op == 0xDE: self.sbc8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xDF: self.rst(0x18)
        elif op == 0xE0: self.ret_cc(0x04, False)
        elif op == 0xE1: self.HL = self.pop()
        elif op == 0xE2: self.jp_cc(0x04, False)
        elif op == 0xE3: self.HL, v = self.rd16(self.SP), self.HL; self.wr16(self.SP, v)  # EX (SP),HL
        elif op == 0xE4: self.call_cc(0x04, False)
        elif op == 0xE5: self.push(self.HL)
        elif op == 0xE6: self.and8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xE7: self.rst(0x20)
        elif op == 0xE8: self.ret_cc(0x04, True)
        elif op == 0xE9: self.PC = self.HL
        elif op == 0xEA: self.jp_cc(0x04, True)
        elif op == 0xEB: self.DE, self.HL = self.HL, self.DE  # EX DE,HL
        elif op == 0xEC: self.call_cc(0x04, True)
        elif op == 0xED: self.ed_op()
        elif op == 0xEE: self.xor8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xEF: self.rst(0x28)
        elif op == 0xF0: self.ret_cc(0x80, False)
        elif op == 0xF1:  # POP AF
            v = self.pop()
            self.A = (v >> 8) & 0xFF
            self.F = v & 0xFF
        elif op == 0xF2: self.jp_cc(0x80, False)
        elif op == 0xF3: self.IFF1 = 0  # DI
        elif op == 0xF4: self.call_cc(0x80, False)
        elif op == 0xF5: self.push((self.A << 8) | self.F)
        elif op == 0xF6: self.or8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xF7: self.rst(0x30)
        elif op == 0xF8: self.ret_cc(0x80, True)
        elif op == 0xF9: self.SP = self.HL
        elif op == 0xFA: self.jp_cc(0x80, True)
        elif op == 0xFB: self.IFF1 = 1  # EI
        elif op == 0xFC: self.call_cc(0x80, True)
        elif op == 0xFE: self.cp8(self.rd8(self.PC)); self.PC = (self.PC + 1) & 0xFFFF
        elif op == 0xFF: self.rst(0x38)
        else:
            raise Exception("unimplemented op %02X at PC=%04X" % (op, pc))

    # ---- helpers ----
    def signed(self, v): return v - 256 if v & 0x80 else v

    def inc8(self, v):
        r = (v + 1) & 0xFF
        self.F = (self.F & 0x01) | self._s(r) | self._z(r) | self._p(r) | (0x10 if (v & 0xF) == 0xF else 0)
        return r

    def dec8(self, v):
        r = (v - 1) & 0xFF
        self.F = (self.F & 0x01) | self._s(r) | self._z(r) | self._p(r) | (0x10 if (v & 0xF) == 0 else 0) | 0x02
        return r

    def inc8_m(self, a):
        v = self.rd8(a); self.wr8(a, self.inc8(v))

    def dec8_m(self, a):
        v = self.rd8(a); self.wr8(a, self.dec8(v))

    def add8(self, v):
        r = self.A + v
        self.F = self._s(r) | self._z(r) | self._p(r) | self._c(r) | self._h_add(self.A, v)
        self.A = r & 0xFF

    def adc8(self, v):
        c = self.F & 1
        r = self.A + v + c
        self.F = self._s(r) | self._z(r) | self._p(r) | self._c(r) | self._h_add(self.A, v + c) | (self.F & 0x02)
        self.A = r & 0xFF

    def sub8(self, v):
        r = self.A - v
        self.F = self._s(r) | self._z(r) | self._p(r) | (0x01 if r < 0 else 0) | self._h_sub(self.A, v) | 0x02
        self.A = r & 0xFF

    def sbc8(self, v):
        c = self.F & 1
        r = self.A - v - c
        self.F = self._s(r) | self._z(r) | self._p(r) | (0x01 if r < 0 else 0) | self._h_sub(self.A, v + c) | 0x02
        self.A = r & 0xFF

    def and8(self, v):
        self.A &= v
        self.F = self._s(self.A) | self._z(self.A) | self._p(self.A) | 0x10

    def xor8(self, v):
        self.A ^= v
        self.F = self._s(self.A) | self._z(self.A) | self._p(self.A)

    def or8(self, v):
        self.A |= v
        self.F = self._s(self.A) | self._z(self.A) | self._p(self.A)

    def cp8(self, v):
        r = self.A - v
        self.F = self._s(r) | self._z(r) | self._p(r) | (0x01 if r < 0 else 0) | self._h_sub(self.A, v) | 0x02

    def daa(self):
        a = self.A
        if (a & 0x0F) > 9 or (self.F & 0x10):
            a += 6
        if (a & 0xF0) > 0x90 or (self.F & 0x01):
            a += 0x60
        self.A = a & 0xFF
        self.F = self._s(self.A) | self._z(self.A) | self._p(self.A) | (0x01 if a & 0x100 else 0)

    def ret_cc(self, mask, cond):
        if bool(self.F & mask) == cond:
            self.PC = self.pop()

    def jp_cc(self, mask, cond):
        nn = self.rd16(self.PC)
        if bool(self.F & mask) == cond:
            self.PC = nn
        else:
            self.PC = (self.PC + 2) & 0xFFFF

    def call_cc(self, mask, cond):
        if bool(self.F & mask) == cond:
            self.call(self.rd16(self.PC))
        else:
            self.PC = (self.PC + 2) & 0xFFFF

    def call(self, nn):
        self.push(self.PC + 2 & 0xFFFF)
        self.PC = nn

    def rst(self, n):
        self.push(self.PC)
        self.PC = n

    def cb_op(self):
        op2 = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        reg = op2 & 7
        def g():
            return [self.B, self.C, self.D, self.E, self.H, self.L, self.rd8(self.HL), self.A][reg]
        def s(v):
            if reg == 6: self.wr8(self.HL, v)
            elif reg == 0: self.B = v
            elif reg == 1: self.C = v
            elif reg == 2: self.D = v
            elif reg == 3: self.E = v
            elif reg == 4: self.H = v
            elif reg == 5: self.L = v
            elif reg == 7: self.A = v
        grp = op2 >> 6
        bit = (op2 >> 3) & 7
        v = g()
        if grp == 0:  # rotations/shifts
            if bit == 0:  # RLC
                v = ((v << 1) | (v >> 7)) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | (1 if v & 1 else 0)
                s(v)
            elif bit == 1:  # RRC
                v = ((v >> 1) | (v << 7)) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | (1 if v & 0x80 else 0)
                s(v)
            elif bit == 2:  # RL
                c = self.F & 1
                v = ((v << 1) | c) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | (1 if v & 0x80 else 0)
                s(v)
            elif bit == 3:  # RR
                c = (self.F & 1) << 7
                v = ((v >> 1) | c) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | (1 if v & 1 else 0)
                s(v)
            elif bit == 4:  # SLA
                carry = 1 if (v & 0x80) else 0
                v = (v << 1) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | carry
                s(v)
            elif bit == 5:  # SRA
                v = ((v >> 1) | (v & 0x80)) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | (1 if v & 1 else 0)
                s(v)
            elif bit == 6:  # SLL (undocumented)
                carry = 1 if (v & 0x80) else 0
                v = ((v << 1) | 1) & 0xFF
                self.F = self._s(v) | self._z(v) | self._p(v) | carry
                s(v)
            else:  # SRL
                carry = 1 if (v & 1) else 0
                v = v >> 1
                self.F = self._s(v) | self._z(v) | self._p(v) | carry
                s(v)
        elif grp == 1:  # BIT
            self.F = (self.F & 0x01) | 0x10 | (0x02 if reg == 6 else 0) | (0x80 if not (v & (1 << bit)) else 0) | (0x40 if not (v & (1 << bit)) else 0)
        elif grp == 2:  # RES
            s(v & ~(1 << bit))
        else:  # SET
            s(v | (1 << bit))

    def ed_op(self):
        op2 = self.rd8(self.PC); self.PC = (self.PC + 1) & 0xFFFF
        if op2 == 0x40: self.B = self.io_in(self.C); self.BC = (self.BC & 0xFF00) | self.B; self.F = (self.F & 0x01) | (0x40 if self.B == 0 else 0) | (0x80 if self.B & 0x80 else 0) | (0x04 if (bin(self.B).count('1') & 1) == 0 else 0)
        elif op2 == 0x41: self.BC = (self.BC & 0xFF00) | self.io_in(self.C)  # OUT (C),0
        elif op2 == 0x42: self.sbc16(self.BC)
        elif op2 == 0x43: self.wr16(self.rd16(self.PC), self.BC); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x44: self.NEG = True
        elif op2 == 0x4A: self.adc16(self.BC)
        elif op2 == 0x4B: self.BC = self.rd16(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x50: self.DE = (self.DE & 0xFF00) | self.io_in(self.C); self.D = self.DE >> 8; self.F = (self.F & 0x01) | (0x40 if self.DE == 0 else 0) | (0x80 if self.DE & 0x8000 else 0) | (0x04 if (bin(self.DE).count('1') & 1) == 0 else 0)
        elif op2 == 0x51: self.io_out(self.C, self.D)
        elif op2 == 0x52: self.sbc16(self.DE)
        elif op2 == 0x53: self.wr16(self.rd16(self.PC), self.DE); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x5A: self.adc16(self.DE)
        elif op2 == 0x5B: self.DE = self.rd16(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x60: self.HL = (self.HL & 0xFF00) | self.io_in(self.C); self.L = self.HL & 0xFF; self.H = self.HL >> 8; self.F = (self.F & 0x01) | (0x40 if self.HL == 0 else 0) | (0x80 if self.HL & 0x8000 else 0) | (0x04 if (bin(self.HL).count('1') & 1) == 0 else 0)
        elif op2 == 0x61: self.io_out(self.C, self.H)
        elif op2 == 0x62: self.sbc16(self.HL)
        elif op2 == 0x63: self.wr16(self.rd16(self.PC), self.HL); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x67: self.RLD = True
        elif op2 == 0x6A: self.adc16(self.HL)
        elif op2 == 0x6B: self.HL = self.rd16(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x6F: self.RRD = True
        elif op2 == 0x72: self.sbc16(self.SP)
        elif op2 == 0x73: self.wr16(self.rd16(self.PC), self.SP); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0x78: self.io_in(self.C); self.BC = (self.BC & 0xFF00) | self.A
        elif op2 == 0x79: self.io_out(self.C, self.A)
        elif op2 == 0x7A: self.adc16(self.SP)
        elif op2 == 0x7B: self.SP = self.rd16(self.rd16(self.PC)); self.PC = (self.PC + 2) & 0xFFFF
        elif op2 == 0xA0: self.ldi()
        elif op2 == 0xA1: self.cpi()
        elif op2 == 0xA2: self.ini()
        elif op2 == 0xA3: self.outi()
        elif op2 == 0xA8: self.ldd()
        elif op2 == 0xA9: self.cpd()
        elif op2 == 0xAA: self.ind()
        elif op2 == 0xAB: self.outd()
        elif op2 == 0xB0: self.ldir()
        elif op2 == 0xB1: self.cpir()
        elif op2 == 0xB2: self.inir()
        elif op2 == 0xB3: self.otir()
        elif op2 == 0xB8: self.lddr()
        elif op2 == 0xB9: self.cpdr()
        elif op2 == 0xBA: self.indr()
        elif op2 == 0xBB: self.otdr()
        elif op2 == 0x44: pass  # NEG
        else:
            raise Exception("unimplemented ED op %02X at PC=%04X" % (op2, self.PC - 1))

    def sbc16(self, hl):
        c = self.F & 1
        r = hl - c
        self.F = (0x01 if r < 0 else 0) | 0x02 | (0x10 if ((hl & 0xFFF) - c) < 0 else 0)
        if hl == 0x1234: pass
        self.F |= self._z(r & 0xFFFF) | (0x80 if r & 0x8000 else 0) | self._p(r & 0xFFFF)
        if hl == 0x1234: pass
        return r & 0xFFFF

    def adc16(self, hl):
        c = self.F & 1
        r = hl + c
        self.F = (0x01 if r & 0x10000 else 0) | (0x10 if ((hl & 0xFFF) + c) > 0xFFF else 0) | self._z(r & 0xFFFF) | (0x80 if r & 0x8000 else 0) | self._p(r & 0xFFFF)
        return r & 0xFFFF

    def ldi(self):
        v = self.rd8(self.HL); self.wr8(self.DE, v)
        self.HL = (self.HL + 1) & 0xFFFF; self.DE = (self.DE + 1) & 0xFFFF; self.BC = (self.BC - 1) & 0xFFFF
        self.F = (self.F & 0xC1) | (0x08 if (v + self.A) & 0x02 else 0)
    def ldir(self):
        while self.BC:
            self.ldi()
            if not self.BC: break
    def ldd(self):
        v = self.rd8(self.HL); self.wr8(self.DE, v)
        self.HL = (self.HL - 1) & 0xFFFF; self.DE = (self.DE - 1) & 0xFFFF; self.BC = (self.BC - 1) & 0xFFFF
        self.F = (self.F & 0xC1) | (0x08 if (v + self.A) & 0x02 else 0)
    def lddr(self):
        while self.BC:
            self.ldd()
            if not self.BC: break
    def cpi(self):
        v = self.rd8(self.HL)
        self.HL = (self.HL + 1) & 0xFFFF; self.BC = (self.BC - 1) & 0xFFFF
        r = self.A - v
        self.F = (self.F & 0x01) | self._z(r) | (0x10 if ((self.A & 0xF) - (v & 0xF)) < 0 else 0) | 0x02
        self.F |= 0x08 if self.BC else 0
    def cpir(self):
        self.cpi()
        while self.BC and not (self.F & 0x40):
            self.cpi()
    def cpd(self):
        v = self.rd8(self.HL)
        self.HL = (self.HL - 1) & 0xFFFF; self.BC = (self.BC - 1) & 0xFFFF
        r = self.A - v
        self.F = (self.F & 0x01) | self._z(r) | (0x10 if ((self.A & 0xF) - (v & 0xF)) < 0 else 0) | 0x02
        self.F |= 0x08 if self.BC else 0
    def cpdr(self):
        self.cpd()
        while self.BC and not (self.F & 0x40):
            self.cpd()
    def ini(self):
        v = self.io_in(self.C); self.wr8(self.HL, v)
        self.HL = (self.HL + 1) & 0xFFFF; self.B = (self.B - 1) & 0xFF; self.BC = (self.B << 8) | (self.BC & 0xFF)
        self.F = (0x80 if self.B & 0x80 else 0) | (0x40 if self.B == 0 else 0) | self._p(v) | (0x10 if v & 0x80 else 0) | (0x01 if self.B else 0)
    def inir(self):
        self.ini()
        while self.B:
            self.ini()
    def ind(self):
        v = self.io_in(self.C); self.wr8(self.HL, v)
        self.HL = (self.HL - 1) & 0xFFFF; self.B = (self.B - 1) & 0xFF; self.BC = (self.B << 8) | (self.BC & 0xFF)
        self.F = (0x80 if self.B & 0x80 else 0) | (0x40 if self.B == 0 else 0) | self._p(v) | (0x10 if v & 0x80 else 0) | (0x01 if self.B else 0)
    def indr(self):
        self.ind()
        while self.B:
            self.ind()
    def outi(self):
        v = self.rd8(self.HL)
        self.io_out(self.C, v)
        self.HL = (self.HL + 1) & 0xFFFF; self.B = (self.B - 1) & 0xFF; self.BC = (self.B << 8) | (self.BC & 0xFF)
        self.F = (0x80 if self.B & 0x80 else 0) | (0x40 if self.B == 0 else 0) | (0x10 if v & 0x80 else 0) | (0x01 if self.B else 0)
    def otir(self):
        self.outi()
        while self.B:
            self.outi()
    def outd(self):
        v = self.rd8(self.HL)
        self.io_out(self.C, v)
        self.HL = (self.HL - 1) & 0xFFFF; self.B = (self.B - 1) & 0xFF; self.BC = (self.B << 8) | (self.BC & 0xFF)
        self.F = (0x80 if self.B & 0x80 else 0) | (0x40 if self.B == 0 else 0) | (0x10 if v & 0x80 else 0) | (0x01 if self.B else 0)
    def otdr(self):
        self.outd()
        while self.B:
            self.outd()

    # register property shims for clarity
    @property
    def B(self): return (self.BC >> 8) & 0xFF
    @B.setter
    def B(self, v): self.BC = (self.BC & 0xFF) | ((v & 0xFF) << 8)
    @property
    def C(self): return self.BC & 0xFF
    @C.setter
    def C(self, v): self.BC = (self.BC & 0xFF00) | (v & 0xFF)
    @property
    def D(self): return (self.DE >> 8) & 0xFF
    @D.setter
    def D(self, v): self.DE = (self.DE & 0xFF) | ((v & 0xFF) << 8)
    @property
    def E(self): return self.DE & 0xFF
    @E.setter
    def E(self, v): self.DE = (self.DE & 0xFF00) | (v & 0xFF)
    @property
    def H(self): return (self.HL >> 8) & 0xFF
    @H.setter
    def H(self, v): self.HL = (self.HL & 0xFF) | ((v & 0xFF) << 8)
    @property
    def L(self): return self.HL & 0xFF
    @L.setter
    def L(self, v): self.HL = (self.HL & 0xFF00) | (v & 0xFF)

# ---------------------------------------------------------------------------
# Mock z80neo firmware ports (mirrors memory.c io_read_port/io_write_port and
# cpm_disk.c, including the A0-stuck-HIGH aliasing)
# ---------------------------------------------------------------------------
class Firmware:
    def __init__(self, img_path):
        import os
        self.serial_status = 0x02   # TX ready
        self.rx_data_available = False
        self.rx_buffer = []
        self.tx_log = []
        self.mmu_page = [0x20, 0x21, 0x22, 0x23]
        self.sectors_per_track = 26
        self.cur = 0
        d = os.path.dirname(img_path)
        paths = [os.path.join(d, "CPMDISK%d.IMG" % n) for n in range(4)]
        self.drives = []
        for p in paths:
            st = {"ready": False, "img": b"", "size": 0, "track": 0, "sector": 0,
                  "cmd": 0, "idx": 0, "error": 0, "buf": bytearray(128)}
            try:
                st["img"] = open(p, "rb").read()
                st["size"] = len(st["img"])
                st["ready"] = st["size"] > 0
            except OSError:
                pass
            self.drives.append(st)
        print("drives: " + ", ".join(
            "%s=%s" % (chr(65 + i), "ok" if s["ready"] else "-")
            for i, s in enumerate(self.drives)))

    def _drv(self):
        return self.drives[self.cur]

    def port_alias(self, p):
        # No A0 quirk: the Pico sees the port exactly as the Z80 drives it
        return p

    def seek_sector(self, track, sector):
        st = self._drv()
        off = (track * self.sectors_per_track + sector) * 128
        if off >= st["size"]:
            st["error"] = 1
            return False
        st["buf"] = bytearray(st["img"][off:off+128])
        if len(st["buf"]) < 128:
            st["buf"].extend(b'\x00' * (128 - len(st["buf"])))
        st["error"] = 0
        return True

    def io_in(self, port):
        p = self.port_alias(port)
        if p == 0x80:
            ch = self.rx_buffer.pop(0) if self.rx_buffer else 0
            if not self.rx_buffer:
                self.rx_data_available = False
                self.serial_status &= ~0x01
            return ch
        if p == 0xE0:
            return self.cpm_read_data()
        if p == 0xE2:
            return self.cpm_read_status()
        if p == 0x81:
            if self.rx_data_available or self.rx_buffer:
                self.serial_status |= 0x01
            else:
                self.serial_status &= ~0x01
            return self.serial_status
        if p == 0xF0: return self.mmu_page[0]
        if p == 0xF1: return self.mmu_page[1]
        if p == 0xF2: return self.mmu_page[2]
        if p == 0xF3: return self.mmu_page[3]
        return 0x00

    def io_out(self, port, data):
        p = self.port_alias(port)
        if p == 0x80:
            self.tx_log.append(data)
            self.serial_status |= 0x02
            return
        if p == 0xE3:               # drive select
            if data < len(self.drives):
                self.cur = data
            return
        if p == 0xE1:
            self._drv()["track"] = data
            return
        if p == 0xE2:
            self._drv()["sector"] = data
            return
        if p == 0xE0:
            self.cpm_cmd_write(data)
            return
        if p == 0xF0: self.mmu_page[0] = data
        elif p == 0xF1: self.mmu_page[1] = data
        elif p == 0xF2: self.mmu_page[2] = data
        elif p == 0xF3: self.mmu_page[3] = data

    def cpm_cmd_write(self, data):
        st = self._drv()
        if not st["ready"]:
            return
        if st["cmd"] == 0:             # CMD_IDLE
            if data == 0x00:           # CMD_READ
                st["cmd"] = 0x10
                st["idx"] = 0
                self.seek_sector(st["track"], st["sector"])
            elif data == 0x01:         # CMD_WRITE
                st["cmd"] = 0x11
                st["idx"] = 0
                st["error"] = 0
        elif st["cmd"] == 0x11:        # CMD_WRITE
            if st["idx"] < 128:
                st["buf"][st["idx"]] = data
                st["idx"] += 1
            if st["idx"] >= 128:
                st["error"] = 0
                st["cmd"] = 0

    def cpm_read_data(self):
        st = self._drv()
        if not st["ready"]:
            return 0xFF
        if st["cmd"] == 0x10 and st["idx"] < 128:
            b = st["buf"][st["idx"]]
            st["idx"] += 1
            if st["idx"] >= 128:
                st["cmd"] = 0
            return b
        return 0

    def cpm_read_status(self):
        st = self._drv()
        if not st["ready"]:
            return 0xFF
        return st["error"]


def main():
    repo = sys.argv[1] if len(sys.argv) > 1 else '.'
    hex_path = os.path.join(repo, 'software/cpm/z80neo/build/cpmz80neo.hex')
    img_path = sys.argv[5] if len(sys.argv) > 5 else os.path.join(repo, 'software/cpm/z80neo/CPMDISK0.IMG')

    mem = bytearray(64 * 1024)
    load_hex(hex_path, mem)

    fw = Firmware(img_path)
    cpu = Z80(mem, fw.io_in, fw.io_out)

    # reset state: Z80 starts at 0x0000, SP undefined
    cpu.PC = 0x0000
    cpu.SP = 0xEE9C  # BIOS will set it anyway

    out = []
    max_steps = 20000000
    trace = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] not in ('x', 'no', '-') else None
    if trace:
        trace = int(trace)
    warmboots = []
    # optional: inject keystrokes once the CCP prompt appears ("a>")
    inject = sys.argv[4] if len(sys.argv) > 4 else None
    injected = False
    out_total = []
    for step in range(max_steps):
        pc = cpu.PC
        op = mem[pc]
        # trace console output
        if fw.tx_log:
            out.append(bytes(fw.tx_log).decode('latin1'))
            fw.tx_log.clear()
            out_total.extend(out)
        if inject and not injected:
            joined = ''.join(out_total)
            if 'a>' in joined:
                for ch in inject:
                    fw.rx_buffer.append(13 if ch == '<' else ord(ch))
                fw.rx_data_available = True
                fw.serial_status |= 0x01
                injected = True
                print("+++ injected: %r" % inject)
        if trace and step < trace:
            print("%06d PC=%04X op=%02X A=%02X F=%02X BC=%04X DE=%04X HL=%04X SP=%04X mem3=%02X%02X%02X" %
                  (step, pc, op, cpu.A, cpu.F, cpu.BC, cpu.DE, cpu.HL, cpu.SP,
                   mem[pc], mem[(pc+1)&0xFFFF], mem[(pc+2)&0xFFFF]))
        cpu.step()
        if trace and step < trace and mem[pc] in (0xC3, 0xEB, 0xCD, 0xC9, 0xE9):
            print("        -> PC=%04X BC=%04X DE=%04X HL=%04X SP=%04X" % (cpu.PC, cpu.BC, cpu.DE, cpu.HL, cpu.SP))
        # watch for reboots: PC re-enters the BIOS boot entry or page zero
        if cpu.PC in (0xEB33, 0xEB75, 0xEB00) or cpu.PC < 0x0100:
            warmboots.append((step, pc))
            if len(warmboots) <= 15:
                print("*** re-enter boot: from PC=%04X to PC=%04X (step %d) A=%02X F=%02X BC=%04X DE=%04X HL=%04X SP=%04X" %
                      (pc, cpu.PC, step, cpu.A, cpu.F, cpu.BC, cpu.DE, cpu.HL, cpu.SP))
        # detect: BIOS signon then CCP prompt → success; HALT instruction → stop
        if cpu.halted:
            print("CPU HALTED at PC=%04X after %d steps" % (pc, step))
            break
        # trap common failure: repeated warm boot (JP 0) loop → count
        if step > 200000 and cpu.PC == 0x0000:
            pass
    else:
        print("TIMEOUT after %d steps, PC=%04X SP=%04X" % (max_steps, cpu.PC, cpu.SP))

    print("---- console output ----")
    sys.stdout.write(''.join(out))
    print("---- end ----")
    print("final PC=%04X  SP=%04X  A=%02X BC=%04X DE=%04X HL=%04X" %
          (cpu.PC, cpu.SP, cpu.A, cpu.BC, cpu.DE, cpu.HL))
    st = fw._drv()
    print("disk: drive=%s track=%d sector=%d cmd=%d idx=%d err=%d" %
          (chr(65 + fw.cur), st["track"], st["sector"], st["cmd"], st["idx"], st["error"]))


if __name__ == '__main__':
    main()

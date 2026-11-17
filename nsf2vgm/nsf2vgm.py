#!/usr/bin/env python3
"""
nsf2vgm - convert one track of an NSF (NES Sound Format) file to a VGM file, so that
vgm_nes2psg can convert it to PSG channels.

The NSF's 6502 code is emulated: init is called once for the chosen track, then play is
called at the NSF's play rate. Every write to the 2A03 APU ($4000-$4017) is written to the
VGM as an NES APU command, with waits between play calls. DMC sample memory ($C000-$FFFF)
is written as a VGM data block (again whenever bankswitching changes it).

usage: python nsf2vgm.py [-track <n>] [-len <seconds>] [-q] <file.nsf> [<out.vgm>]
  -track <n>     track to play, 1-based (default: the NSF's starting track)
  -len <s>       seconds to play (default 120) - NSFs have no length, they loop forever
  -q             quiet
The output defaults to <file.nsf>_<track>.vgm

Only the internal 2A03 APU is converted; expansion audio (VRC6, VRC7, FDS, MMC5, N163,
Sunsoft 5B) is reported and ignored. PAL-only NSFs are played at their PAL rate.
"""
import struct
import sys

VGM_RATE = 44100
NES_NTSC_CLOCK = 1789773
STOP = 0x4FFF            # return address used to detect the end of init/play
MAX_INSTR = 2000000      # safety limit per init/play call

class NSF:
    def __init__(self, data):
        if data[:5] != b'NESM\x1a':
            raise SystemExit('Not an NSF file')
        self.version = data[5]
        self.songs = data[6]
        self.start = data[7]
        self.load, self.init, self.play = struct.unpack_from('<HHH', data, 8)
        self.name = data[0x0E:0x2E].split(b'\0')[0].decode('latin1')
        self.artist = data[0x2E:0x4E].split(b'\0')[0].decode('latin1')
        self.copyright = data[0x4E:0x6E].split(b'\0')[0].decode('latin1')
        self.ntsc_speed = struct.unpack_from('<H', data, 0x6E)[0]
        self.banks = list(data[0x70:0x78])
        self.pal_speed = struct.unpack_from('<H', data, 0x78)[0]
        self.region = data[0x7A]
        self.expansion = data[0x7B]
        self.data = data[0x80:]
        self.banked = any(self.banks)

class CPU:
    """Plain 6502 (2A03: no decimal mode). Memory is a flat 64K bytearray; APU writes and
    bankswitch writes are intercepted."""
    def __init__(self, nsf, on_apu):
        self.m = bytearray(0x10000)
        self.nsf = nsf
        self.on_apu = on_apu
        self.a = self.x = self.y = 0
        self.sp = 0xFD
        self.p = 0x24
        self.pc = 0
        self.dmc_dirty = True
        if nsf.banked:
            pad = nsf.load & 0x0FFF
            self.image = bytes(pad) + nsf.data
            for i, b in enumerate(nsf.banks):
                self.bank(i, b)
        else:
            end = min(0x10000, nsf.load + len(nsf.data))
            self.m[nsf.load:end] = nsf.data[:end - nsf.load]

    def bank(self, slot, num):
        start = num * 0x1000
        chunk = self.image[start:start + 0x1000]
        chunk = chunk + bytes(0x1000 - len(chunk))
        base = 0x8000 + slot * 0x1000
        self.m[base:base + 0x1000] = chunk
        if base >= 0xC000:
            self.dmc_dirty = True

    def rd(self, a):
        if a < 0x2000:
            return self.m[a & 0x7FF]
        if a == 0x4015:
            return 0
        return self.m[a]

    def wr(self, a, v):
        if a < 0x2000:
            self.m[a & 0x7FF] = v
        elif 0x4000 <= a <= 0x4017:
            self.on_apu(a, v)
        elif 0x5FF8 <= a <= 0x5FFF:
            if self.nsf.banked:
                self.bank(a - 0x5FF8, v)
        elif 0x6000 <= a < 0x8000:
            self.m[a] = v
        # ROM and everything else: ignored

    def push(self, v):
        self.m[0x100 | self.sp] = v
        self.sp = (self.sp - 1) & 0xFF

    def pull(self):
        self.sp = (self.sp + 1) & 0xFF
        return self.m[0x100 | self.sp]

    def call(self, addr, a=0, x=0):
        """JSR to addr with a sentinel return address, run until it returns"""
        self.a, self.x, self.y = a, x, 0
        ret = STOP - 1
        self.push(ret >> 8)
        self.push(ret & 0xFF)
        self.pc = addr
        n = run(self, MAX_INSTR)
        if n >= MAX_INSTR:
            print('Warning: routine at $%04X did not return within %d instructions' % (addr, MAX_INSTR))
        return n

# opcode table: opcode -> (mnemonic, mode)
MODES = {}
def _ops(name, pairs):
    for op, mode in pairs:
        MODES[op] = (name, mode)
_ops('ADC', [(0x69,'imm'),(0x65,'zp'),(0x75,'zpx'),(0x6D,'abs'),(0x7D,'absx'),(0x79,'absy'),(0x61,'indx'),(0x71,'indy')])
_ops('AND', [(0x29,'imm'),(0x25,'zp'),(0x35,'zpx'),(0x2D,'abs'),(0x3D,'absx'),(0x39,'absy'),(0x21,'indx'),(0x31,'indy')])
_ops('ASL', [(0x0A,'acc'),(0x06,'zp'),(0x16,'zpx'),(0x0E,'abs'),(0x1E,'absx')])
_ops('BIT', [(0x24,'zp'),(0x2C,'abs')])
_ops('BPL', [(0x10,'rel')]); _ops('BMI', [(0x30,'rel')]); _ops('BVC', [(0x50,'rel')]); _ops('BVS', [(0x70,'rel')])
_ops('BCC', [(0x90,'rel')]); _ops('BCS', [(0xB0,'rel')]); _ops('BNE', [(0xD0,'rel')]); _ops('BEQ', [(0xF0,'rel')])
_ops('BRK', [(0x00,'imp')])
_ops('CMP', [(0xC9,'imm'),(0xC5,'zp'),(0xD5,'zpx'),(0xCD,'abs'),(0xDD,'absx'),(0xD9,'absy'),(0xC1,'indx'),(0xD1,'indy')])
_ops('CPX', [(0xE0,'imm'),(0xE4,'zp'),(0xEC,'abs')])
_ops('CPY', [(0xC0,'imm'),(0xC4,'zp'),(0xCC,'abs')])
_ops('DEC', [(0xC6,'zp'),(0xD6,'zpx'),(0xCE,'abs'),(0xDE,'absx')])
_ops('EOR', [(0x49,'imm'),(0x45,'zp'),(0x55,'zpx'),(0x4D,'abs'),(0x5D,'absx'),(0x59,'absy'),(0x41,'indx'),(0x51,'indy')])
_ops('CLC', [(0x18,'imp')]); _ops('SEC', [(0x38,'imp')]); _ops('CLI', [(0x58,'imp')]); _ops('SEI', [(0x78,'imp')])
_ops('CLV', [(0xB8,'imp')]); _ops('CLD', [(0xD8,'imp')]); _ops('SED', [(0xF8,'imp')])
_ops('INC', [(0xE6,'zp'),(0xF6,'zpx'),(0xEE,'abs'),(0xFE,'absx')])
_ops('JMP', [(0x4C,'abs'),(0x6C,'ind')])
_ops('JSR', [(0x20,'abs')])
_ops('LDA', [(0xA9,'imm'),(0xA5,'zp'),(0xB5,'zpx'),(0xAD,'abs'),(0xBD,'absx'),(0xB9,'absy'),(0xA1,'indx'),(0xB1,'indy')])
_ops('LDX', [(0xA2,'imm'),(0xA6,'zp'),(0xB6,'zpy'),(0xAE,'abs'),(0xBE,'absy')])
_ops('LDY', [(0xA0,'imm'),(0xA4,'zp'),(0xB4,'zpx'),(0xAC,'abs'),(0xBC,'absx')])
_ops('LSR', [(0x4A,'acc'),(0x46,'zp'),(0x56,'zpx'),(0x4E,'abs'),(0x5E,'absx')])
_ops('NOP', [(0xEA,'imp')])
_ops('ORA', [(0x09,'imm'),(0x05,'zp'),(0x15,'zpx'),(0x0D,'abs'),(0x1D,'absx'),(0x19,'absy'),(0x01,'indx'),(0x11,'indy')])
_ops('TAX', [(0xAA,'imp')]); _ops('TXA', [(0x8A,'imp')]); _ops('DEX', [(0xCA,'imp')]); _ops('INX', [(0xE8,'imp')])
_ops('TAY', [(0xA8,'imp')]); _ops('TYA', [(0x98,'imp')]); _ops('DEY', [(0x88,'imp')]); _ops('INY', [(0xC8,'imp')])
_ops('ROL', [(0x2A,'acc'),(0x26,'zp'),(0x36,'zpx'),(0x2E,'abs'),(0x3E,'absx')])
_ops('ROR', [(0x6A,'acc'),(0x66,'zp'),(0x76,'zpx'),(0x6E,'abs'),(0x7E,'absx')])
_ops('RTI', [(0x40,'imp')]); _ops('RTS', [(0x60,'imp')])
_ops('SBC', [(0xE9,'imm'),(0xE5,'zp'),(0xF5,'zpx'),(0xED,'abs'),(0xFD,'absx'),(0xF9,'absy'),(0xE1,'indx'),(0xF1,'indy')])
_ops('STA', [(0x85,'zp'),(0x95,'zpx'),(0x8D,'abs'),(0x9D,'absx'),(0x99,'absy'),(0x81,'indx'),(0x91,'indy')])
_ops('STX', [(0x86,'zp'),(0x96,'zpy'),(0x8E,'abs')])
_ops('STY', [(0x84,'zp'),(0x94,'zpx'),(0x8C,'abs')])
_ops('TXS', [(0x9A,'imp')]); _ops('TSX', [(0xBA,'imp')]); _ops('PHA', [(0x48,'imp')]); _ops('PLA', [(0x68,'imp')])
_ops('PHP', [(0x08,'imp')]); _ops('PLP', [(0x28,'imp')])
# a few common unofficial NOPs (skip their operands)
for op in (0x1A, 0x3A, 0x5A, 0x7A, 0xDA, 0xFA): MODES[op] = ('NOP', 'imp')
for op in (0x80, 0x82, 0x89, 0xC2, 0xE2): MODES[op] = ('NOP', 'imm')
for op in (0x04, 0x44, 0x64): MODES[op] = ('NOP', 'zp')
for op in (0x14, 0x34, 0x54, 0x74, 0xD4, 0xF4): MODES[op] = ('NOP', 'zpx')
MODES[0x0C] = ('NOP', 'abs')
for op in (0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC): MODES[op] = ('NOP', 'absx')
LEN = {'imp': 1, 'acc': 1, 'imm': 2, 'zp': 2, 'zpx': 2, 'zpy': 2, 'rel': 2, 'indx': 2, 'indy': 2,
       'abs': 3, 'absx': 3, 'absy': 3, 'ind': 3}

unknown_ops = set()

def run(c, limit):
    m = c.m
    rd, wr = c.rd, c.wr
    n = 0

    def nz(v):
        c.p = (c.p & 0x7D) | (v & 0x80) | (0 if v else 0x02)
        return v

    while n < limit:
        pc = c.pc
        if pc == STOP:
            return n
        op = rd(pc)
        ent = MODES.get(op)
        if ent is None:
            unknown_ops.add(op)
            c.pc = (pc + 1) & 0xFFFF
            n += 1
            continue
        name, mode = ent
        b1 = rd((pc + 1) & 0xFFFF)
        # resolve effective address
        if mode == 'imm':   ea = (pc + 1) & 0xFFFF
        elif mode == 'zp':  ea = b1
        elif mode == 'zpx': ea = (b1 + c.x) & 0xFF
        elif mode == 'zpy': ea = (b1 + c.y) & 0xFF
        elif mode in ('abs', 'absx', 'absy', 'ind'):
            ea = b1 | (rd((pc + 2) & 0xFFFF) << 8)
            if mode == 'absx': ea = (ea + c.x) & 0xFFFF
            elif mode == 'absy': ea = (ea + c.y) & 0xFFFF
            elif mode == 'ind':
                ea = rd(ea) | (rd((ea & 0xFF00) | ((ea + 1) & 0xFF)) << 8)
        elif mode == 'indx':
            z = (b1 + c.x) & 0xFF
            ea = m[z] | (m[(z + 1) & 0xFF] << 8)
        elif mode == 'indy':
            ea = ((m[b1] | (m[(b1 + 1) & 0xFF] << 8)) + c.y) & 0xFFFF
        else:
            ea = 0
        c.pc = (pc + LEN[mode]) & 0xFFFF
        n += 1
        p = c.p

        if name == 'LDA': c.a = nz(rd(ea))
        elif name == 'STA': wr(ea, c.a)
        elif name == 'LDX': c.x = nz(rd(ea))
        elif name == 'LDY': c.y = nz(rd(ea))
        elif name == 'STX': wr(ea, c.x)
        elif name == 'STY': wr(ea, c.y)
        elif name in ('BPL', 'BMI', 'BVC', 'BVS', 'BCC', 'BCS', 'BNE', 'BEQ'):
            flag = {'BPL': (0x80, 0), 'BMI': (0x80, 1), 'BVC': (0x40, 0), 'BVS': (0x40, 1),
                    'BCC': (0x01, 0), 'BCS': (0x01, 1), 'BNE': (0x02, 0), 'BEQ': (0x02, 1)}[name]
            if bool(p & flag[0]) == bool(flag[1]):
                c.pc = (c.pc + (b1 - 256 if b1 > 127 else b1)) & 0xFFFF
        elif name in ('ADC', 'SBC'):
            v = rd(ea)
            if name == 'SBC': v ^= 0xFF
            r = c.a + v + (p & 1)
            ov = (~(c.a ^ v) & (c.a ^ r) & 0x80)
            c.p = (c.p & ~0x41 & 0xFF) | (1 if r > 0xFF else 0) | (0x40 if ov else 0)
            c.a = nz(r & 0xFF)
        elif name == 'AND': c.a = nz(c.a & rd(ea))
        elif name == 'ORA': c.a = nz(c.a | rd(ea))
        elif name == 'EOR': c.a = nz(c.a ^ rd(ea))
        elif name in ('CMP', 'CPX', 'CPY'):
            reg = {'CMP': c.a, 'CPX': c.x, 'CPY': c.y}[name]
            v = rd(ea)
            c.p = (c.p & 0xFE) | (1 if reg >= v else 0)
            nz((reg - v) & 0xFF)
        elif name == 'BIT':
            v = rd(ea)
            c.p = (c.p & 0x3D) | (v & 0xC0) | (0 if (c.a & v) else 0x02)
        elif name in ('INC', 'DEC'):
            v = (rd(ea) + (1 if name == 'INC' else -1)) & 0xFF
            wr(ea, v); nz(v)
        elif name in ('ASL', 'LSR', 'ROL', 'ROR'):
            v = c.a if mode == 'acc' else rd(ea)
            carry = p & 1
            if name == 'ASL': out, v = v >> 7, (v << 1) & 0xFF
            elif name == 'LSR': out, v = v & 1, v >> 1
            elif name == 'ROL': out, v = v >> 7, ((v << 1) | carry) & 0xFF
            else: out, v = v & 1, (v >> 1) | (carry << 7)
            c.p = (c.p & 0xFE) | out
            nz(v)
            if mode == 'acc': c.a = v
            else: wr(ea, v)
        elif name == 'JMP': c.pc = ea
        elif name == 'JSR':
            ret = (c.pc - 1) & 0xFFFF
            c.push(ret >> 8); c.push(ret & 0xFF)
            c.pc = ea
        elif name == 'RTS':
            lo = c.pull(); hi = c.pull()
            c.pc = (((hi << 8) | lo) + 1) & 0xFFFF
        elif name == 'RTI':
            c.p = (c.pull() & 0xEF) | 0x20
            lo = c.pull(); hi = c.pull()
            c.pc = (hi << 8) | lo
        elif name == 'INX': c.x = nz((c.x + 1) & 0xFF)
        elif name == 'INY': c.y = nz((c.y + 1) & 0xFF)
        elif name == 'DEX': c.x = nz((c.x - 1) & 0xFF)
        elif name == 'DEY': c.y = nz((c.y - 1) & 0xFF)
        elif name == 'TAX': c.x = nz(c.a)
        elif name == 'TAY': c.y = nz(c.a)
        elif name == 'TXA': c.a = nz(c.x)
        elif name == 'TYA': c.a = nz(c.y)
        elif name == 'TSX': c.x = nz(c.sp)
        elif name == 'TXS': c.sp = c.x
        elif name == 'PHA': c.push(c.a)
        elif name == 'PLA': c.a = nz(c.pull())
        elif name == 'PHP': c.push(c.p | 0x30)
        elif name == 'PLP': c.p = (c.pull() & 0xEF) | 0x20
        elif name == 'CLC': c.p &= 0xFE
        elif name == 'SEC': c.p |= 0x01
        elif name == 'CLI': c.p &= 0xFB
        elif name == 'SEI': c.p |= 0x04
        elif name == 'CLV': c.p &= 0xBF
        elif name == 'CLD': c.p &= 0xF7
        elif name == 'SED': c.p |= 0x08
        elif name == 'BRK':
            # no IRQ handler in an NSF - treat as end of routine
            return n
        # NOP: nothing
    return n

def main():
    args = sys.argv[1:]
    track, length, quiet = None, 120.0, False
    pos = []
    while args:
        a = args.pop(0)
        if a == '-track': track = int(args.pop(0))
        elif a == '-len': length = float(args.pop(0))
        elif a == '-q': quiet = True
        else: pos.append(a)
    if not pos:
        print(__doc__); sys.exit(1)
    fn = pos[0]
    nsf = NSF(open(fn, 'rb').read())
    if track is None:
        track = nsf.start
    out = pos[1] if len(pos) > 1 else '%s_%d.vgm' % (fn, track)

    pal = (nsf.region & 3) == 1
    speed = (nsf.pal_speed if pal else nsf.ntsc_speed) or (20000 if pal else 16639)
    print('NSF: "%s" by %s (%s)' % (nsf.name, nsf.artist, nsf.copyright))
    print('%d tracks, playing track %d of %d, load $%04X init $%04X play $%04X, %s %.2fHz%s' % (
        nsf.songs, track, nsf.songs, nsf.load, nsf.init, nsf.play, 'PAL' if pal else 'NTSC',
        1000000.0 / speed, ', bankswitched' if nsf.banked else ''))
    if nsf.expansion:
        names = ['VRC6', 'VRC7', 'FDS', 'MMC5', 'N163', 'Sunsoft 5B']
        used = [n for i, n in enumerate(names) if nsf.expansion & (1 << i)]
        print('Warning: expansion audio (%s) is not converted - only the 2A03 APU' % ', '.join(used))
    if not 1 <= track <= nsf.songs:
        raise SystemExit('Track must be 1-%d' % nsf.songs)

    cmds = bytearray()
    def on_apu(addr, val):
        cmds.extend((0xB4, addr - 0x4000, val))
    cpu = CPU(nsf, on_apu)

    def dmc_block():
        # DMC samples are fetched from $C000-$FFFF
        data = bytes(cpu.m[0xC000:0x10000])
        cmds.extend(b'\x67\x66\xC2')
        cmds.extend(struct.pack('<IH', len(data) + 2, 0xC000))
        cmds.extend(data)
        cpu.dmc_dirty = False

    # standard NSF init sequence: clear RAM, reset the APU, init the track
    for a in range(0x4000, 0x4014):
        cpu.wr(a, 0)
    cpu.wr(0x4015, 0x0F)
    cpu.wr(0x4017, 0x40)
    cpu.call(nsf.init, track - 1, 1 if pal else 0)

    frames = int(length * 1000000 / speed)
    samples_per_call = VGM_RATE * speed / 1000000.0
    acc = 0.0
    total = 0
    for f in range(frames):
        if cpu.dmc_dirty:
            dmc_block()
        cpu.call(nsf.play)
        acc += samples_per_call
        wait = int(acc)
        acc -= wait
        total += wait
        if wait == 735:
            cmds.append(0x62)
        elif wait == 882:
            cmds.append(0x63)
        else:
            while wait > 0:
                w = min(wait, 0xFFFF)
                cmds.extend(struct.pack('<BH', 0x61, w))
                wait -= w
    cmds.append(0x66)

    # VGM 1.61 header (0x100 bytes), NES APU clock at 0x84, data at 0x100
    hdr = bytearray(0x100)
    hdr[0:4] = b'Vgm '
    struct.pack_into('<I', hdr, 0x04, 0x100 + len(cmds) - 4)
    struct.pack_into('<I', hdr, 0x08, 0x161)
    struct.pack_into('<I', hdr, 0x18, total)
    struct.pack_into('<I', hdr, 0x24, 50 if pal else 60)
    struct.pack_into('<I', hdr, 0x34, 0x100 - 0x34)
    struct.pack_into('<I', hdr, 0x84, NES_NTSC_CLOCK if not pal else 1662607)
    with open(out, 'wb') as f:
        f.write(hdr + cmds)
    if unknown_ops:
        print('Warning: unsupported opcodes executed as 1-byte NOPs: %s' % ' '.join('%02X' % o for o in sorted(unknown_ops)))
    print('Wrote %s - %d play calls, %.1f seconds' % (out, frames, total / VGM_RATE))

if __name__ == '__main__':
    main()

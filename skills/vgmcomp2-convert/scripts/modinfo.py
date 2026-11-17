# modinfo.py - reconnaissance for a 4-channel ProTracker MOD (M.K. and friends)
# usage: python modinfo.py <file.mod> [-positions] [-pattern <n> [<firstrow> <lastrow>]]
# Prints samples (length, volume, loop), the order list, which instruments play on which
# channel, period ranges and effect usage. -positions adds a per-order-position table with
# the start row of each position (60hz rows, following speed/tempo changes) and the
# instruments each channel uses there. -pattern dumps the notes of one pattern.
import sys, collections

NOTE = ['C-', 'C#', 'D-', 'D#', 'E-', 'F-', 'F#', 'G-', 'G#', 'A-', 'A#', 'B-']
PER = [856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453, 428, 404, 381, 360, 339,
       320, 302, 285, 269, 254, 240, 226, 214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113]

def nm(p):
    if not p: return '...'
    return NOTE[PER.index(p) % 12] + str(PER.index(p) // 12 + 1) if p in PER else '%3d' % p

def cell(d, pat, row, ch):
    b = d[1084 + pat * 1024 + row * 16 + ch * 4:][:4]
    return (b[0] & 0xf0) | (b[2] >> 4), ((b[0] & 0xf) << 8) | b[1], b[2] & 0xf, b[3]

def position_starts(d, order):
    """start frame (60hz row) of each order position, following Fxx speed/tempo and breaks"""
    speed, tempo, frame, starts = 6, 125, 0.0, []
    for p in order:
        starts.append(round(frame))
        for r in range(64):
            brk = False
            for c in range(4):
                ins, per, e, ep = cell(d, p, r, c)
                if e == 0xf:
                    if 0 < ep < 32: speed = ep
                    elif ep >= 32: tempo = ep
                if e in (0xb, 0xd): brk = True
            frame += speed * 2.5 / tempo * 60
            if brk: break
    starts.append(round(frame))
    return starts

def main():
    a = sys.argv[1:]
    d = open(a[0], 'rb').read()
    print('title', d[:20].split(b'\0')[0].decode('latin1'), ' tag', d[1080:1084])
    for i in range(31):
        o = 20 + i * 30
        ln = int.from_bytes(d[o + 22:o + 24], 'big') * 2
        if ln:
            print('%2d %-22s len %6d vol %2d fine %d loop %d+%d' % (
                i + 1, d[o:o + 22].split(b'\0')[0].decode('latin1'), ln, d[o + 25], d[o + 24] & 0xf,
                int.from_bytes(d[o + 26:o + 28], 'big') * 2, int.from_bytes(d[o + 28:o + 30], 'big') * 2))
    order = list(d[952:952 + d[950]])
    print('order (%d):' % len(order), order)
    use = [collections.Counter() for _ in range(4)]
    pers = collections.defaultdict(set)
    fx = collections.Counter()
    last = [0] * 4
    for p in order:
        for r in range(64):
            for c in range(4):
                ins, per, e, ep = cell(d, p, r, c)
                if ins: last[c] = ins; use[c][ins] += 1
                if per and last[c]: pers[last[c]].add(per)
                if e or ep: fx['%X' % e + ('%02X' % ep if e == 0xf else 'xx')] += 1
    for c in range(4):
        print('channel %d instruments (uses):' % c, dict(sorted(use[c].items())))
    for i in sorted(pers):
        print('ins %2d periods %d-%d (%d distinct)' % (i, min(pers[i]), max(pers[i]), len(pers[i])))
    print('effects:', fx.most_common(20))
    if '-positions' in a:
        st = position_starts(d, order)
        for pos, p in enumerate(order):
            u = [collections.Counter() for _ in range(4)]
            for r in range(64):
                for c in range(4):
                    ins = cell(d, p, r, c)[0]
                    if ins: u[c][ins] += 1
            print('pos %2d pat %2d rows %5d-%5d  ' % (pos, p, st[pos], st[pos + 1]) +
                  ' | '.join(' '.join('%dx%d' % kv for kv in sorted(x.items())) or '-' for x in u))
    if '-pattern' in a:
        i = a.index('-pattern'); p = int(a[i + 1])
        r0, r1 = (int(a[i + 2]), int(a[i + 3])) if len(a) > i + 3 else (0, 63)
        for r in range(r0, r1 + 1):
            print('%2d  ' % r + ' | '.join('%s %2d %X%02X' % (nm(cell(d, p, r, c)[1]), cell(d, p, r, c)[0],
                                          cell(d, p, r, c)[2], cell(d, p, r, c)[3]) for c in range(4)))

if __name__ == '__main__':
    main()

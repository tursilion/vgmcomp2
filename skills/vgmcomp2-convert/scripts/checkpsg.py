# checkpsg.py - quality checks on a prepared file (output of prepare4SN / prepare4AY / prepare4SID)
# usage: python checkpsg.py [-chip sn|ay|sid] [-section <rows>] <prepared file>
# Prepared files have 4 channels per row: tone,vol,tone,vol,tone,vol,noise,vol (hex).
# Volume: SN = attenuation (0 loudest, 15 silent); AY and SID = level (15 loudest, 0 silent).
# Reports, per channel:
#   1-row gaps / blips      a single silent row inside sound, or a single sounding row alone
#   alternating runs        on/off/on/off flicker - sounds jittery, fix with smoothvolume etc.
#   gap-neighbor levels     loud neighbours = audible dropout; quiet ones are just tails
# Per tone pair: exact unisons (same counter, both sounding) - same-frequency channels on these
# chips interfere; and near unisons (1-2 counts apart, a slow beat / chorus).
# Then average level (linear 0-255 estimate) while sounding, per section of -section rows
# (default 900 = 15s), to spot quiet/loud parts and balance problems.
import sys

SN_LIN = [254, 202, 160, 128, 100, 80, 64, 50, 40, 32, 24, 20, 16, 12, 10, 0]

def main():
    a = sys.argv[1:]
    chip, sec = 'sn', 900
    if '-chip' in a: i = a.index('-chip'); chip = a[i + 1].lower(); del a[i:i + 2]
    if '-section' in a: i = a.index('-section'); sec = int(a[i + 1]); del a[i:i + 2]
    rows = [[int(x, 0) for x in l.strip().split(',')] for l in open(a[0]) if l.strip()]
    if chip == 'sn':
        on = lambda v: v < 15
        lin = lambda v: SN_LIN[v]
    else:
        on = lambda v: v > 0
        lin = lambda v: SN_LIN[15 - v] if chip == 'ay' else v * 17
    n = len(rows)
    print('rows %d (%.1f s)' % (n, n / 60))
    for c in range(4):
        s = [on(r[c * 2 + 1]) for r in rows]
        gaps = [i for i in range(1, n - 1) if not s[i] and s[i - 1] and s[i + 1]]
        blips = [i for i in range(1, n - 1) if s[i] and not s[i - 1] and not s[i + 1]]
        alt, i = [], 0
        while i < n - 4:
            if all(s[i + k] != s[i + k + 1] for k in range(4)): alt.append(i); i += 4
            else: i += 1
        nb = sorted(set(rows[i - 1][c * 2 + 1] for i in gaps))
        print('ch%d gaps %d blips %d alternating-runs %d  gap-neighbor vols %s  first at %s' % (
            c, len(gaps), len(blips), len(alt), nb, (gaps + blips + alt)[:6]))
    for x, y in ((0, 1), (0, 2), (1, 2)):
        both = [(r[x * 2], r[y * 2]) for r in rows if on(r[x * 2 + 1]) and on(r[y * 2 + 1])]
        uni = sum(1 for p, q in both if p == q)
        near = sum(1 for p, q in both if 0 < abs(p - q) <= 2)
        print('ch%d/ch%d exact unison %d rows, near (1-2 counts) %d rows' % (x, y, uni, near))
    print('level while sounding (linear estimate), per %d rows:  ch0 ch1 ch2 noise' % sec)
    for st in range(0, n, sec):
        blk = rows[st:st + sec]
        lv = []
        for c in range(4):
            v = [lin(r[c * 2 + 1]) for r in blk if on(r[c * 2 + 1])]
            lv.append(round(sum(v) / len(v)) if v else 0)
        print('  row %6d (%5.0fs) %s' % (st, st / 60, ' '.join('%4d' % x for x in lv)))

if __name__ == '__main__':
    main()

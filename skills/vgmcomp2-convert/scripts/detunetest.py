# detunetest.py - choose a scalepitch factor that keeps one channel out of unison with others
# usage: python detunetest.py <channel to detune> <other channel> [<other channel> ...]
# Two chip channels on the same counter interfere (they can cancel or phase badly). When the
# unisons are passing coincidences (arpeggio steps, stabs, passing notes) deleting notes with
# cleardupes punches holes in the line - detuning by a few cents avoids the unison instead.
# This tries factors near 1.0 and reports exact unisons and 1-count near misses (a slow beat)
# against each other channel. Pick the smallest detune with zero exact unisons, then apply it
# with: scalepitch <factor> <channel>. (0.996/1.004 ~ 7 cents, 0.992/1.008 ~ 14 cents.)
# If the channel is also being octave-shifted, fold it into one factor (e.g. 0.5 * 1.004 = 0.502)
# and test with -pre 0.5.
import sys
a = sys.argv[1:]
pre = 1.0
if '-pre' in a: i = a.index('-pre'); pre = float(a[i + 1]); del a[i:i + 2]
def load(f): return [tuple(int(x, 0) for x in l.split(',')) for l in open(f)]
def fold(c):
    while c > 0x3ff: c = (c + 1) // 2
    return c
target = load(a[0]); others = [load(f) for f in a[1:]]
others = [[(fold(c & 0xffff), v) for c, v in o] for o in others]
print('factor  ' + '  '.join('exact/near vs %s' % f.split('_')[-1] for f in a[1:]))
for s in (1.0, 1.004, 0.996, 1.006, 0.994, 1.008, 0.992, 1.01, 0.99):
    t = [(fold(int((c & 0xffff) * pre * s + 0.5)) if v else c, v) for c, v in target]
    res = []
    for o in others:
        ex = sum(1 for p, q in zip(t, o) if p[1] and q[1] and p[0] == q[0])
        nr = sum(1 for p, q in zip(t, o) if p[1] and q[1] and abs(p[0] - q[0]) == 1)
        res.append('%5d/%-5d' % (ex, nr))
    print('%.4f  %s' % (pre * s, '            '.join(res)))

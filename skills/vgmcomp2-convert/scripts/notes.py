# notes.py - decode a channel into note names, to check a melody against the source
# usage: python notes.py <file> [-col <channel 0-3>] [-rows <first> <last>] [-chip sn|ay|sid]
#   <file> is either a 60hz channel file, or a prepared file (use -col to pick the channel)
# Prints the sequence of notes (a new entry each time the sounding note changes), plus the
# note histogram with the cents offset of each counter - useful to confirm tuning factors and
# that the melody survived merging (compare against the MIDI/MOD notes or another section).
import sys, math, collections

N = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
def nm(c):
    m = 69 + 12 * math.log2(111860.8 / c / 440); r = round(m)
    return '%s%d' % (N[r % 12], r // 12 - 1), round((m - r) * 100)

a = sys.argv[1:]
col, r0, r1, chip = None, 0, None, 'sn'
if '-col' in a: i = a.index('-col'); col = int(a[i + 1]); del a[i:i + 2]
if '-rows' in a: i = a.index('-rows'); r0, r1 = int(a[i + 1]), int(a[i + 2]); del a[i:i + 3]
if '-chip' in a: i = a.index('-chip'); chip = a[i + 1]; del a[i:i + 2]
rows = [[int(x, 0) for x in l.strip().split(',')] for l in open(a[0]) if l.strip()]
if col is None:
    data = [(r[0] & 0xffff, r[1] > 0) for r in rows]
else:
    on = (lambda v: v < 15) if (chip == 'sn') else (lambda v: v > 0)
    data = [(r[col * 2] & 0xffff, on(r[col * 2 + 1])) for r in rows]
data = data[r0:r1]
seq, prev, hist = [], None, collections.Counter()
for c, s in data:
    k = c if (s and c) else None
    if k and k != prev:
        seq.append(nm(c)[0])
    if k: hist[c] += 1
    prev = k
print(' '.join(seq))
print('most used:', ', '.join('%s%+dc x%d' % (nm(c)[0], nm(c)[1], n) for c, n in hist.most_common(12)))

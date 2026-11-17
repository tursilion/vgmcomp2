# scalesection.py - scale the volume of one channel over a range of rows only
# usage: python scalesection.py <scale> <first row> <last row> [-ramp <rows>] <channel file>
# The scale ramps in and out over -ramp rows (default 30, about half a second) so the change
# isn't a sudden jump at the section edges. Volumes are clipped at 255.
# Original file is renamed with .section.old extension (numbered if one already exists).
import sys, os

args = sys.argv[1:]
ramp = 30
if '-ramp' in args:
    i = args.index('-ramp'); ramp = int(args[i + 1]); del args[i:i + 2]
scale, first, last, fn = float(args[0]), int(args[1]), int(args[2]), args[3]

rows = [l.strip().split(',') for l in open(fn)]
bak = fn + '.section.old'
k = 1
while os.path.exists(bak):
    bak = '%s.section%d.old' % (fn, k); k += 1
os.replace(fn, bak)
clipped = changed = 0
with open(fn, 'w', newline='\r\n') as fp:
    for r, (f, v) in enumerate(rows):
        v = int(v, 0)
        if first <= r <= last and v:
            # ramp factor 0..1 at both edges
            edge = min(r - first, last - r)
            w = 1.0 if ramp <= 0 else min(1.0, (edge + 1) / ramp)
            nv = int(v * (1 + (scale - 1) * w) + 0.5)
            if nv > 255: nv = 255; clipped += 1
            if nv != v: changed += 1
            v = nv
        fp.write('%s,0x%02X\n' % (f, v))
print('%d volumes changed in rows %d-%d (x%.2f), %d clipped' % (changed, first, last, scale, clipped))

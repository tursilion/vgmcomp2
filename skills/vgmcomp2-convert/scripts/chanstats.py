# chanstats.py - summarize 60hz channel files and how they overlap
# usage: python chanstats.py <file.60hz> [<file.60hz> ...]      (wildcards ok)
# Per file: rows, active rows, counter range, rows below the SN floor (counter > 0x3ff), max
# volume, distinct counters, flag usage. Then for each pair: rows where both sound, exact
# unisons, and octave doublings - the numbers you need to decide what to keep, merge or cut.
import sys, glob, math

def load(f):
    return [tuple(int(x, 0) for x in l.strip().split(',')) for l in open(f)]

files = sorted(set(f for a in sys.argv[1:] for f in (glob.glob(a) or [a])))
data = {f: load(f) for f in files}
for f, t in data.items():
    act = [x for x in t if x[1]]
    fr = [x[0] & 0xffff for x in act]
    print('%-40s rows %6d active %6d counters %s-%s >0x3ff %5d maxvol %3d distinct %3d flags %s' % (
        f, len(t), len(act), min(fr) if fr else 0, max(fr) if fr else 0, sum(1 for x in fr if x > 0x3ff),
        max((x[1] for x in t), default=0), len(set(fr)), sorted(set(x[0] >> 16 for x in act))))

def semi(c): return 69 + 12 * math.log2(111860.8 / c / 440) if c else 0
tones = [f for f in files if '_ton' in f]
if len(tones) > 1:
    print('\npair                                  both-sound  unison  octave')
    for i, a in enumerate(tones):
        for b in tones[i + 1:]:
            both = uni = octv = 0
            for x, y in zip(data[a], data[b]):
                if x[1] and y[1] and x[0] & 0xffff and y[0] & 0xffff:
                    both += 1
                    d = abs(semi(x[0] & 0xffff) - semi(y[0] & 0xffff))
                    if d < 0.3: uni += 1
                    elif abs(d - 12 * round(d / 12)) < 0.3: octv += 1
            print('%-18s %-18s %8d %7d %7d' % (a.split('_')[-1], b.split('_')[-1], both, uni, octv))

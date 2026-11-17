# instlevels.py - average level of each MOD instrument in mod2psg's output (per instrument)
# usage: python instlevels.py <file.mod> <prefix> [-rows <first> <last>]
#   <prefix> is the mod2psg output prefix (e.g. SONG.MOD) - reads <prefix>_tonNN / _noiNN.60hz
# mod2psg writes MOD channel c to _ton0c and its noise instruments to _noi0(4+c) for a 4 channel
# MOD (check the -Writing lines). This walks the MOD to know which instrument is playing on each
# channel at each row and averages the output volume per instrument. Use it to set -volins so
# instruments that read too quiet (rippled/off-centre samples fool mod2psg's peak detector) come
# up to a common level, and to find which instrument is buried in a given section (-rows).
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from modinfo import cell

def main():
    a = sys.argv[1:]
    r0, r1 = 0, None
    if '-rows' in a: i = a.index('-rows'); r0, r1 = int(a[i + 1]), int(a[i + 2]); del a[i:i + 3]
    mod, pre = a[0], a[1]
    d = open(mod, 'rb').read()
    order = list(d[952:952 + d[950]])
    def load(f):
        return [tuple(int(x, 0) for x in l.split(',')) for l in open(f)] if os.path.exists(f) else None
    tone = {c: load('%s_ton%02d.60hz' % (pre, c)) for c in range(4)}
    noise = {c: load('%s_noi%02d.60hz' % (pre, c + 4)) for c in range(4)}
    # event list: frame -> (channel, instrument)
    speed, tempo, frame, ev = 6, 125, 0.0, []
    for p in order:
        for r in range(64):
            brk = False
            for c in range(4):
                ins, per, e, ep = cell(d, p, r, c)
                if ins: ev.append((int(frame), c, ins))
                if e == 0xf:
                    if 0 < ep < 32: speed = ep
                    elif ep >= 32: tempo = ep
                if e in (0xb, 0xd): brk = True
            frame += speed * 2.5 / tempo * 60
            if brk: break
    n = max(len(t) for t in list(tone.values()) + list(noise.values()) if t)
    r1 = n if r1 is None else r1
    lev = collections.defaultdict(list); last = [0] * 4; j = 0
    for r in range(n):
        while j < len(ev) and ev[j][0] <= r:
            last[ev[j][1]] = ev[j][2]; j += 1
        if not r0 <= r < r1: continue
        for c in range(4):
            for kind, src in (('tone', tone[c]), ('noise', noise[c])):
                if src and src[r][1]:
                    lev[(last[c], kind)].append(src[r][1])
    print('ins  kind   avg  p90  max   rows')
    for (i, kind), v in sorted(lev.items()):
        v.sort()
        print('%3d  %-5s %4d %4d %4d %6d' % (i, kind, sum(v) / len(v), v[int(len(v) * .9)], v[-1], len(v)))

if __name__ == '__main__':
    main()

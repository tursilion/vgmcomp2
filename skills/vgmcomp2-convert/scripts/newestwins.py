# newestwins.py - merge two channels into one, where the most recently started note wins
# usage: python newestwins.py <chanA> <chanB>      (result written to chanA)
# MODs often alternate one melody between two channels so each note rings into the next.
# A fixed priority merge (underlaytones) keeps only one channel's notes and drops every other
# melody note; this keeps whichever channel struck a note most recently instead.
# A note start is a frequency change, or the volume rising from silence or jumping up sharply.
# Original chanA is renamed with .newest.old extension.
import sys, os

def load(fn):
    return [tuple(int(x, 0) for x in l.strip().split(',')) for l in open(fn)]

def onsets(t):
    out = [False] * len(t)
    for r in range(len(t)):
        f, v = t[r]
        if not v:
            continue
        pf, pv = t[r - 1] if r else (0, 0)
        if pv == 0 or f != pf or v > pv * 1.5 + 8:
            out[r] = True
    return out

fa, fb = sys.argv[1], sys.argv[2]
a, b = load(fa), load(fb)
n = min(len(a), len(b))
oa, ob = onsets(a), onsets(b)
last_a = last_b = -1
out = []
fromb = 0
for r in range(n):
    if oa[r]: last_a = r
    if ob[r]: last_b = r
    va, vb = a[r][1], b[r][1]
    if va and vb:
        use_b = last_b > last_a
    else:
        use_b = bool(vb)
    out.append(b[r] if use_b else a[r])
    fromb += use_b
os.replace(fa, fa + '.newest.old')
with open(fa, 'w', newline='\r\n') as fp:
    for f, v in out:
        fp.write('0x%08X,0x%02X\n' % (f, v))
print('%d rows taken from %s' % (fromb, fb))

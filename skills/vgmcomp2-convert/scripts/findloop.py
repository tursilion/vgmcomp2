# findloop.py - find where a song repeats, for sources with no length (SID, NSF, some VGMs)
# usage: python findloop.py <file.60hz> [<file.60hz> ...] [-min <rows>]
# Render the song much longer than you expect (e.g. 240s), convert, then point this at the
# tone channel files. It finds the shortest repeat period where the channels match their copy
# one period later (allowing small sampling jitter - a 50Hz player sampled into 60hz rows never
# repeats exactly row for row), then the loop start. One full pass = loop start + period rows:
# cut every channel to that with truncrows.py so the song ends at the loop point.
import sys
import numpy as np

a = sys.argv[1:]
mn = 600
if '-min' in a: i = a.index('-min'); mn = int(a[i + 1]); del a[i:i + 2]
X = []
for f in a:
    t = [tuple(int(x, 0) for x in l.split(',')) for l in open(f)]
    X.append(np.array([c & 0xffff if v else 0 for c, v in t]))
n = min(len(x) for x in X)
X = [x[:n] for x in X]
W = min(2400, n // 4); s0 = min(600, n // 8)
best = []
for P in range(mn, n - W - s0):
    sc = np.mean([np.mean(x[s0:s0 + W] == x[s0 + P:s0 + P + W]) for x in X])
    best.append((sc, P))
best.sort(reverse=True)
top = best[0][0]
# prefer the shortest period that scores nearly as well as the best (multiples score too)
cands = sorted(P for sc, P in best[:50] if sc >= top - 0.02)
P = cands[0]
print('candidate periods (rows, score):', [(p, round(s, 3)) for s, p in best[:6]])
print('loop period %d rows (%.2fs), match %.3f' % (P, P / 60, dict((p, s) for s, p in best)[P]))
for st in range(0, P):
    if st + P + 240 > n: break
    if all(np.mean(x[st:st + 240] == x[st + P:st + P + 240]) > 0.95 for x in X):
        print('loop starts at row %d -> one full pass is %d rows (%.2fs)' % (st, st + P, (st + P) / 60))
        break
else:
    print('could not pin the loop start - render longer or check the channels')

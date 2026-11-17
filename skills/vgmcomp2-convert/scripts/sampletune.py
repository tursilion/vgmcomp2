# sampletune.py - measure the real pitch of each MOD sample and suggest mod2psg -tuneins values
# usage: python sampletune.py <file.mod> [-floor <Hz>] [-ins 1,4,8]
# mod2psg maps MOD periods straight to tone counters, which assumes every sample sounds about
# 259Hz (C4) at period 428 (C-2). Real samples are often octaves off - or not tuned to C at all.
# This measures each sample's fundamental at period 428 (YIN pitch detector + spectral peaks),
# prints the note and cents offset, and for the periods each instrument actually plays:
#   tuneins_true  - factor that makes it sound at its true pitch
#   octaves up    - whole octaves needed so the lowest note clears -floor (default 109.4Hz =
#                   SN counter 0x3ff; use ~27 for AY, ~2 for SID)
#   tuneins_fit   - tuneins_true shifted by those octaves
# Chord samples show several strong peaks - pick the chord root by ear/peaks. Unpitched samples
# (drums, noise) show low confidence (conf > ~0.3) and should be forced to noise instead.
# Bass instruments often sound better at true pitch plus foldoctave on the few low notes,
# rather than shifting the whole line up; judge by the note histogram (see SKILL.md).
import sys, math, collections
import numpy as np

ASSUMED = 111860.8 / 432          # what mod2psg plays for period 428 with tuneins 1.0
N = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

def nm(f, cents=True):
    m = 69 + 12 * math.log2(f / 440); r = round(m)
    return '%s%d%s' % (N[r % 12], r // 12 - 1, ('%+dc' % round((m - r) * 100)) if cents else '')

def yin(x, rate, fmin=25, fmax=2500, th=0.12):
    W = min(len(x) // 2, 2048)
    tmax = min(int(rate / fmin), len(x) - W - 1); tmin = int(rate / fmax)
    if tmax <= tmin + 2: return None, 1.0
    a = x[:W]
    dd = np.array([np.dot(a - x[t:t + W], a - x[t:t + W]) for t in range(tmax)])
    cm = np.concatenate([[1], dd[1:] * np.arange(1, tmax) / np.maximum(np.cumsum(dd[1:]), 1e-9)])
    for t in range(tmin, tmax - 1):
        if cm[t] < th:
            while t + 1 < tmax and cm[t + 1] < cm[t]: t += 1
            p, q, r = cm[t - 1], cm[t], cm[t + 1]; den = p - 2 * q + r
            return rate / (t + (0.5 * (p - r) / den if den else 0)), cm[t]
    t = int(np.argmin(cm[tmin:])) + tmin
    return rate / t, cm[t]

def main():
    a = sys.argv[1:]
    floor = 109.4
    only = None
    if '-floor' in a: i = a.index('-floor'); floor = float(a[i + 1]); del a[i:i + 2]
    if '-ins' in a: i = a.index('-ins'); only = set(map(int, a[i + 1].split(','))); del a[i:i + 2]
    d = open(a[0], 'rb').read()
    order = list(d[952:952 + d[950]]); npat = max(order) + 1
    off = 1084 + npat * 1024; S = {}
    for i in range(31):
        o = 20 + i * 30; ln = int.from_bytes(d[o + 22:o + 24], 'big') * 2
        S[i + 1] = np.array([b - 256 if b > 127 else b for b in d[off:off + ln]], float); off += ln
    pers = collections.defaultdict(set); last = [0] * 4
    for p in order:
        for r in range(64):
            for c in range(4):
                b = d[1084 + p * 1024 + r * 16 + c * 4:][:4]
                ins = (b[0] & 0xf0) | (b[2] >> 4); per = ((b[0] & 0xf) << 8) | b[1]
                if ins: last[c] = ins
                if per and last[c]: pers[last[c]].add(per)
    rate = 3546895 / 428
    for i in range(1, 32):
        x = S[i]
        if len(x) < 512 or (only and i not in only) or i not in pers: continue
        seg = x[len(x) // 6:]; seg = seg - seg.mean()
        f, q = yin(seg, rate)
        s = x[min(1000, len(x) // 4):][:16384]; s = s - s.mean()
        m = np.abs(np.fft.rfft(s * np.hanning(len(s)))); F = np.fft.rfftfreq(len(s), 1 / rate)
        pk = [k for k in range(2, len(m) - 1) if m[k] > m[k - 1] and m[k] >= m[k + 1] and 30 < F[k] < 3000]
        pk = sorted(sorted(pk, key=lambda k: -m[k])[:5]); mx = max(m[k] for k in pk) if pk else 1
        peaks = ' '.join('%s(%.2f)' % (nm(F[k], False), m[k] / mx) for k in pk)
        if f is None:
            print('%2d too short to measure' % i); continue
        lo, hi = min(pers[i]), max(pers[i])
        fmin, fmax = f * 428 / hi, f * 428 / lo
        k = 0
        while fmin * 2 ** k < floor: k += 1
        T = ASSUMED / f
        tag = '  (low confidence - unpitched?)' if q > 0.3 else ''
        print('%2d %s at C-2 (%.1fHz, conf %.2f)%s  peaks %s' % (i, nm(f), f, q, tag, peaks))
        if pk:
            # the pitch detector can lock onto a subharmonic - compare with the strongest peak
            top = F[max(pk, key=lambda k: m[k])]
            ratio = top / f
            if ratio > 1.5 and abs(ratio - round(ratio)) < 0.08:
                print('    NOTE: strongest peak %s is %dx the detected pitch - if that peak is the real '
                      'fundamental, divide the tuneins values below by %d' % (nm(top), round(ratio), round(ratio)))
        print('    plays %s-%s  tuneins_true %.4f  octaves up to clear floor %d -> tuneins_fit %.4f' % (
            nm(fmin, False), nm(fmax, False), T, k, T / 2 ** k))

if __name__ == '__main__':
    main()

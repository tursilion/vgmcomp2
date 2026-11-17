# voice2psg_proto.py - prototype of improved voice2psg analysis (Python + numpy)
# Same framing and output format as voice2psg.exe so results can be A/B compared:
#   44.1kHz, 1470-sample (2 frame) Hann window, hop 735 (1/60s), 2 frames of leading silence,
#   writes <wav>_tonNN.60hz (and _noiNN.60hz with -noise).
#
# usage: python voice2psg_proto.py [options] <file.wav>
#   -channels <n>   number of tone channels (default 3)
#   -mode peaks|lpc peaks: spectral peak picking (default)
#                   lpc:   formant tracking with linear prediction (sine-wave-speech style)
#   -emph <c>       pre-emphasis coefficient for peak picking (default 0.7, 0 = off)
#   -nosort         keep loudest-first channel order (like the original) instead of by frequency
#   -nosmooth       disable the 3-frame median filter on each channel's frequency track
#   -noise          output unvoiced consonants (s, sh, f, t...) as white noise on a noise channel
#   -noisegain <f>  noise volume multiplier (default 3.0)
#   -nosnap         lpc mode: don't snap formants to the nearest voice harmonic
#   -relpow <f>     channel volume = (level / loudest channel) ^ relpow, measured on the
#                   pre-emphasized spectrum (default 0.5; 1.0 = linear like the original)
#   -maxfreq <n>    highest frequency considered (default 4000)
#   -spacing <f>    minimum ratio between two picked peaks (default 1.12, ~2 semitones)
#   -add <n>        add n to output channel numbers
#   -q              quiet
import sys, os, wave
import numpy as np

RATE = 44100
HOP = 735
WIN = HOP * 2
SN_CLOCK = 111860.8

def readwav(fn):
    w = wave.open(fn, 'rb')
    ch, sw, fr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    readwav.nyquist = fr / 2
    raw = w.readframes(n)
    if sw == 1:
        x = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128) / 128.0
    elif sw == 2:
        x = np.frombuffer(raw, '<i2').astype(np.float64) / 32768.0
    else:
        raise SystemExit('only 8 or 16 bit wave files supported')
    if ch == 2:
        x = x.reshape(-1, 2).mean(axis=1)
    # linear resample to 44.1kHz, as voice2psg does
    t = np.arange(int(len(x) / fr * RATE)) * (fr / RATE)
    x = np.interp(t, np.arange(len(x)), x)
    # DC blocker (~35Hz high-pass): many 8-bit samples sit off-centre, which makes silence
    # read as loud and drags the spectral measurements toward 0Hz
    y = np.empty_like(x); prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = v - prev_x + 0.995 * prev_y; prev_x = v; y[i] = prev_y
    x = y
    x = np.concatenate([np.zeros(int(round(2 / 60 * RATE))), x])
    pk = np.max(np.abs(x))
    return x / pk if pk > 0 else x

def lpc(frame, order):
    # autocorrelation method + Levinson-Durbin
    r = np.correlate(frame, frame, 'full')[len(frame) - 1:len(frame) + order]
    if r[0] <= 0:
        return None
    a = np.zeros(order + 1); a[0] = 1.0; e = r[0]
    for i in range(1, order + 1):
        k = -(r[i] + np.dot(a[1:i], r[i - 1:0:-1])) / e
        a[1:i + 1] = a[1:i + 1] + k * np.concatenate([a[i - 1:0:-1], [1.0]])
        e *= (1 - k * k)
        if e <= 0:
            return None
    return a

def formants(frame, fs, maxfreq):
    # all LPC resonance candidates as (freq, bandwidth)
    order = 2 + fs // 1000
    a = lpc(frame * np.hamming(len(frame)), order)
    if a is None:
        return []
    cand = []
    for z in np.roots(a):
        if np.imag(z) <= 0:
            continue
        f = np.angle(z) * fs / (2 * np.pi)
        bw = -fs / np.pi * np.log(np.abs(z))
        if 90 < f < maxfreq and bw < 1200:
            cand.append((f, bw))
    return sorted(cand)

# nominal formant bands (F1, F2, F3, F4) used by the tracker
FBANDS = [(200, 1000), (700, 2600), (1800, 3800), (2800, 4500)]

def track(cand, prev, n):
    # pick one candidate per formant band: cost = broadness + jump from the previous frame.
    # A broad F1 that stays put beats a sharp F3 masquerading as F2 - that swap is what made
    # sustained vowels sound like random notes. Missing bands hold their previous frequency.
    out = []
    for c in range(n):
        lo, hi = FBANDS[c]
        floor = out[-1] * 1.15 if out else 0
        opts = [(f, bw) for f, bw in cand if lo <= f <= hi and f > floor]
        if opts:
            def cost(o):
                k = o[1] / 400.0
                # formants are the NEXT resonance up - penalize skipping sharp ones
                k += sum(1.0 for f, bw in cand if floor < f < o[0] and bw < 500)
                if prev and prev[c]:
                    k += 2.0 * abs(np.log(o[0] / prev[c]))
                return k
            out.append(min(opts, key=cost)[0])
        elif prev and prev[c] and prev[c] > floor:
            out.append(prev[c])
        else:
            break
    return out

def main():
    args = sys.argv[1:]
    opt = dict(channels=3, mode='peaks', emph=0.7, sort=True, smooth=True, noise=False,
               maxfreq=4000, spacing=1.12, add=0, quiet=False,
               noisegain=3.0, snap=True, relpow=0.5)
    while args and args[0].startswith('-'):
        a = args.pop(0)
        if a == '-channels': opt['channels'] = int(args.pop(0))
        elif a == '-mode': opt['mode'] = args.pop(0)
        elif a == '-emph': opt['emph'] = float(args.pop(0))
        elif a == '-nosort': opt['sort'] = False
        elif a == '-nosmooth': opt['smooth'] = False
        elif a == '-noise': opt['noise'] = True
        elif a == '-noisegain': opt['noisegain'] = float(args.pop(0))
        elif a == '-nosnap': opt['snap'] = False
        elif a == '-relpow': opt['relpow'] = float(args.pop(0))
        elif a == '-maxfreq': opt['maxfreq'] = int(args.pop(0))
        elif a == '-spacing': opt['spacing'] = float(args.pop(0))
        elif a == '-add': opt['add'] = int(args.pop(0))
        elif a == '-q': opt['quiet'] = True
        else: raise SystemExit('unknown option ' + a)
    if not args:
        raise SystemExit(__doc__ if __doc__ else 'usage: voice2psg_proto.py [options] file.wav')
    fn = args[0]
    N = opt['channels']
    x = readwav(fn)
    srcnyq = readwav.nyquist
    # brightness thresholds scale with the source's bandwidth (an 8kHz sample has no 's' above
    # 4kHz, but its 's' is still the brightest thing in it). These match the fixed values that
    # were tuned on 14kHz samples: hf split 2500Hz, bright 2500Hz, mid 1500Hz.
    hfsplit = 2500
    bright, midb = min(2500, 0.36 * srcnyq), min(1500, 0.21 * srcnyq)
    emph = np.concatenate([[x[0]], x[1:] - opt['emph'] * x[:-1]])
    win = np.hanning(WIN)
    freqs = np.fft.rfftfreq(WIN, 1 / RATE)
    lo = np.searchsorted(freqs, 110); hi = np.searchsorted(freqs, opt['maxfreq'])
    # for LPC: 10kHz copy of the pre-emphasized signal (simple FFT-free decimation by
    # averaging is fine here since LPC only looks below 4-5kHz)
    if opt['mode'] == 'lpc':
        lpcfs = 10000
        tt = np.arange(int(len(x) / RATE * lpcfs)) * (RATE / lpcfs)
        # crude anti-alias: moving average over the decimation span before interpolating
        span = int(RATE / lpcfs)
        sm = np.convolve(x, np.ones(span) / span, 'same')
        x10 = np.interp(tt, np.arange(len(x)), sm)
        x10 = np.concatenate([[x10[0]], x10[1:] - 0.95 * x10[:-1]])

    rows = []
    prevf = []      # previous frame's formant frequencies (lpc mode continuity)
    prevsnap = []   # previous frame's snapped harmonic bins
    pos = 0
    while pos + WIN < len(x):
        fr = x[pos:pos + WIN]
        avgvol = np.mean(np.abs(fr))
        rms = np.sqrt(np.mean(fr * fr))
        mag = np.abs(np.fft.rfft(fr * win))           # true spectrum (for volumes)
        emag = np.abs(np.fft.rfft(emph[pos:pos + WIN] * win))   # tilted spectrum (for picking)
        pw = mag[lo:np.searchsorted(freqs, 8000)] ** 2 + 1e-20
        flat = np.exp(np.mean(np.log(pw))) / np.mean(pw)
        hf = mag[np.searchsorted(freqs, hfsplit):] ** 2
        hfratio = hf.sum() / ((mag[lo:] ** 2).sum() + 1e-20)
        cent = (freqs * mag ** 2).sum() / ((mag ** 2).sum() + 1e-20)
        zcr = np.sum(np.abs(np.diff(np.sign(fr)))) / 2 / WIN * RATE
        # fricative detection: strong HF energy = unvoiced (s, sh, f, t); moderate HF with a
        # high zero-crossing rate = fricative mixed with voicing (keep the tones too)
        unvoiced = hfratio > 0.6
        mixed = (not unvoiced) and hfratio > 0.2 and zcr > 3000
        # noise rate by brightness - thresholds suit band-limited sources (MOD/8kHz-14kHz samples)
        nrate = 16 if cent > bright else (32 if cent > midb else 64)

        tones = []      # list of (freq, relative amplitude)
        noise = None
        if rms < 0.01:
            pass
        elif opt['noise'] and unvoiced:
            # unvoiced consonant - white noise, plus the strongest high peaks (>1.5kHz) as quiet
            # tones; the high tones are what made the original's bursts (the 'j' in jack) audible
            noise = (nrate, min(255, int(avgvol * 256 * opt['noisegain'])))
            h0 = np.searchsorted(freqs, 1500)
            s_ = mag[h0:hi]
            pk = np.where((s_[1:-1] > s_[:-2]) & (s_[1:-1] >= s_[2:]))[0] + 1
            for p in pk[np.argsort(-s_[pk])]:
                f = freqs[h0 + p]
                if all(max(f, c) / min(f, c) >= opt['spacing'] for c, _ in tones):
                    tones.append((f, s_[p]))
                if len(tones) >= N: break
            if tones:
                ref = max(a for _, a in tones)
                tones = sorted((f, 0.5 * (a / ref) ** opt['relpow']) for f, a in tones)
        elif flat >= 0.90 and not opt['noise']:
            pass    # original behaviour: flat frames produce nothing
        else:
            if opt['mode'] == 'lpc':
                seg = x10[int(pos / RATE * lpcfs):int((pos + WIN) / RATE * lpcfs)]
                fl = track(formants(seg, lpcfs, opt['maxfreq']), prevf, N)
                snaps = [0] * N
                prevf = fl + [0] * (N - len(fl)) if fl else []
                taken = set()
                for ci, f in enumerate(fl):
                    if opt['snap']:
                        # snap to the loudest voice harmonic within +/-150Hz so the tones stay
                        # harmonics of the speaker's pitch (keeps the voice 'voiced', not whistly)
                        b0, b1 = np.searchsorted(freqs, f - 150), np.searchsorted(freqs, f + 150)
                        cand = [b for b in range(max(b0, 1), min(b1, len(mag) - 1))
                                if mag[b] > mag[b - 1] and mag[b] >= mag[b + 1] and b not in taken]
                        if cand:
                            b = max(cand, key=lambda b: mag[b])
                            # hysteresis: stay on the previous harmonic unless another is 2x louder
                            pb = prevsnap[ci] if ci < len(prevsnap) else 0
                            near = [c for c in cand if abs(c - pb) <= 2]
                            if pb and near and mag[near[0]] * 2 >= mag[b]:
                                b = near[0]
                            taken.add(b)
                            snaps[ci] = b
                            y0, y1, y2 = np.log(mag[b - 1] + 1e-20), np.log(mag[b] + 1e-20), np.log(mag[b + 1] + 1e-20)
                            den = y0 - 2 * y1 + y2
                            f = freqs[b] + (0.5 * (y0 - y2) / den if den else 0) * (RATE / WIN)
                    b = int(round(f / (RATE / WIN)))
                    tones.append((f, emag[max(b - 2, 0):b + 3].max()))
                prevsnap = snaps
            else:
                # local maxima of the tilted spectrum, loudest first, with minimum spacing
                s = emag[lo:hi]
                pk = np.where((s[1:-1] > s[:-2]) & (s[1:-1] >= s[2:]))[0] + 1
                pk = pk[np.argsort(-s[pk])]
                chosen = []
                for p in pk:
                    # parabolic interpolation for the true peak frequency
                    y0, y1, y2 = np.log(s[p - 1] + 1e-20), np.log(s[p] + 1e-20), np.log(s[p + 1] + 1e-20)
                    d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2) if (y0 - 2 * y1 + y2) != 0 else 0
                    f = freqs[lo + p] + d * (RATE / WIN)
                    if all(max(f, c) / min(f, c) >= opt['spacing'] for c, _ in chosen):
                        chosen.append((f, emag[lo + p]))
                    if len(chosen) >= N:
                        break
                tones = chosen
            if opt['noise'] and mixed:
                noise = (nrate,
                         min(255, int(avgvol * 256 * opt['noisegain'] * hfratio * 2)))
            if tones:
                ref = max(a for _, a in tones)
                tones = [(f, (a / ref) ** opt['relpow']) for f, a in tones]
                if opt['sort']:
                    tones.sort()
        rows.append((tones, noise, avgvol))
        pos += HOP

    # build per-channel tracks
    nrows = len(rows)
    tf = np.zeros((N, nrows)); tv = np.zeros((N, nrows), int)
    for r, (tones, noise, avgvol) in enumerate(rows):
        for c, (f, rel) in enumerate(tones):
            f = min(max(f, 110), opt['maxfreq'])
            tf[c, r] = f
            tv[c, r] = min(255, int(rel * avgvol * 256))
    if opt['smooth']:
        # 3-frame median on log frequency where all three frames are voiced
        for c in range(N):
            f = tf[c].copy()
            for r in range(1, nrows - 1):
                if tv[c, r - 1] and tv[c, r] and tv[c, r + 1]:
                    tf[c, r] = np.exp(np.median(np.log(f[r - 1:r + 2])))
    nf = np.zeros(nrows, int); nv = np.zeros(nrows, int)
    for r, (tones, noise, avgvol) in enumerate(rows):
        if noise:
            nf[r], nv[r] = noise

    # remove old outputs, write new ones
    if opt['add'] == 0:
        for i in range(100):
            for t in ('ton', 'noi'):
                p = '%s_%s%02d.60hz' % (fn, t, i)
                if os.path.exists(p):
                    os.remove(p)
    ch = opt['add']
    for c in range(N):
        if not tv[c].any():
            continue
        with open('%s_ton%02d.60hz' % (fn, ch), 'w', newline='\r\n') as fp:
            for r in range(nrows):
                cnt = int(SN_CLOCK / tf[c, r] + 0.5) if tv[c, r] else 0
                fp.write('0x%08X,0x%02X\n' % (cnt, tv[c, r] if cnt else 0))
        if not opt['quiet']: print('-Writing channel %d as %s_ton%02d.60hz' % (ch, fn, ch))
        ch += 1
    if nv.any():
        with open('%s_noi%02d.60hz' % (fn, ch), 'w', newline='\r\n') as fp:
            for r in range(nrows):
                fp.write('0x%08X,0x%02X\n' % (nf[r], nv[r]))
        if not opt['quiet']: print('-Writing channel %d as %s_noi%02d.60hz' % (ch, fn, ch))
    if not opt['quiet']: print('Wrote %d rows' % nrows)

if __name__ == '__main__':
    main()

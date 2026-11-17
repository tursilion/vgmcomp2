# drums2noise_proto.py - prototype: convert a drum loop / break sample to an SN noise channel
# Each 1/60s frame is classified by where its energy is:
#   kick  (mostly < 200Hz)  -> noise rate 64 (lowest)
#   hat   (mostly > 5kHz)   -> noise rate 16 (highest)
#   other (snare/clap)      -> noise rate 32
# Volume follows the frame RMS, and the retrigger flag is set on onsets or drum changes.
# Framing matches voice2psg (44.1kHz, 735 samples per row, 2 rows of leading silence) so the
# output can be spliced the same way. Writes <wav>_noi00.60hz.
#
# usage: python drums2noise_proto.py [-gain <f>] [-periodickick] <file.wav>
#   -gain <f>        volume scale (default 400: full-scale RMS of ~0.64 hits 255)
#   -periodickick    use periodic noise for kicks (a deeper, more tonal thump)
import sys
import numpy as np
sys.path.insert(0, __import__('os').path.dirname(__import__('os').path.abspath(__file__)))
from voice2psg_proto import readwav, RATE, HOP

args = sys.argv[1:]
gain, periodic = 400.0, False
while args and args[0].startswith('-'):
    a = args.pop(0)
    if a == '-gain': gain = float(args.pop(0))
    elif a == '-periodickick': periodic = True
    else: raise SystemExit('unknown option ' + a)
if not args:
    raise SystemExit('usage: drums2noise_proto.py [-gain <f>] [-periodickick] <file.wav>')
fn = args[0]
x = readwav(fn)
F = np.fft.rfftfreq(HOP, 1 / RATE)
w = np.hanning(HOP)
def band(m, a, b): return (m[np.searchsorted(F, a):np.searchsorted(F, b)] ** 2).sum()

out = []
prev_rms, prev_kind = 0.0, None
pos = 0
while pos + HOP <= len(x):
    fr = x[pos:pos + HOP]
    m = np.abs(np.fft.rfft(fr * w))
    tot = band(m, 20, RATE / 2) + 1e-12
    rms = np.sqrt(np.mean(fr * fr))
    vol = min(255, int(rms * gain))
    if vol < 2:
        out.append((0, 0)); prev_kind = None
    else:
        low, hi5 = band(m, 20, 200) / tot, band(m, 5000, RATE / 2) / tot
        if low > 0.6:   kind, rate = 'k', 64
        elif hi5 > 0.6: kind, rate = 'h', 16
        else:           kind, rate = 's', 32
        flags = 0
        if kind == 'k' and periodic: flags |= 0x00100000
        if rms > 1.8 * prev_rms + 0.02 or kind != prev_kind: flags |= 0x00010000
        out.append((flags | rate, vol))
        prev_kind = kind
    prev_rms = rms
    pos += HOP

with open(fn + '_noi00.60hz', 'w', newline='\r\n') as fp:
    for f, v in out:
        fp.write('0x%08X,0x%02X\n' % (f, v))
print('-Writing %s_noi00.60hz (%d rows)' % (fn, len(out)))

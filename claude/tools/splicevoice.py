# splicevoice.py - place voice2psg renderings of MOD voice samples at their trigger points
# usage: python splicevoice.py <mod> <voicedir> <rows> <voice ins list> <peak> <outprefix>
# Reads voicedir/vNN_PER.wav_tonXX.60hz (two tone channels, from voice2psg -channels 2) and
# an optional _noiXX.60hz (from voice2psg_proto.py -noise, or noise-only from drums2noise_proto.py), walks the MOD note/volume events,
# and writes <outprefix>_ton00.60hz / _ton01.60hz (and _noi02.60hz if any phrase had noise)
# with the voice phrases placed at the correct rows. A new note on the channel cuts the voice (as in the MOD).
# Each phrase is normalized to 'peak' (tones and noise together) then scaled by the MOD
# channel volume (0-64).
# If two voices overlap, the louder one wins (ties go to the newest trigger).
import sys, os, glob

def walk(path):
    d = open(path, 'rb').read()
    order = list(d[952:952+d[950]])
    vols = [d[20+i*30+25] for i in range(31)]
    speed, tempo, frame, out = 6, 125, 0.0, []
    for p in order:
        for r in range(64):
            brk, cells = False, []
            for c in range(4):
                b = d[1084+p*1024+r*16+c*4:][:4]
                ins = (b[0]&0xf0)|(b[2]>>4); per = ((b[0]&0xf)<<8)|b[1]; e = b[2]&0xf; ep = b[3]
                cells.append((c, ins, per, e, ep))
                if e == 0xf:
                    if 0 < ep < 32: speed = ep
                    elif ep >= 32: tempo = ep
                if e in (0xb, 0xd): brk = True
            for cell in cells: out.append((int(frame),) + cell)
            frame += speed*2.5/tempo*60
            if brk: break
    return out, vols

def load(fn):
    return [tuple(int(x, 0) for x in l.strip().split(',')) for l in open(fn)]

mod, vdir, rows, voices, peak, outp = sys.argv[1], sys.argv[2], int(sys.argv[3]), \
    set(map(int, sys.argv[4].split(','))), float(sys.argv[5]), sys.argv[6]
ev, defvol = walk(mod)

cache = {}
def phrase(ins, per):
    k = (ins, per)
    if k not in cache:
        base = os.path.join(vdir, 'v%02d_%d.wav' % k)
        tones = [load(f) for f in sorted(glob.glob(base+'_ton??.60hz'))][:2]
        nf = sorted(glob.glob(base+'_noi??.60hz'))
        noise = load(nf[0]) if nf else None
        n = len(tones[0]) if tones else len(noise)
        while len(tones) < 2: tones.append([(0, 0)]*n)
        if noise is None: noise = [(0, 0)]*n
        # voice2psg output starts with silence - skip it so retriggers don't leave a gap
        lead = 0
        while lead < n and all(t[lead][1] == 0 for t in tones+[noise]): lead += 1
        a, b, nz = tones[0][lead:], tones[1][lead:], noise[lead:]
        # normalize tones and noise together so the analyzer's tone/noise balance is kept
        m = max(max(v for f, v in a), max(v for f, v in b), max(v for f, v in nz)) or 1
        cache[k] = (a, b, nz, peak/m)
    return cache[k]

# per MOD channel: list of (start, end, ins, per, [(frame, vol)...]) segments
segs = []
state = [None]*4      # current voice segment per channel
lastins = [0]*4
for f, c, ins, per, e, ep in ev:
    if ins: lastins[c] = ins
    if per:
        if state[c]: state[c]['end'] = f; state[c] = None
        i = lastins[c]
        if i in voices:
            state[c] = {'start': f, 'end': rows, 'ins': i, 'per': per,
                        'vols': [(f, ep if e == 0xc else defvol[i-1])]}
            segs.append(state[c])
    elif state[c]:
        if e == 0xc: state[c]['vols'].append((f, ep))
        elif ins: state[c]['vols'].append((f, defvol[ins-1]))

out = [[(0, 0)]*rows, [(0, 0)]*rows, [(0, 0)]*rows]
owner = [None]*rows
for s in segs:
    a, b, nz, norm = phrase(s['ins'], s['per'])
    vi = 0
    for r in range(s['start'], min(s['end'], rows, s['start']+len(a))):
        while vi+1 < len(s['vols']) and s['vols'][vi+1][0] <= r: vi += 1
        mv = min(s['vols'][vi][1], 64)/64.0
        k = r - s['start']
        cur = owner[r]
        if cur is not None and cur[0] > mv: continue   # louder voice already here
        owner[r] = (mv, s['start'])
        for ch, src in ((0, a), (1, b), (2, nz)):
            fr, v = src[k]
            out[ch][r] = (fr, min(255, int(v*norm*mv + 0.5)) if (fr & 0xffff) else 0)

for ch in (0, 1, 2):
    if ch == 2 and not any(v for f, v in out[2]): continue
    with open('%s_%s%02d.60hz' % (outp, 'noi' if ch == 2 else 'ton', ch), 'w', newline='\r\n') as fp:
        for fr, v in out[ch]:
            fp.write('0x%08X,0x%02X\n' % (fr, v))
print('%d voice segments placed, %d rows with voice' % (len(segs), sum(1 for o in owner if o)))

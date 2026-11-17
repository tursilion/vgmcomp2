#!/usr/bin/env python3
"""
midi2psg - convert a MIDI file to vgmcomp2 PSG channel files (60hz).

Each MIDI part (a track/channel pair) becomes one or more tone files (one per
polyphony slot), and each drum part (MIDI channel 10) becomes one noise file.

usage: python midi2psg.py [options] <input.mid> [speed_scale] [decay_scale]
  speed_scale   song speed multiplier (default 1.0, 0.5=half speed, 2.0=double speed)
  decay_scale   envelope decay rate multiplier (default 1.0, 0.5=slower, 2.0=faster)
options:
  -poly <n>       maximum notes per part (default 3)
  -partpoly <track> <n>  maximum notes for one track's parts (repeatable) - 1 makes a part
                  monophonic, with each new note cutting off the previous one
  -bendrange <n>  pitch bend range in semitones (default 2)
  -q              quiet

Original script was written by Claude.ai; reworked to:
  - use one global tempo map for every track (tempo changes live in track 0)
  - split parts by MIDI channel within each track (type 0 files work), detect drums by
    MIDI channel 10 (not by output position)
  - shape each note with an envelope from its GM instrument family (plucked/struck
    instruments decay, sustained ones hold until note off), plus a short release
  - apply channel volume (CC7), expression (CC11) and pitch bend
  - map GM drum notes to SN noise rates (16/32/64) with per-drum decay, and set the
    retrigger flag on each hit
  - keep each note in the same output slot for its whole life, so chords don't make
    notes jump between files
"""

import math
import sys
from dataclasses import dataclass, field

try:
    import mido
except ImportError:
    print("Error: 'mido' library not found. Install with: pip install mido")
    sys.exit(1)

FPS = 60
SN_CLOCK = 111860.9

GM_NAMES = ("AcPiano BrPiano ElGrand Honky EP1 EP2 Harpsi Clav Celesta Glock MusicBox Vibes "
            "Marimba Xylo TubBells Dulcimer DrawOrg PercOrg RockOrg ChurchOrg ReedOrg Accord "
            "Harmonica TangoAcc NylonGtr SteelGtr JazzGtr CleanGtr MutedGtr OverdrGtr DistGtr "
            "GtrHarm AcBass FingBass PickBass Fretless Slap1 Slap2 SynBass1 SynBass2 Violin Viola "
            "Cello Contrab TremStr PizzStr Harp Timpani Strings1 Strings2 SynStr1 SynStr2 ChoirAah "
            "VoiceOoh SynVoice OrchHit Trumpet Trombone Tuba MutedTpt FrHorn Brass SynBrass1 "
            "SynBrass2 SopSax AltoSax TenorSax BariSax Oboe EngHorn Bassoon Clarinet Piccolo Flute "
            "Recorder PanFlute Bottle Shaku Whistle Ocarina SqLead SawLead CalliopeLd ChiffLd "
            "CharangLd VoiceLd FifthsLd BassLead Pad1 Pad2 Pad3 Pad4 Pad5 Pad6 Pad7 Pad8 FX1 FX2 "
            "FX3 FX4 FX5 FX6 FX7 FX8 Sitar Banjo Shamisen Koto Kalimba Bagpipe Fiddle Shanai "
            "TinkleBell Agogo SteelDrum Woodblock Taiko MeloTom SynDrum RevCymbal GtrFret Breath "
            "Seashore Bird Phone Heli Applause Gunshot").split()

def envelope_for_program(prog):
    """(decay time constant in seconds or None for sustained, release seconds)"""
    if prog < 8:    return (1.2, 0.10)   # pianos
    if prog < 16:   return (0.6, 0.10)   # chromatic percussion (mallets, bells)
    if prog < 24:   return (None, 0.08)  # organs
    if prog < 32:   return (1.0, 0.08)   # guitars
    if prog < 40:   return (1.5, 0.06)   # basses
    if prog in (45, 46): return (0.5, 0.08)  # pizzicato, harp
    if prog == 47:  return (0.8, 0.10)   # timpani
    if prog < 56:   return (None, 0.15)  # strings, ensembles, choirs (orch hit decays a bit anyway)
    if prog < 80:   return (None, 0.08)  # brass, reeds, pipes
    if prog < 88:   return (None, 0.06)  # synth leads
    if prog < 96:   return (None, 0.25)  # synth pads
    if prog < 104:  return (None, 0.20)  # synth effects
    if prog < 109:  return (0.7, 0.08)   # sitar, banjo, shamisen, koto, kalimba
    if prog < 112:  return (None, 0.08)  # bagpipe, fiddle, shanai
    if prog < 120:  return (0.3, 0.05)   # percussive
    return (0.5, 0.10)                   # sound effects

# GM drum note -> (SN noise rate, decay seconds)
def drum_for_note(n):
    if n in (35, 36):              return (64, 0.10)   # kicks
    if n in (38, 40):              return (32, 0.14)   # snares
    if n in (37, 39):              return (32, 0.08)   # side stick, clap
    if n in (42, 44):              return (16, 0.05)   # closed / pedal hat
    if n == 46:                    return (16, 0.25)   # open hat
    if n in (49, 52, 55, 57):      return (16, 0.80)   # crashes, china, splash
    if n in (51, 53, 59):          return (16, 0.35)   # rides
    if n in (41, 43, 45):          return (64, 0.18)   # low toms
    if n in (47, 48, 50):          return (32, 0.16)   # high toms
    if n in (54, 56, 69, 70, 82):  return (16, 0.06)   # tambourine, cowbell, cabasa, maracas, shaker
    return (32, 0.10)

@dataclass
class Note:
    start: float
    end: float
    note: int
    vel: int
    prog: int

@dataclass
class Part:
    track: int
    channel: int
    notes: list = field(default_factory=list)
    cc7: list = field(default_factory=lambda: [(0.0, 100)])
    cc11: list = field(default_factory=lambda: [(0.0, 127)])
    bend: list = field(default_factory=lambda: [(0.0, 0)])
    progs: set = field(default_factory=set)

def value_at(timeline, t):
    """timeline is a sorted list of (time, value) - return the value in effect at t"""
    lo, hi = 0, len(timeline) - 1
    while lo < hi:
        mid = (lo + hi + 1) // 2
        if timeline[mid][0] <= t: lo = mid
        else: hi = mid - 1
    return timeline[lo][1]

def parse_midi(path, speed_scale):
    mid = mido.MidiFile(path)
    # global tempo map from all tracks: list of (abs tick, tempo)
    tempo_events = sorted((tick, msg.tempo) for tick, msg in
                          ((t, m) for track in mid.tracks for t, m in abs_ticks(track))
                          if msg.type == 'set_tempo')
    segs = []   # (start tick, start seconds, tempo)
    tick0, sec0, tempo = 0, 0.0, 500000
    for tick, tmp in tempo_events:
        sec0 += mido.tick2second(tick - tick0, mid.ticks_per_beat, tempo)
        tick0, tempo = tick, tmp
        segs.append((tick0, sec0, tempo))
    if not segs or segs[0][0] != 0:
        segs.insert(0, (0, 0.0, 500000))
    def t2s(tick):
        lo = 0
        for i, s in enumerate(segs):
            if s[0] <= tick: lo = i
            else: break
        st, ss, tp = segs[lo]
        return (ss + mido.tick2second(tick - st, mid.ticks_per_beat, tp)) / speed_scale

    parts = {}
    for ti, track in enumerate(mid.tracks):
        prog = {}
        on = {}
        for tick, msg in abs_ticks(track):
            if not hasattr(msg, 'channel'):
                continue
            ch = msg.channel
            key = (ti, ch)
            t = t2s(tick)
            if msg.type == 'program_change':
                prog[ch] = msg.program
                continue
            p = parts.setdefault(key, Part(ti, ch))
            if msg.type == 'control_change' and msg.control == 7:
                p.cc7.append((t, msg.value))
            elif msg.type == 'control_change' and msg.control == 11:
                p.cc11.append((t, msg.value))
            elif msg.type == 'pitchwheel':
                p.bend.append((t, msg.pitch))
            elif msg.type == 'note_on' and msg.velocity > 0:
                if (ch, msg.note) in on:   # retrigger without note off - close the old one
                    s, v, pr = on.pop((ch, msg.note))
                    p.notes.append(Note(s, t, msg.note, v, pr))
                on[(ch, msg.note)] = (t, msg.velocity, prog.get(ch, 0))
                p.progs.add(prog.get(ch, 0))
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                if (ch, msg.note) in on:
                    s, v, pr = on.pop((ch, msg.note))
                    p.notes.append(Note(s, t, msg.note, v, pr))
        end = t2s(max((tk for tk, _ in abs_ticks(track)), default=0))
        for (ch, n), (s, v, pr) in on.items():
            parts[(ti, ch)].notes.append(Note(s, end, n, v, pr))
    parts = {k: p for k, p in parts.items() if p.notes}
    for p in parts.values():
        p.notes.sort(key=lambda n: n.start)
        for tl in (p.cc7, p.cc11, p.bend):
            tl.sort(key=lambda x: x[0])
    return parts

def abs_ticks(track):
    t = 0
    for msg in track:
        t += msg.time
        yield t, msg

def render_tone_part(p, frames, poly, decay_scale, bendrange):
    slots = [[(0, 0)] * frames for _ in range(poly)]
    owner = [None] * poly            # (note, release_end_time) per slot
    stolen = 0
    pending = list(p.notes)
    for f in range(frames):
        t = f / FPS
        # start notes beginning in this frame
        while pending and pending[0].start < t + 1 / FPS:
            n = pending.pop(0)
            if n.end <= n.start:
                continue
            free = [i for i in range(poly) if owner[i] is None]
            released = [i for i in range(poly) if owner[i] is not None and owner[i].end <= n.start]
            if released:
                # a new note cuts off a release tail (as a real instrument would) - this
                # keeps monophonic lines in one slot instead of spreading them out
                owner[min(released, key=lambda i: owner[i].end)] = n
            elif free:
                owner[free[0]] = n
            else:
                # more notes held than slots - steal the slot with the oldest note
                i = min(range(poly), key=lambda i: owner[i].start)
                owner[i] = n
                stolen += 1
        for i in range(poly):
            n = owner[i]
            if n is None:
                continue
            tau, rel = envelope_for_program(n.prog)
            age = max(0.0, t - n.start)
            level = n.vel / 127.0 * 255.0
            level *= value_at(p.cc7, t) / 127.0 * value_at(p.cc11, t) / 127.0
            if tau is not None:
                level *= math.exp(-age * decay_scale / tau)
            else:
                level *= math.exp(-age * decay_scale * 0.05)   # very slow sag on sustained notes
            if t >= n.end:
                level *= math.exp(-(t - n.end) / rel)
                if t - n.end > rel * 4:
                    owner[i] = None
                    continue
            vol = int(level + 0.5)
            if vol <= 0:
                if t >= n.end:
                    owner[i] = None
                continue
            semis = n.note - 69 + value_at(p.bend, t) / 8192.0 * bendrange
            freq = 440.0 * 2 ** (semis / 12.0)
            slots[i][f] = (int(round(SN_CLOCK / freq)), min(255, vol))
    return [s for s in slots if any(v for _, v in s)], stolen

def render_drum_part(p, frames, decay_scale):
    out = [(0, 0)] * frames
    hits = []   # (start time, rate, decay, level)
    pending = list(p.notes)
    for f in range(frames):
        t = f / FPS
        newhit = False
        while pending and pending[0].start < t + 1 / FPS:
            n = pending.pop(0)
            rate, dec = drum_for_note(n.note)
            level = n.vel / 127.0 * 255.0 * value_at(p.cc7, n.start) / 127.0 * value_at(p.cc11, n.start) / 127.0
            hits.append((n.start, rate, dec, level))
            newhit = True
        best = None
        alive = []
        for h in hits:
            v = h[3] * math.exp(-max(0.0, t - h[0]) * decay_scale / h[2])
            if v >= 1:
                alive.append(h)
                if best is None or v > best[1]:
                    best = (h, v)
        hits = alive
        if best:
            h, v = best
            flags = 0x10000 if (newhit and h[0] >= t - 1 / FPS) else 0
            out[f] = (flags | h[1], min(255, int(v + 0.5)))
    return out

def main():
    args = sys.argv[1:]
    poly, bendrange, quiet = 3, 2.0, False
    partpoly = {}
    pos = []
    while args:
        a = args.pop(0)
        if a == '-poly': poly = int(args.pop(0))
        elif a == '-partpoly':
            t = int(args.pop(0)); partpoly[t] = int(args.pop(0))
        elif a == '-bendrange': bendrange = float(args.pop(0))
        elif a == '-q': quiet = True
        else: pos.append(a)
    if not 1 <= len(pos) <= 3:
        print(__doc__)
        sys.exit(1)
    infile = pos[0]
    speed_scale = float(pos[1]) if len(pos) > 1 else 1.0
    decay_scale = float(pos[2]) if len(pos) > 2 else 1.0
    if speed_scale <= 0 or decay_scale < 0:
        print("Error: speed_scale must be positive and decay_scale non-negative")
        sys.exit(1)

    parts = parse_midi(infile, speed_scale)
    length = max(n.end for p in parts.values() for n in p.notes) + 1.0
    frames = int(math.ceil(length * FPS))
    print(f"Parsing MIDI file: {infile} - {len(parts)} parts, {length:.1f} seconds, {frames} rows")

    idx = 0
    for key in sorted(parts):
        p = parts[key]
        progs = ','.join(GM_NAMES[x] for x in sorted(p.progs))
        if p.channel == 9:
            data = render_drum_part(p, frames, decay_scale)
            fn = f"{infile}_noi{idx:02d}.60hz"
            write_file(fn, data)
            print(f"track {p.track} ch {p.channel + 1:2d} drums     {len(p.notes):5d} notes -> {fn}")
            idx += 1
        else:
            slots, stolen = render_tone_part(p, frames, partpoly.get(p.track, poly), decay_scale, bendrange)
            names = []
            for s in slots:
                fn = f"{infile}_ton{idx:02d}.60hz"
                write_file(fn, s)
                names.append(fn)
                idx += 1
            msg = f"track {p.track} ch {p.channel + 1:2d} {progs:9s} {len(p.notes):5d} notes -> {', '.join(names)}"
            if stolen and partpoly.get(p.track, poly) > 1:
                msg += f" ({stolen} notes stole a slot - more than {partpoly.get(p.track, poly)} at once)"
            print(msg)
    print("Conversion complete.")

def write_file(fn, data):
    with open(fn, 'w') as f:
        for tick, vol in data:
            f.write(f"0x{tick:08X},0x{vol:02X}\n")

if __name__ == "__main__":
    main()

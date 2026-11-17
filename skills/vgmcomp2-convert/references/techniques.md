# Techniques

## Contents
- Measuring sample tuning
- Octaves and the chip floor
- Unisons: combine or detune
- Alternating melodies and monophonic merges
- Reducing parts to the chip's channels
- Flicker and jittery volume
- Levels and balance
- Arpeggios
- Drums and noise
- Lengthless songs and silence
- Verifying and reporting

## Measuring sample tuning

mod2psg maps MOD periods straight to SN counters, assuming each sample sounds ~259Hz (C4) at
period 428. Real samples were recorded at any pitch: measured examples include whole octaves
off (common), a tritone off, a sample tuned to E instead of C (factor 1.571), and bass/pad
samples 2-3 octaves off. Left alone, parts end up in the wrong octaves relative to each other
and some are off-key.

`sampletune.py` measures the fundamental (YIN) and spectral peaks. Watch for:
- Subharmonic lock: YIN can report an octave (or two) below the true fundamental - the script
  flags when the strongest peak is an integer multiple. Trust the peak structure.
- Chord samples: several strong peaks (root, third, fifth). Use the root.
- Exact ratios beat rounded semitones: most samples in a module are off by similar amounts
  (e.g. all 15-25 cents flat), so exact factors keep them in tune with each other and with
  the original as heard.
Confirm afterwards with `notes.py` (the cents column shows consistent offsets across parts).

## Octaves and the chip floor

SN can't go below ~109Hz. Choices for a low line:
- Shift the whole line up an octave (tuneins/scalepitch 0.5): keeps the contour, octave
  jumps and bass feel. Best when much of the line is low or it jumps octaves.
- Fold only the outliers (`foldoctave.py`): best when most notes already fit, or when a
  whole-line shift would land the bass on top of another part's register (a bass moved onto
  a riff's C4 gave thousands of unisons and stopped sounding like a bass - keep it low and
  fold).
- `cliptones` clamps to the limit = wrong pitch. Avoid.
Do octave folds BEFORE unison removal, so folded notes are checked too.

## Unisons: combine or detune

Two channels on the same counter can interfere audibly. Decide per case:
- Real doubling (both play the same held notes - stereo doubles, a part doubling the
  melody for whole beats): `cleardupes a b` mutes b where equal and copies the louder volume
  to a. Nothing audible is lost. Order matters: the first file keeps its notes - make that
  the more important part.
- Passing coincidences (arpeggio steps, staccato stabs, a bass landing on a chord tone,
  voices meeting on passing notes): cleardupes would punch holes in every arpeggio cycle or
  stab rhythm. Detune instead - `detunetest.py` finds a factor (typically 0.992-1.008, i.e.
  7-14 cents) with zero exact and few 1-count unisons; apply with `scalepitch`. Fold it into
  an octave shift when there is one (0.5 x 1.004 = 0.502).
- 1-2 count near misses that come from the samples' own tuning differences also beat in the
  original - leave them.
Check segment lengths to tell the cases apart: many short (<=4 row) segments = coincidence;
regular 8+ row segments = doubling.

## Alternating melodies and monophonic merges

- MOD composers often alternate one melody between two channels so each note rings into the
  next. Symptom: neither channel alone matches the tune; note-change counts are high on both;
  a later section's lead (interval n-grams via notes.py) matches neither. Fix:
  `newestwins.py a b` (most recently struck note wins) BEFORE cleardupes. A fixed-priority
  `underlaytones` drops every other note.
- MIDI legato lines: `midi2psg -partpoly <track> 1`.
- `underlaytones a b` (b has priority) is right for parts that take turns (melody handed
  from piano to an FX instrument, a part that only plays in the intro filling rests).

## Reducing parts to the chip's channels

Identify roles first (chanstats.py pairwise unison/octave stats, per-section activity,
note-change counts). Keep melody, bass, then the most characteristic third part. Typical
cuts: octave-doubling sub-basses, echo copies (same line delayed), stereo doubles, sustained
pads that duplicate a rhythmic part's notes, a part that is always covered by another.
Typical merges: parts that never overlap (underlay), chord parts into an arpeggio
(`arp3tones`), intro-only parts filling another channel's rests. Explain each cut to the
user - they may want a different choice.

## Flicker and jittery volume

A single row of silence between sounding rows, or on/off alternation, sounds like jitter -
usually envelope tails crossing the chip's quietest step, or a volume misread.
- `smoothvolume` (3-row average) on channels whose volume jumps frame to frame (sampled bass,
  pads, DMC); then `mutetones -SN` (smoothing gives rests volume - re-mute rows with
  frequency 0, otherwise counter 0 plays as the lowest note); then `despeckle` (mutes lone
  rows, fills single-row gaps with the same note on both sides).
- A single silent row between two separate notes is articulation, not flicker - leave it.
- Re-run despeckle after cleardupes/merges, which can leave single-row scraps.

## Levels and balance

- mod2psg's volume detector tracks peak-to-peak per wave cycle and misreads rippled waveforms
  (bass synths, FM basses, pads) as very quiet and jittery. Recent mod2psg versions remove DC
  offset first; older ones also read off-centre samples as near silent. Measure per
  instrument with `instlevels.py`, boost with `-volins`, pin `-vol`, then smooth.
- Check levels per section (`checkpsg.py`), not just overall - users notice a buried lead in
  one section or a quiet opening riff. Fix one section with `scalesection.py`; fix an
  instrument everywhere with -volins / scalevolume.
- VGM sources mix chips at different levels (Genesis PSG under FM) - rebalance channels.
- Boosted drums can clip into crunchy hits: cap with `clipmaxvol` halfway between the
  original peak and 255 rather than lowering all drums.
- Quiet build-ups (a part fading in) get buried by anything louder on top - check the
  first seconds specifically.

## Arpeggios

A 60Hz arpeggio sounds harsher and louder than a single note at the same volume. Mix arps
well below the parts around them (x0.4 was right once). Never cleardupes against an arp
channel; detune instead.

## Drums and noise

- SN: fixed noise rates 16/32/64. Kick 64, snare/clap 32, hats/cymbals/shakers 16, toms
  64/32. Merge all drum sources with `mergenoise` (louder wins); remember to rename the
  `.merge.old` backup between repeated merges into the same file.
- Drum hits that are a single row or separated by one silent row are authentic.
- FM drum channels (VGM) and SID noise frames go to noise; `fixsnnoise` maps arbitrary rates.

## Lengthless songs and silence

SID and NSF loop forever and have no length. Render 4 minutes, `findloop.py` the tone files,
then keep intro + one loop with `truncrows.py` so the song ends exactly at the loop point.
`trimsilence <one channel>` removes silent rows from start/end of the whole song (it finds
all channels by prefix) - note the row shift if you reference row numbers later.

## Verifying and reporting

After every build: `checkpsg.py` (no alternating runs, no exact unisons, sensible levels per
section) and `notes.py` (melody matches the source: compare note sequences or interval
patterns against the MIDI/MOD, or one section of the tune against another). Report what you
measured and what you can't know (you can't hear it). Give the testPlayer command. When the
user reports a problem, locate it in rows, inspect the raw files around it, find the cause,
and fix the cause - not just the symptom.

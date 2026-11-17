# Sources

## Contents
- MOD / tracker formats (mod2psg)
- MIDI (midi2psg.py)
- VGM / VGZ (vgm_*2psg)
- C64 SID (sid2psg)
- NES NSF (nsf2vgm.py + vgm_nes2psg)
- Voice samples and drum loops inside MODs

## MOD / tracker formats (mod2psg)

mod2psg plays the module through libmodplug and samples each channel 60 times a second.
Each MOD channel becomes `_tonNN` (pitched instruments) and `_noiNN` (noise instruments,
numbered after the tones), so a 4 channel MOD can give 8 files.

Reconnaissance:
- `modinfo.py song.mod -positions` - samples, which instrument plays on which channel,
  per-order-position instrument table with start rows (60hz), effects used.
- `sampletune.py song.mod` - real pitch of each sample and suggested `-tuneins`.
- Instrument names often say what they are ("bassdrum", "snare", "Sp..." for speech) - but
  verify from the sample data: a "440THICK-BK" turned out to be a kick drum (pitch falling
  150->60Hz over 7 frames); a "kraftsmatter" was a 3-frame tick.
- Classify unpitched samples as noise: low YIN confidence, envelope-only, or a pitch that
  drops fast (kick). Compare frame RMS and zero-crossing rate over the first ~10 frames.
- Old MODs converted with older tools may carry a `DRUMS$xxxxxxxx` sample-name tag (MSB =
  sample 1). It was a human estimate - use it as a hint only.

Import options that matter:
- `-forcenoise <n>` / `-forcetone <n>`: ONE instrument per flag - repeat the flag. (The
  manual says comma lists; the code uses atoi, so `5,6,7` silently means just 5.)
- `-forcefreq <ins> <16|32|64>`: fixed SN noise rate per drum (16 high ... 64 low).
- `-tuneins <ins> <factor>`: counter multiplier (2.0 = octave down, 0.5 = octave up).
- `-volins <ins> <factor>`: per instrument volume; 0 mutes (use for voices/loops rendered
  separately). Changing any -volins changes the auto volume scale - pin it with `-vol`
  (read "Calculated volume scale" from a plain run first).
- Effects (portamento, vibrato, arpeggio, volume slides, speed/tempo changes) are played by
  libmodplug, so they come through automatically.

Common MOD traits:
- Stereo doubling: the same notes on two channels (Amiga stereo width) -> cleardupes.
- Alternating melodies across two channels -> newestwins.py (see techniques).
- Chord samples (one sample = a whole chord): one channel can only play one pitch - use the
  root at the chord's real pitch; check peaks to find it.
- Sustained 4-voice chords in the middle register: one voice must go - keep melody and bass
  first, then the most characteristic voice.

## MIDI (midi2psg.py, in the vgmcomp2 `midi2psg` folder)

Recent versions of midi2psg (check for `-partpoly` in its usage text): parts are split by
(track, MIDI channel); channel 10 is drums (GM note -> noise rate, retrigger flags); notes
get envelopes by GM family (plucked decays, sustained holds); CC7/CC11/pitch bend applied;
each note keeps its output slot; one global tempo map. Older versions treated tracks as
channels, mis-detected drums, ignored tempo changes on other tracks and gave every note
the same piano-like decay - if the user has an old one, say so.

- `-poly <n>` (default 3) slots per part; `-partpoly <track> <n>` per track. Use 1 for
  melodic lines whose notes overlap (legato) - otherwise phrases spill into a second slot
  file that you'd drop, and notes go missing.
- The summary lists which part/instrument went to which file - read it before arranging.

Reconnaissance with mido: per (track, channel) part - program, note count, range, maximum
polyphony, how often active per section, and pairwise unison/octave relations (a part that
mostly doubles another at the octave is a cut candidate).

Arranging MIDI: decide by role. Typical keeps: melody (merge parts that never overlap, e.g.
piano + FX taking over the melody), bass/bass figure, and the most characteristic rhythm or
harmony part. Chord parts -> `arp3tones` into one channel (3-note arpeggio), mixed low, or
used to fill rests under another part with `underlaytones`. Sub-bass doublings, sustained
string pads that duplicate stabs, reverse cymbals etc. are cut.

## VGM / VGZ

`vgm_id file.vgz` lists chips. Then the matching importer: `vgm_psg2psg` (SN), `vgm_ay2psg`,
`vgm_nes2psg`, `vgm_gb2psg`, `vgm_md2psg` (Genesis YM2612 + SN), `vgm_pokey2psg`.
- Genesis: FM channels first (ton00-05), DAC if used, then the PSG tones and noise. FM
  channel volumes come out far lower than the PSG's - rebalance. FM drum channels (low
  E1-ish hits every few rows plus pitch sweeps) belong on the noise channel: copy the tone
  file to a `_noi` name, `fixsnnoise`, `mergenoise` with the PSG noise. Genesis tunes often
  have detuned/echo pairs of FM channels (one is cut) and a PSG part written for the SN.
- NES: squares, triangle, noise, DMC. The DMC is often a sampled bass (Sunsoft) - keep it;
  its level jitters (smooth if it flickers). Triangle may be fills or bass.
- Parts written natively for an SN (Genesis/SMS PSG) need little more than levels.

## C64 SID (sid2psg)

`sid2psg -len <seconds> file.sid` - the length is mandatory and unknown: render ~240s, run
`findloop.py` on the tone files, then rerender just past one pass and `truncrows.py` all
channels to intro + one loop. The header says PAL/NTSC; sid2psg runs the player at true
speed, sampled into 60hz rows (so loops don't repeat row-exact - findloop allows for this).
- 3 voices -> ton00-02, plus each voice's noise-waveform frames as noi03-05. Voice 3 often
  plays bass AND drums (switches to noise between notes) - its noise goes to the SN noise
  channel; bass gaps during hits are authentic.
- SID bass goes far below the SN floor; voices often meet on passing notes (detune).

## NES NSF (nsf2vgm.py + vgm_nes2psg)

The toolchain has no NSF importer; `scripts/nsf2vgm.py` emulates the 6502 (bankswitching,
NTSC/PAL, DMC sample blocks) and writes an NES VGM: `python nsf2vgm.py -track 1 -len 240
song.nsf out.vgm`, then `vgm_nes2psg out.vgm`. Find the loop as for SID. Expansion audio
(VRC6, FDS, ...) is reported and ignored.

## Voice samples and drum loops inside MODs

mod2psg renders a speech sample as one fixed pitch with an envelope - a monotone buzz. Better:
1. Mute them in mod2psg (`-volins <n> 0`).
2. `extractvoice.py song.mod voice <ins,list>` - a WAV per (instrument, period) used, at the
   rate the MOD plays it (pitch and speed come out right).
3. Voices: `voice2psg_proto.py -channels 2 -mode lpc -noise v05_226.wav` (LPC formant
   tracking: channels follow F1/F2, snapped to the speaker's harmonics; consonants as noise).
   The original `voice2psg.exe` picks the loudest FFT bins - both channels often land on the
   same peak and swap order every frame; the prototype is clearly more intelligible.
4. Drum loops / breakbeats (a long sample retriggered every bar, envelope full of dips,
   aperiodic): `drums2noise_proto.py v04_190.wav` - per frame kick/snare/hat classification
   to noise rates 64/32/16 with retrigger flags.
5. `splicevoice.py song.mod voice <rows> <ins,list> <peak> <prefix>` places each phrase at its
   trigger rows, honours MOD volume commands and note cutoffs (stutters work), louder voice
   wins on overlap. Outputs `<prefix>_ton00/01` and `_noi02`.
6. Put the voice tracks on channels where the music is quiet during speech (check with
   chanstats); underlay the music under them; merge voice noise into the noise channel.
Check timing alignment (splice frame vs a mod2psg drum onset) before trusting it.

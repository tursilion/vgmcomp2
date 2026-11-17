---
name: vgmcomp2-convert
description: Convert music (ProTracker/S3M/XM/IT MODs, MIDI, VGM/VGZ from Genesis/SMS/NES/Game Boy/AY/Pokey, C64 SID, NES NSF, voice WAVs) into chip-ready data for the TI SN76489 (TI-99/4A, ColecoVision, SMS), AY-3-8910 or C64 SID using Tursi's vgmcomp2 toolchain (mod2psg, midi2psg, vgm_*2psg, sid2psg, prepare4SN/AY/SID, testPlayer). Use this whenever someone wants to port, arrange, reduce, retune or clean up a song for these sound chips, works with .60hz channel files or .psg/.snpsg prepared files, mentions vgmcomp2/vgmcomp or any of its tools, or asks why a converted tune sounds wrong (missing notes, wrong octave, flicker, too quiet, harsh unisons) - even if they only say "make this MOD play on the TI".
---

# vgmcomp2 song conversion

vgmcomp2 converts music into 60Hz frame data for small sound chips. A conversion is never a
single command: sources have more channels than the chip, samples are mistuned, volumes are
misread, and the result has to be arranged. This skill is the workflow and the hard-won
techniques for doing that well. Work like an arranger: measure, decide what matters
musically, apply tools, verify with numbers, then let the user listen and iterate.

## 1. Setup - find the toolchain (paths differ on every machine)

Locate the vgmcomp2 folder before anything else; never assume a path. Look for a directory
containing `Release/mod2psg.exe`, `Release/prepare4SN.exe`, `Release/testPlayer.exe` (or the
`dist/VGMComp2.zip` that holds them). Ask the user if it isn't found. Also read
`dist/VGMComp2.pdf` / `.docx` there if you need tool details beyond `references/tools.md`.

- The tools are Windows console programs. On Windows run them directly. From WSL run them
  through `cmd.exe /c "cd /d <windows path> && <script>.cmd"` (use `wslpath -w` for paths) -
  they work fine. On Linux/macOS use wine.
- Python 3 is needed for the helper scripts here (and for `midi2psg.py`); numpy for voice
  and sample analysis; `mido` for MIDI. Use the Python that will run the build (on WSL the
  Windows Python is usually simplest; a system-managed Linux Python may need a venv).
- The scripts in this skill's `scripts/` folder are self-contained. Refer to them by the
  skill's actual path, or copy the ones a song needs next to its build script.

## 2. Workflow

1. **Workspace**: one folder per song, copy the source in, and write a `build_<song>.cmd`
   (or `.sh`) that rebuilds everything from the source with a comment on every step saying
   *why*. Rerun the whole script after each change - many tools rename their inputs to
   `.old` backups and refuse to overwrite existing backups, so partial reruns break.
2. **Reconnaissance** (before converting): identify every part and its role.
   - MOD: `scripts/modinfo.py song.mod -positions`, then `scripts/sampletune.py song.mod`.
   - MIDI: list tracks/channels/programs/polyphony (see `references/sources.md`).
   - VGM: `vgm_id`, then import and `scripts/chanstats.py "prefix_*.60hz"`.
   - SID/NSF: render long (240s), `scripts/findloop.py` to find one pass.
3. **Import** with the right options (`references/sources.md`): force drums to noise at
   fixed rates, tune samples to their true pitch, mute voice samples to render separately.
4. **Arrange** to the chip's channel count (`references/chips.md`): keep melody, bass, and
   the most characteristic third part; merge parts that never overlap; cut doublings.
5. **Clean up**: fold out-of-range notes, smooth jittery volumes, fix flicker, resolve
   unisons, balance levels (`references/techniques.md`).
6. **Prepare**: `prepare4SN` / `prepare4AY` / `prepare4SID` -> one prepared file.
7. **Verify** with `scripts/checkpsg.py` (flicker, unisons, section levels) and
   `scripts/notes.py` (melody matches the source). Report numbers, not impressions.
8. **Hand over**: tell the user what was kept/cut and why, anything uncertain, and give a
   copy-paste testPlayer command with their real path, e.g.
   `<vgmcomp2>\Release\testPlayer.exe -sn song.snpsg` (`-ay` for AY; testPlayer has no SID
   mode). testPlayer plays tool output only - not .wav files. For a long song, offer a
   trimmed copy of the prepared file starting at the section under review.
9. **Iterate** on the user's listening feedback. You can measure but you cannot hear; their
   ears are the judge. Turn each complaint into a measurement, fix the cause, re-verify.

## 3. The techniques that matter most

Details and reasoning for each are in `references/techniques.md`. The short version:

- **Measure sample tuning, don't trust it.** mod2psg assumes every sample sounds ~C4 at
  period 428. Real samples can be octaves, a tritone, or 4 semitones off. Use exact measured
  ratios (`sampletune.py`) so parts stay in tune with each other.
- **Octaves: shift whole lines, fold only outliers.** Notes below the chip floor (SN
  counter 0x3ff = ~109Hz) must move. Move a whole line an octave when much of it is low and
  it has octave jumps; fold only the outliers (`foldoctave.py`) when most of it already fits
  or when shifting would land it on another part's register. cliptones clamps (wrong note).
- **Unisons: combine doublings, detune coincidences.** Two channels on the same counter
  interfere. Real doubling (held notes in unison) -> `cleardupes`. Passing coincidences
  (arpeggio steps, stabs, a bass that meets a chord tone) -> detune a few cents with
  `scalepitch` (`detunetest.py` picks the factor). Never cleardupes against an arpeggio or
  staccato channel: it punches a hole in every cycle.
- **Alternating melodies**: MODs often split one melody across two channels so notes ring
  into each other. Merge with `newestwins.py` (most recent note wins), *before* cleardupes.
  Fixed-priority `underlaytones` would drop every other note.
- **Monophonic MIDI parts**: `midi2psg.py -partpoly <track> 1` so overlapping legato notes
  don't spill into a second slot that gets dropped.
- **Volume detector**: mod2psg misreads rippled or off-centre samples (bass synths, pads)
  as very quiet and jittery. Measure per instrument (`instlevels.py`), boost with `-volins`,
  pin `-vol <original auto scale>` (or mod2psg rescales everything back down), then
  `smoothvolume` + `mutetones -SN` (re-mute true rests) + `despeckle`.
- **Arpeggios sound harsher and louder** than a single note at the same volume - mix them
  well below (x0.4 was right in one tune) so they don't bury what's under them.
- **Section fixes**: when one part is buried in one section only, use `scalesection.py`
  instead of changing an instrument everywhere.
- **Drums**: map to the fixed noise rates (16 high, 32 mid, 64 low). Kicks low, snares mid,
  hats/cymbals high. Cap loud hits (`clipmaxvol`) if boosted drums sound crunchy.
- **Voices and drum loops** in MODs: render separately (`voice2psg_proto.py`,
  `drums2noise_proto.py`) and splice at the trigger points (`splicevoice.py`). See
  `references/sources.md`.
- **Lengthless sources** (SID, NSF): find the loop, keep exactly one pass
  (`truncrows.py`); `trimsilence` long silent tails.

## 4. Reference files

- `references/sources.md` - per source type: MOD, MIDI, VGM chips, SID, NSF, voice samples.
- `references/chips.md` - SN, AY and SID targets: ranges, noise, volume, prepare tool quirks.
- `references/techniques.md` - the techniques above in depth, with the reasons.
- `references/tools.md` - toolchain command reference and gotchas.

## 5. Scripts (in `scripts/`)

Analysis: `modinfo.py`, `sampletune.py`, `chanstats.py`, `instlevels.py`, `findloop.py`,
`detunetest.py`, `checkpsg.py`, `notes.py`.
Editing (each keeps a `.old` backup): `foldoctave.py`, `newestwins.py`, `scalesection.py`,
`truncrows.py`.
Sources: `nsf2vgm.py` (NSF -> VGM for vgm_nes2psg), `extractvoice.py` + `voice2psg_proto.py`
+ `drums2noise_proto.py` + `splicevoice.py` (MOD voices and drum loops).
Each script's header comment explains its usage.

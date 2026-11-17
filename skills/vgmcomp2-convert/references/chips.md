# Target chips

All `.60hz` channel files use SN shift counts for frequency (even for AY/SID targets) and
linear 0-255 volume. The prepare tool for the target converts and enforces limits, writing
one prepared file of 4 channels per row (tone,vol x3 + noise,vol). Name it for the chip
(`.snpsg`, `.aypsg`/`.psgay`, `.sidpsg`/`.psgsid`) so it isn't confused later.

## SN76489 (TI-99/4A, ColecoVision, SMS) - prepare4SN

- 3 square tone channels + 1 noise channel.
- Tone counter 0x001-0x3ff: lowest note ~109Hz (A2). Bass lines from almost every source go
  lower - shift the line up an octave or fold the low notes (see techniques). Counters below
  ~7 are inaudible.
- Volume: 16 logarithmic attenuation steps (0 loud, 15 silent). The quietest steps are where
  envelope tails flicker on/off.
- Noise: white or periodic, at fixed rates 16/32/64 (high/mid/low), or tracking tone channel
  3's frequency. Using channel 3 costs a tone voice, so map drums to the fixed rates
  (`fixsnnoise`, or `-forcefreq` in mod2psg). Periodic noise at a tracked rate can play
  bass (`bass2noise`) if the noise channel is otherwise free.
- Identical counters on two channels can interfere audibly - see unisons in techniques.
- prepare4SN counts "tones clipped" on silent rows too (rests that keep an old low counter);
  check which rows before worrying.
- Verify: `testPlayer.exe -sn file.snpsg`.

## AY-3-8910 / YM2149 (ColecoVision SGM, MSX, Spectrum, ...) - prepare4AY

- 3 tone channels; ONE noise generator shared through the mixer: noise plays through one of
  the tone channels' volume. prepare4AY maps the noise volume to the closest tone channel
  volume (or a muted channel). So drums and the tone carrying them share a level - balance
  noise volumes to match a tone channel beforehand, and expect a drum to color whichever
  channel it borrows.
- Tone range 0x001-0xfff in file units: about 2 octaves lower than the SN, so far fewer
  bass notes need folding (`foldoctave.py 0xfff`), and whole-line octave shifts are usually
  unnecessary - keep bass at true pitch.
- Noise rate 0x00-0x1f (clipped at 0x1f); periodic/trigger flags are ignored.
- Volume 16 levels, absolute (0 mute, 15 loud), logarithmic like the SN.
- Envelopes/hardware AY features are not used by the toolchain.
- The same arranging and cleanup techniques apply; the unison and flicker checks still
  matter (`checkpsg.py -chip ay`). Verify with `testPlayer.exe -ay`.

## C64 SID (SID Blaster on TI) - prepare4SID

- 3 voices only, each either tone or noise, and the type is NOT stored - you decide (and
  must remember) which voice is noise. There is no 4th noise channel: drums must take a
  voice, or share one (like many SID composers: a voice alternates bass and drum hits -
  build that with underlaytones/mergenoise onto one channel and set its waveform per song).
  The prepared file's 4th channel is a dummy.
- Frequency is inverted (higher counter = higher pitch) and spans 16 bits; in file units
  notes are only limited at the top (counter below ~29 in SN units, i.e. very high notes) -
  fold those down with `foldoctave.py 0xffff -min 29`. Bass needs no folding.
- Volume 16 levels, linear (not logarithmic) - balance by ear differently: quiet parts read
  quieter on SID than the same number on SN/AY.
- Arrangement is the hard part: with only 3 voices for melody + bass + drums, usually drop
  the third harmony part, or alternate drum hits into the bass voice's gaps.
- testPlayer has no SID mode; verify with the numbers and on hardware/emulator.

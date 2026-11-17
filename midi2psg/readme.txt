midi2psg.py - convert a MIDI file to PSG channel files (60hz) for the vgmcomp2 tools.

usage: python midi2psg.py [options] <input.mid> [speed_scale] [decay_scale]
  speed_scale   song speed multiplier (default 1.0, 0.5=half speed, 2.0=double speed)
  decay_scale   envelope decay rate multiplier (default 1.0, 0.5=slower, 2.0=faster)
  -poly <n>       maximum notes per part (default 3)
  -partpoly <track> <n>  maximum notes for one track (repeatable, 1 = monophonic)
  -bendrange <n>  pitch bend range in semitones (default 2)
  -q              quiet

- Each part (a track + MIDI channel pair) is written as one tone file per polyphony slot
  (-poly, or -partpoly <track> <n> for one track; 1 = monophonic, newest note wins).
  A note keeps its slot for its whole life, and a new note takes over a slot whose note
  has been released first, so monophonic lines stay in a single file.
- MIDI channel 10 parts are drums: GM drum notes are mapped to SN noise rates (kick 64,
  snare 32, hats/cymbals 16, toms 64/32) with a decay per drum, the louder hit wins, and
  the retrigger flag is set on each hit. One noise file per drum part.
- Notes are shaped by their GM instrument family: plucked/struck instruments (piano,
  guitar, bass, mallets...) decay, sustained ones (organ, strings, brass, winds, synths)
  hold until note off. Every note gets a short release after note off.
- Channel volume (CC7), expression (CC11) and pitch bend are applied.
- One tempo map is used for all tracks.
- Output files are numbered in order of track, then channel. The summary printed at the
  end lists which part (and GM instrument) went to which file.
- Requires the mido library (pip install mido) and Python 3.

Script was originally written by Claude.ai.

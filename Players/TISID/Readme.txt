See the notes in TISN - this is just a quick port of the hand-edit code
to play back on the SID Blaster instead. The specifications there are
not guaranteed here, but it should be pretty close. Maybe a little faster
since there are only three channels.

There IS one gotcha, though, you need to specify which of the three
channels are noise or tone before you start. In order to make that
easy, three configuration registers are set before you call StartSong,
and this value will go directly into the control reg, so you can set
noise, pulse, sawtooth or triangle... but I don't recommend the others.

Anyway, you need to know what the song is using and set them appropriately.

The player does NOT use the ADSR capabilities. This means that we 
toggle GATE to retrigger when we need to get louder. This may cause
an audible click - it's a side effect of how we do things.

It's not implemented here, but because we check the running volume
to determine when to toggle gate, if you resume after mutes you
will need to manually toggle any affected channel's gate bit, otherwise
you won't hear anything until the volume on that channel rises. At
this time I'm not building the code for SFX+Music on the SID, I'm
not even sure how useful it is as this, since it's so limiting.
It's really just three extra channels if you need them. Naturally,
if you play /just/ SFX or just music then it's not even a concern.

sidVol reports volume is in the most significant nibble. (it's the sustain bits)
sidTone is in little endian byte order

Also, there's no toggle bits reported on the volume output, it's
just the raw register write. You can add that if you need it but
it's just extra work here, while it was more or less free on the SN.

People are welcome to build a better encode system for SID. ;) But
since this system is all dealing with SN data, it is what it is.

Functions:

StartSID(unsigned char *pSong, unsigned char nIdx)
- start playing song number nIdx (0-based) inside SBF file pSong. pSong must
  be word aligned. For asm, store pSong in R1 and nIdx in R2 MSB.
- Before calling, initialize the byte values "SidCtrl1", "SidCtrl2",
  and "SidCtrl3" with the control register bits for each voice.

StopSID
- stop playing any active song

SongSID
- call this function once every frame to play the music. Note that
  your call must be able to tolerate all registers being modified

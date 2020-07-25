#ifndef INCLUDE_COLECOAYPLAY_H
#define INCLUDE_COLECOAYPLAY_H

// As long as BUILD_COLECO is set, we can just install this header directly
#include "../CPlayer/CPlayer.h"

// but there are extra functions defined that we need to include
// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void ay_StartSong(const unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void ay_StopSong();

// this needs to be called 60 times per second by your system
void ay_SongLoop();

// this array contains the current volume of each voice (ignoring mutes)
// Sound chip specific, but in both cases that means 0x0 is maximum and 0xF
// is muted. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command. if it is 0xff
// then no channels are playing noise.
// You, the caller, need to strip the trigger nibble when you are done
// (if it matters to your software, that is)
extern uint8 ay_songVol[4];

// this array contains the current note on each voice (ignoring mutes)
// The most significant nibble is set to the PSG command nibble when a
// new trigger has occurred. 
// Note that these command nibbles are not meaningful to the AY PSG
// You, the caller, need to strip the trigger nibble when you are done
// (if it matters to your software, that is)
// However, do not strip them if you are using the SFX player, as the SFX
// player requires those nibbles to write the data back.
extern uint16 ay_songNote[4];

// helpful wrapper
#define isAYPlaying ((ay_songNote[3]&SONGACTIVEACTIVE) != 0)

#define CALL_PLAYER_AY \
    ay_SongLoop();

#endif  // file include

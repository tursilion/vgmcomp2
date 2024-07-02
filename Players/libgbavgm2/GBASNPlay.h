#ifndef INCLUDE_GBASNPLAY_H
#define INCLUDE_GBASNPLAY_H

// As long as BUILD_GBA is set, we can just install this header directly
#include "CPlayer.h"

#define CALL_PLAYER_SN \
    SongLoop();

// helpful wrapper
#define isSNPlaying ((songNote[3]&SONGACTIVEACTIVE) != 0)

// call this to set up the initial registers
void gbasninit();
// call this when the interrupt indicates audio data needed
void snupdateaudio();

#endif  // file include

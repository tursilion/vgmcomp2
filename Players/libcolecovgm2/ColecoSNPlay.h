#ifndef INCLUDE_COLECOSNPLAY_H
#define INCLUDE_COLECOSNPLAY_H

// As long as BUILD_COLECO is set, we can just install this header directly
#include "ColCPlayer.h"

#define CALL_PLAYER_SN \
    SongLoop();

// helpful wrapper
#define isSNPlaying ((songNote[3]&SONGACTIVEACTIVE) != 0)

#endif  // file include

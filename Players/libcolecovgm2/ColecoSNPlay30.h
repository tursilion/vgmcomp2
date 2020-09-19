#ifndef INCLUDE_COLECOSNPLAY30_H
#define INCLUDE_COLECOSNPLAY30_H

// As long as BUILD_COLECO is set, we can just install this header directly
#include "ColCPlayer30.h"

#define CALL_PLAYER_SN30 \
    SongLoop30();

// helpful wrapper
#define isSNPlaying ((songNote30[3]&SONGACTIVEACTIVE) != 0)

#endif  // file include

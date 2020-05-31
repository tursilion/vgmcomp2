// Wrapper for for CPlayer for TI SN
// Note that you can use a very similar wrapper
// to create CPlayerSfx, just define SONG_PREFIX
// #define SONG_PREFIX sfx_

#ifdef USE_SN_PSG
#include "sound.h"
// provide the WRITE_BYTE_TO_SOUND_CHIP macro
#define WRITE_BYTE_TO_SOUND_CHIP(x) SOUND=(x)
#endif

#ifdef USE_AY_PSG
// At the moment, this is not possible on the TI, but we'll see...
#define WRITE_BYTE_TO_SOUND_CHIP(reg,x) writeSound(reg,x)
#endif

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

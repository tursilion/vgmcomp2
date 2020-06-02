// Wrapper for for CPlayer for TI SN
// Note that you can use a very similar wrapper
// to create CPlayerSfx, just define SONG_PREFIX
// #define SONG_PREFIX sfx_

#ifdef USE_SN_PSG

#include "sound.h"

// provide the WRITE_BYTE_TO_SOUND_CHIP macro
// arguments are songActive (for mutes), channel (for mutes), byte
// if you don't need SFX and don't care about mutes, you can remove the array and if clause
const unsigned char muteMap[5] = { 0x00, 0x80, 0x40, 0x20, 0x10 };
#define WRITE_BYTE_TO_SOUND_CHIP(soundActive, chan, x) if (0 == ((soundActive)&muteMap[(chan)])) SOUND=(x)

#endif

#ifdef USE_AY_PSG

// At the moment, this is not possible on the TI, but we'll see...
// if you don't need SFX and don't care about mutes, you can remove the array and if clause
// Note that because noise shares volume with the tone channels, it may be difficult to control
// the volume of a noise-only sound effect, consider pairing it with a tone channel
const unsigned char muteMap[5] = { 0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x80, 0x40, 0x20 };
#define WRITE_BYTE_TO_SOUND_CHIP(soundActive, reg, x) if (0 == ((soundActive)&muteMap[(reg)])) SOUND=(x)

#endif

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

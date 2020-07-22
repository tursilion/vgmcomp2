// Wrapper for CPlayer.c that gives us the SN functions
// this directly includes CPlayer.c

// direct sound chip access
volatile __sfr __at 0xff SOUND;

// prefix for SFX player
#define SONG_PREFIX sfx_

// build for SN
#define USE_SN_PSG

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
// we don't USE the mute here, but we apply it to the main player
// Mute is cleared in the CALL_PLAYER_SFX macro
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    SOUND=(x); songNote[3]|=(0x80>>(chan-1));

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

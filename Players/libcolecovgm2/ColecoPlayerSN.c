// Wrapper for CPlayer.c that gives us the SN functions
// this directly includes CPlayer.c

// direct sound chip access
volatile __sfr __at 0xff SOUND;

// no prefix for SN player
//#define SONG_PREFIX sfx_

// build for SN
#define USE_SN_PSG

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    if (((mutes)&(0x80>>((chan)-1)))==0) SOUND=(x);

// and now include the actual implementation
#include "ColCPlayer.c"

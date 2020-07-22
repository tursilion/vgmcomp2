// Wrapper for CPlayer.c that gives us the AY functions
// This assumes Phoenix mapping (same as SGM)
// this directly includes CPlayer.c
// No mutes used in this player.

// direct sound chip access
volatile __sfr __at 0x50 AYADR;
volatile __sfr __at 0x51 AYDAT;

// prefix for AY player
#define SONG_PREFIX ay_

// build for AY
#define USE_AY_PSG

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
// (Mutes are ignored here)
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    AYADR=(chan); AYDAT=(x);

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

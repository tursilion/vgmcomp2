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

#ifdef USE_SID_PSG

// SID has got 29 registers, so the mute map is a bit larger...
// This player technically only hits the frequency and Sustain regs, but
// it's faster to have the whole table anyway, might as well be accurate
// NOTE: it is the player's responsibility to make sure the SID is mapped in!
const unsigned char muteMap[] = {  
    0x80,0x80,0x80,0x80,0x80,0x80,0x80, // chan 1
    0x40,0x40,0x40,0x40,0x40,0x40,0x40, // chan 2
    0x20,0x20,0x20,0x20,0x20,0x20,0x20, // chan 3
    0,0,0,0                             // filter
    //0,0,0,0                           // these are read only anyway, don't bother with them
};

// TODO: I don't remember - do we need to delay then switch to another
// register or is it only certain registers that cause an issue?
// If so, the player can do the switch after all voices are loaded,
// and I think there should be sufficient delay between writes...??
// The XB sample code does a write to 0x5832 (R/O POTX reg) after all
// writes are complete, probably to get the address latch off the
// configuration registers. So we probably should do the same.
#define WRITE_BYTE_TO_SOUND_CHIP(soundActive, reg, x)       \
    if (0 == ((soundActive)&muteMap[(reg)])) {              \
        *((volatile unsigned char *)(0x5800+reg*2)) = (x);  \
    }

#endif

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

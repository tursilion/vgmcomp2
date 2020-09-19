// Wrapper for CPlayer.c that gives us the SN 30Hz SFX functions
// this directly includes CPlayer.c

#include "ColecoSfxPlay30.h"

// direct sound chip access
volatile __sfr __at 0xff SOUND;

// prefix for SFX player
#define SONG_PREFIX sfx_

// build for SN
#define USE_SN_PSG

// current priority - you must manually clear this!
unsigned char sfxPriority;

// wrapper function to handle sfx priorities
// we might need to be smarter and patch the main player
// with SFX requirements... but we'll try to cheat and do this...
void StartSfx30(unsigned char* music, unsigned char song, unsigned char priority) {
    if ((priority > sfxPriority) || (!isSFXPlaying)) {
        sfxPriority = priority;
        sfx_StartSong30(music, song);
    }
}

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// for the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
// we don't USE the mute here, but we apply it to the main player
// Mute is cleared in the CALL_PLAYER_SFX macro
// This isn't perfect - long tones won't update the sound channel
// so might be overridden with music. We'll see how it plays out.
// Note this writes to sfx_songNote[] due to macro expansion!!
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    SOUND=(x); songNote[3]|=(0x80>>(chan-1));

// and now include the actual implementation
#include "ColCPlayer30.c"

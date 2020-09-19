#ifndef INCLUDE_TISFXPLAY_H
#define INCLUDE_TISFXPLAY_H

// most of the important defines need to be called from TISNPlay.h
// which must be included before this one

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
// pri - priority to play (lower number = higher priority)
void StartSfx(const unsigned char *pSbf, uWordSize songNum, uWordSize pri);

// Call this to stop the current sfx
void StopSfx();

// Main loop - do not call this directly, use CALL_PLAYER_SFX macro below
void SfxLoop();

// 30hz loop - do not call this directly, use CALL_PLAYER_SFX30 macro below
void SfxLoop30();

// helpful wrapper
#define isSFXPlaying ((sfxActive&(SONGACTIVEACTIVE<<8)) != 0)

// MSB is the active bit with the mutes, LSB is current priority
extern unsigned int sfxActive;

// Option 3: use the hand tuned asm code directly with register preservation
// Have to mark all regs as clobbered. Determine vblank any way you like
// (I recommend VDP_WAIT_VBLANK_CRU), and then include this define "CALL_PLAYER_SFX;"
// This is probably the safest for the hand-tuned code. GCC will decide whether
// it needs to preserve any registers. Make sure to call SFX before SN so it can
// properly set the mutes.
#define CALL_PLAYER_SFX \
    __asm__(                                                        \
        "bl @SfxLoop"                                               \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

// same as above, but for the 30hz player
#define CALL_PLAYER_SFX30 \
    __asm__(                                                        \
        "bl @SfxLoop30"                                             \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

#endif  // file include

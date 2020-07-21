#ifndef INCLUDE_TISIDPLAY_H
#define INCLUDE_TISIDPLAY_H

// Separate header to deal with the slightly different names,
// which is done so SID and SN can live in the same project

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// some defines for the SID tone types
#define SID_TONE_NOISE  0x80
#define SID_TONE_PULSE  0x40
#define SID_TONE_SAW    0x20
#define SID_TONE_TRI    0x10
extern unsigned char SidCtrl1,SidCtrl2,SidCtrl3;

#ifndef DEFINED_TI_TYPES
#define DEFINED_TI_TYPES
typedef int int16;              // must be 16 bit or larger, signed is okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned char uWordSize;// most efficient word size, 8 bits or more unsigned
#endif

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSID(const unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void StopSID();

// Main loop - do not call this directly, use CALL_PLAYER_SID macro below
void SIDLoop();

// helpful wrapper
#define isSIDPlaying ((sidNote[3]&SONGACTIVEACTIVE) != 0)

// this array contains the current volume of each voice (ignoring mutes)
// volume is in the most significant nibble. Note that sidVol[3] is unused.
extern uint8 sidVol[4];

// this array contains the current note on each voice (ignoring mutes)
// it is in little endian order. sidNote[3] is unused except for the active bit below.
extern uint16 sidNote[4];

// songActive is the LSB of sidNote[3]
// this flag contains 1 if playing, zero if stopped
// you can also stop (or pause!) a song by setting it to zero

// we define bits for songActive per LSB 8-bits
#define SONGACTIVEACTIVE 0x01
#define SONGACTIVEMUTE1  0x80
#define SONGACTIVEMUTE2  0x40
#define SONGACTIVEMUTE3  0x20
#define SONGACTIVEMUTE4  0x10

// Option 3: use the hand tuned asm code directly with register preservation
// Have to mark all regs as clobbered. Determine vblank any way you like
// (I recommend VDP_WAIT_VBLANK_CRU), and then include this define "CALL_PLAYER_SID;"
// This is probably the safest for the hand-tuned code. GCC will decide whether
// it needs to preserve any registers.
#define CALL_PLAYER_SID \
    __asm__(                                                        \
        "bl @SIDLoop"                                               \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

#endif  // file include

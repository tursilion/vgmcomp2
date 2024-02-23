This links together the TI playback code into a single linkable library. Link with -ltivgm2

There are three players involved. The SN and SID players will standalone, but the SFX player will also bring in SN. (There is no SID SFX player at the moment). All players will share the common decompression code.

There are also 30hz versions of each playback. Use the normal start and stop calls for these. Do not mix 30 and 60 hz playback.
** 30 hz players are not currently tested **

-----------------------
BASIC: getting it setup
-----------------------

FROM C:
 - tested with GCC 4.4.0
 - link with libtivgm2 (-ltivgm2)
 - include TISNPlay.h to play back music on the SN PSG (standard console)
 - include TISIDPlay.h to play back music on the SID Blaster
 - include TISfxPlay.h to play back sound effects on the SN PSG 
    - this implies you are also playing music - if you are ONLY playing sound effects, just use TISNPlay.h
 - calling syntax is detailed below

FROM ASM:
 - you need to REF any symbol you want to access
 - you're pretty much on your own, but details are below for each function

---------------------------------
INTERMEDIATE: available functions
---------------------------------

SN Playback: Header: TISNPlay.h
-------------------------------
StartSong(const unsigned char *pSbf, uWordSize songNum)
    File:    CPlayerTIHandlers.asm
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
    Purpose: Initialize the player to begin playing a new song. Invalid data will probably just crash.
    C okay?: Yes, uWordSize is an unsigned char.
    Asm?   : pSbf in r0, songNum is MSB of r1
             no return value.
             corrupts r2,r3,r4,r11. All data structures will be initialized.

StopSong()
    File:    CPlayerTIHandlers.asm
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing song. Does NOT mute the sound chip.
    C okay?: Yes.
    Asm?   : no input values.
             no return value.
             corrupts r2,r11. Modifies songActive and songVol.

SongLoop()
    File:    CPlayerTIHandEdit.asm
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.
    C okay?: ** No ** Use CALL_PLAYER_SN macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.
    Note   : Supports ForTI - load sound chip address in R8 and BL Song2Lp() instead

SongLoop30()
    File:    CPlayerTI30Hz.asm
    Return:  void
    Inputs:  none
    Purpose: Run one half of one frame of the current song. Returns quickly if no song is active.
    C okay?: ** No ** Use CALL_PLAYER_SN30 macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.
    Note   : Supports ForTI - load sound chip address in R8 and BL Song2Lp30() instead

isSNPlaying
    File:    TISNPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is SN player is active. Wrapper macro for SongActive byte.
    C okay?: Yes.
    Asm?   : Not available. Check the SongActive byte as defined below.

CALL_PLAYER_SN
    File:    TISNPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to safely call SongLoop from a C program
    C okay?: Yes.
    Asm?   : Not available. Call SongLoop directly or as noted below.


SID Playback: Header: TISIDPlay.h
-------------------------------
StartSID(const unsigned char *pSbf, uWordSize songNum)
    File:    CSIDPlayTIHandlers.asm
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
    Purpose: Initialize the player to begin playing a new song. Invalid data will probably just crash.
    C okay?: Yes, uWordSize is an unsigned char. Before calling, set SidCtrl1, SidCtrl2, SidCtrl3 as below.
    Asm?   : pSbf in r0, songNum is MSB of r1
             no return value.
             corrupts r2,r3,r4,r5,r11,r12. All data structures will be initialized. All SID registers are initialized.

StopSID()
    File:    CSIDPlayTIHandlers.asm
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing song. Does NOT mute the sound chip.
    C okay?: Yes.
    Asm?   : no input values.
             no return value.
             corrupts r11. Modifies songActive and songVol.

SIDLoop()
    File:    CSIDPlayTIHandEdit.asm
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.
    C okay?: ** No **. Use CALL_PLAYER_SID macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.

SIDLoop30()
    File:    CSIDPlayTI30Hz.asm
    Return:  void
    Inputs:  none
    Purpose: Run one half of one frame of the current song. Returns quickly if no song is active.
    C okay?: ** No **. Use CALL_PLAYER_SID30 macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.

isSIDPlaying
    File:    TISIDPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is SID player is active. Wrapper macro for SongActive byte.
    C okay?: Yes.
    Asm?   : Not available. Check the SongActive byte as defined below.

CALL_PLAYER_SID
    File:    TISIDPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to safely call SIDLoop from a C program
    C okay?: Yes.
    Asm?   : Not available. Call SIDLoop directly or as noted below.


SFX Playback: Header: TISfxPlay.h
---------------------------------
StartSfx(const unsigned char *pSbf, uWordSize songNum, uWordSize pri)
    File:    CPlayerTISfxHandlers.asm
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
             pri - 8-bit priority of the SFX. If equal to or lower than currently playing SFX, returns without starting the new one. Otherwise, the new SFX will be started.
    Purpose: Initialize the player to begin playing a new SFX. Invalid data will probably just crash.
    C okay?: Yes, uWordSize is an unsigned char.
    Asm?   : pSbf in r0, songNum is MSB of r1, pri in MSB of r2
             no return value.
             corrupts r2,r3,r4,r5,r11,r12. All data structures will be initialized. All SID registers are initialized.

StopSfx()
    File:    CPlayerTISfxHandlers.asm
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing SFX. Restores the current SN song state on all modified channels.
    C okay?: Yes.
    Asm?   : no input values.
             no return value.
             corrupts r0,r1,r2,r11. Modifies sfxActive.

SfxLoop()
    File:    CPlayerTISfx.asm
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.
    Note:    For proper use, call before SongLoop()
    C okay?: ** No **. Use CALL_PLAYER_SFX macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.
    Note   : Supports ForTI - load sound chip address in R8 and BL Sfx2Lp() instead

SfxLoop30()
    File:    CPlayerTISfx30hz.asm
    Return:  void
    Inputs:  none
    Purpose: Run one half of one frame of the current song. Returns quickly if no song is active.
    Note:    For proper use, call before SongLoop()
    C okay?: ** No **. Use CALL_PLAYER_SFX30 macro.
    Asm?   : no input values.
             no return value.
             corrupts all registers but r10. Modifies all data structures.
    Note   : Supports ForTI - load sound chip address in R8 and BL Song2Lp30() instead

isSFXPlaying
    File:    TISfxPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is SFX player is active. Wrapper macro for SongActive byte.
    C okay?: Yes.
    Asm?   : Not available. Check the SongActive byte as defined below.

CALL_PLAYER_SFX
    File:    TISfxPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to safely call SfxLoop from a C program.
    Note:    For proper use, call before CALL_PLAYER_SN
    C okay?: Yes.
    Asm?   : Not available. Call SfxLoop directly or as noted below.


Unit test: Header: none
-----------------------

PlayerUnitTest()
    File:    CPlayer/CPlayerTest.c
    Return:  void
    Inputs:  none
    Purpose: Run a set of pre-defined tests against getCompressedByte
    Note:    Provide your own extern reference. You MUST link with libti99 or otherwise provide printf().
    C okay?: Yes.
    Asm?   : Maybe? Not verified or analyzed.


--------------------------------
PROFESSIONAL: Direct Data access
--------------------------------

There are a few public variables you can access to determine the status of the song playback. You normally should not change any of this data - although for the AY and SN only, you are permitted to clear the most significant nibble of the note and volume variables if you want to use the data as a flag for changed data. However, this is only true if you are NOT also using the SFX player. The application for doing so is fairly small.

songVol[4]
    Header:  TISNPlay.h
    Size:    BYTE
    Detail:  Most significant nibble: SN command bits, Least significant nibble: volume attenuation.
    Purpose: Contains the last volume byte written to each channel by the SN playback
    Note:    The SFX player uses this value to restore volumes when an SFX ends

songNote[4]
    Header:  TISNPlay.h
    Size:    WORD
    Detail:  Most significant nibble: SN command bits, remainder: tone counter bits in SN order
    Purpose: Contains the last frequency word written to each channel by the SN playback
    Note:    1. The SFX player uses this value to restore frequencies when an SFX ends
             2. songNote[3] contains the noise type in the most significant byte, and songActive in the least
             3. The SongActive byte defines these bits:
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
                SONGACTIVEHALF   0x02   30hz player is on the second half of a frame
                SONGACTIVEMUTE1  0x80   Voice 0 is being muted (externally set)
                SONGACTIVEMUTE2  0x40   Voice 1 is being muted (externally set)
                SONGACTIVEMUTE3  0x20   Voice 2 is being muted (externally set)
                SONGACTIVEMUTE4  0x10   Voice 3 is being muted (externally set)

sidVol[4]
    Header:  TISIDPlay.h
    Size:    BYTE
    Detail:  Most significant nibble: volume level (opposite to SN), least significant nibble: 0
    Purpose: Contains the last volume byte written to each channel's sustain register

sidNote[4]
    Header:  TISIDPlay.h
    Size:    WORD
    Detail:  Most significant byte: LO frequency, Least significant byte: HI frequency
    Purpose: Contains the last frequency word written to each channel
    Note:    1. songNote[3] contains songActive in the MSB, and the LSB is unused
             2. The SongActive byte defines these bits
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
                SONGACTIVEHALF   0x02   30hz player is on the second half of a frame
                SONGACTIVEMUTE1  0x80   Voice 0 is being muted (externally set)
                SONGACTIVEMUTE2  0x40   Voice 1 is being muted (externally set)
                SONGACTIVEMUTE3  0x20   Voice 2 is being muted (externally set)

SidCtrl1,SidCtrl2,SidCtrl3
    Header:  TISIDPlay.h
    Size:    BYTE
    Detail:  Contains the control register for each voice, used during gate toggle operations.
    Purpose: Sets each channel waveform so that they are correct for the song.
    Note:    1. Normally these are set before calling StartSID.
             2. These are re-written each time the gate is toggled.
             3. The following patterns are predefined:
                SID_TONE_NOISE  0x80    Play noise on this channel
                SID_TONE_PULSE  0x40    Play pulse (square wave) on this channel
                SID_TONE_SAW    0x20    Play sawtooth on this channel
                SID_TONE_TRI    0x10    Play triangle on this channel

sfxActive
    Header:  TISfxPlay.h
    Size:    WORD
    Detail:  Most significant byte: SongActive, least significant byte: current priority (if playing)
    Purpose: Contains the SongActive bits and the SFX priority value
    Note:    1. The SFX player /sets/ mute bits instead of muting on them
             2. On successful play, the SFX player copies its mute bits to the SN player's mutes
             3. The SongActive byte defines these bits:
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
                SONGACTIVEHALF   0x02   30hz player is on the second half of a frame
                SONGACTIVEMUTE1  0x80   Voice 0 is being muted (set by SFX)
                SONGACTIVEMUTE2  0x40   Voice 1 is being muted (set by SFX)
                SONGACTIVEMUTE3  0x20   Voice 2 is being muted (set by SFX)
                SONGACTIVEMUTE4  0x10   Voice 3 is being muted (set by SFX)

The internal data type is called a STREAM, and there is one structure for each channel. The layout looks like this:
typedef struct STRTYPE STREAM;
struct STRTYPE {
    uint8 *curPtr;                      // where are are currently getting data from
    uint8 *mainPtr;                     // the main index in the decompression. If 0, we are done.
    uint8 (*curType)(STREAM*, uint8*);  // function pointer to get data for the type
    uWordSize curBytes;                 // current bytes left
    uWordSize framesLeft;               // number of frames left on the current RLE (not used for tone channels)
};
uint8 is a BYTE, uWordSize is /also/ 8 bits. The function pointer points to any of a half-dozen functions tailored to extracting a particular type of byte from a stream.

strDat
    Header:  None
    Size:    STREAM * 9
    Detail:  Contains 9 stream structures - first 4 are tone, next 4 are volume, last is timestream
    Purpose: Contains the data for decompressing each stream for SN player

sidDat
    Header:  None
    Size:    STREAM * 9
    Detail:  Contains 9 stream structures - first 4 are tone, next 4 are volume, last is timestream
    Purpose: Contains the data for decompressing each stream for SID player

sfxDat
    Header:  None
    Size:    STREAM * 9
    Detail:  Contains 9 stream structures - first 4 are tone, next 4 are volume, last is timestream
    Purpose: Contains the data for decompressing each stream for SFX player

workBuf
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for SN

workSID
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for SID

sfxWorkBuf
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for SFX


-------------------------------------------
ADVANCED: Using the console interrupt hook
-------------------------------------------

The system is currently built to be called manually from whichever approach you choose. If you prefer to call from the user interrupt hook, however, you will need to build a small assembly function to wrap the main call. If the player uses the GPLWS, the console will crash on return, so you need to use any other workspace, then go back to the original before returning (note that as written, the player expects >8300 to be its workspace):

    DEF INTCALL
INTCALL
    LWPI >8300      * change to our own workspace (can be anywhere)
    BL @SONGLOOP    * call the song processing
    LWPI >83E0      * restore GPL Workspace
    RT              * back to console

When you are ready to configure it, just load it into 0x83C4:
    LI R0,INTCALL   * address of interrupt hook function
    MOV R0,@>83C4   * load it into the console interrupt hook

Then, in your main loop, enable interrupts periodically as normal:
    LIMI 2          * enable interrupts - if ready, the console routine will run
    LIMI 0          * disable interrupts until next time we're ready to check

If your program can exit, before exitting you should clear the console hook, the ROM has bugs that can cause it to keep going or even crash.
    CLR @>83C4

If you are setting this up from C, I strongly recommend you use a different workspace from >8300, otherwise the C workspace will be corrupted and C won't be expecting it. Alternately, you could use a different version of the interrupt poll function that tells GCC registers are going to go away:

#define VDP_INT_POLL_SAFE   \
    __asm__(                            \
        "limi 2\n\t"                    \
        "limi 0"                        \
        : /* no outputs */              \
        : /* no arguments */            \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

There must be NO spaces after the backslashes at the end of the line! Call this where you want your C code to do a standard "LIMI 2/LIMI 0" poll, and GCC will know that it's possible it might lose any register from this. Note R10 is not listed deliberately - the code preserves the stack pointer at least!

Since a C implementation also needs the INTCALL function above, a convenient place to stop that is in your crt0 assembly file.

Once you have those, your C code can activate the hook with:
    *((volatile void**)0x83c4) = INTCALL;

or, if you have libti99 with vdp.h:
    VDP_INT_HOOK = INTCALL;

You should clear the hook back to 0 before you exit:
    *((volatile void**)0x83c4) = 0;
or
    VDP_INT_HOOK = 0;


--------------------------------
ADVANCED: Stream unpack function
--------------------------------

There is a single shared function used for unpacking all the data by all the players.

getCompressedByte(STREAM *str, const uint8 *buf);
    Header:  multiple
    File:    CPlayerCommonHandEdit.asm
    Return:  BYTE
    Inputs:  str - pointer to the stream object being decoded
             buf - pointer to the root of the song file (optional in most case)
    Purpose: extract the 'next' byte from a stream. If the stream ends, str->mainPtr is set to 0
    C okay?: No, the calling syntax is not compatible. See below.
    Asm?   : str in r15, buf is not used, r6 must be set to 0x0100
             return in r1 MSB. r2 will be zeroed if stream ended, as well as str->mainPtr
             corrupts r2,r3,r4,r5,r11. r15 is not modified but structure at r15 is.

If you want to be able to call this function from C, the following wrapper is used successfully in CPlayerTest.c:

uint8 __attribute__ ((noinline)) getCompressedByteWrap(STREAM *str, const uint8 *buf) {
    __asm__(                                                        \
        "mov r1,r15\n\t"                                            \
        "dect r10\n\t"                                              \
        "mov r11,*r10\n\t"                                          \
        "li r6,>0100\n\t"                                           \
        "bl @getCompressedByte\n\t"                                 \
        "mov *r10+,r11\n"                                           \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r12","r13,","r14","r15","cc"   \
        );
}

Then call getCompressedByteWrap() instead to use it.

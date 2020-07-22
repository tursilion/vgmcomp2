This links together the Coleco playback code into a single linkable library. Link with -lcolecovgm2

There are three players involved. The SN and AY players will standalone, but the SFX player will also bring in SN. (There is no AY SFX player at the moment). All players will share the common decompression code.

-----------------------
BASIC: getting it setup
-----------------------

FROM C:
 - tested with SDCC 4.0.0
 - link with libcolecovgm2 (-lcolecovgm2)
 - include ColecoSNPlay.h to play back music on the SN PSG (standard console)
 - include ColecoAYPlay.h to play back music on the Phoenix AY
 - include ColecoSfxPlay.h to play back sound effects on the SN PSG 
    - this implies you are also playing music - if you are ONLY playing sound effects, just use ColecoSNPlay.h
 - calling syntax is detailed below

FROM ASM:
 - you're pretty much on your own, but details are below for each function

---------------------------------
INTERMEDIATE: available functions
---------------------------------

SN Playback: Header: ColecoSNPlay.h
-------------------------------
StartSong(const unsigned char *pSbf, uWordSize songNum)
    File:    CPlayer.c
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
    Purpose: Initialize the player to begin playing a new song. Invalid data will probably just crash.

StopSong()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing song. Does NOT mute the sound chip.

SongLoop()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.

isSNPlaying
    File:    ColecoSNPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is SN player is active. Wrapper macro for SongActive byte.

CALL_PLAYER_SN
    File:    ColecoSNPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to call SongLoop from a C program (for compatibility with TI version)


AY Playback: Header: ColecoAYPlay.h
-------------------------------
ay_StartSong(const unsigned char *pSbf, uWordSize songNum)
    File:    CPlayer.c
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
    Purpose: Initialize the player to begin playing a new song. Invalid data will probably just crash.

ay_StopSong()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing song. Does NOT mute the sound chip.

ay_SongLoop()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.

isAYPlaying
    File:    ColecoAYPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is AY player is active. Wrapper macro for SongActive byte.

CALL_PLAYER_AY
    File:    ColecoAYPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to call AYLoop from a C program (for compatibility with TI code)


SFX Playback: Header: ColecoSfxPlay.h
---------------------------------
sfx_StartSong(unsigned char *pSbf, uWordSize songNum, uWordSize pri)
    File:    CPlayer.c
    Return:  void
    Inputs:  pSbf - pointer to the song bank being started
             songNum - index of the song, starting with 0, to play. Note: no range checking!
             pri - 8-bit priority of the SFX. If equal to or lower than currently playing SFX, returns without starting the new one. Otherwise, the new SFX will be started.
    Purpose: Initialize the player to begin playing a new SFX. Invalid data will probably just crash.

sfx_StopSong()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Stop any currently playing SFX. Restores the current SN song state on all modified channels.

sfx_SongLoop()
    File:    CPlayer.c
    Return:  void
    Inputs:  none
    Purpose: Run one frame of the current song. Returns quickly if no song is active.
    Note:    For proper use, call before SongLoop()
    Note:    Recommend use CALL_PLAYER_SFX macro, otherwise mutes will not be cleared correctly.

isSFXPlaying
    File:    ColecoSfxPlay.h
    Return:  bool
    Inputs:  none
    Purpose: returns true is SFX player is active. Wrapper macro for SongActive byte.

CALL_PLAYER_SFX
    File:    ColecoSfxPlay.h
    Return:  none
    Inputs:  none
    Purpose: Wrapper macro to call SfxLoop from a C program (recommended)
    Note:    For proper use, call before CALL_PLAYER_SN


Unit test: Header: none
-----------------------

PlayerUnitTest()
    File:    CPlayer/CPlayerTest.c
    Return:  void
    Inputs:  none
    Purpose: Run a set of pre-defined tests against getCompressedByte
    Note:    Provide your own extern reference. You MUST link with libti99 or otherwise provide printf().


--------------------------------
PROFESSIONAL: Direct Data access
--------------------------------

There are a few public variables you can access to determine the status of the song playback. You normally should not change any of this data - although for the AY and SN only, you are permitted to clear the most significant nibble of the note and volume variables if you want to use the data as a flag for changed data. However, this is only true if you are NOT also using the SFX player. The application for doing so is fairly small.

songVol[4]
    Header:  ColecoSNPlay.h
    Size:    BYTE
    Detail:  Most significant nibble: SN command bits, Least significant nibble: volume attenuation.
    Purpose: Contains the last volume byte written to each channel by the SN playback
    Note:    The SFX player uses this value to restore volumes when an SFX ends

songNote[4]
    Header:  ColecoSNPlay.h
    Size:    WORD
    Detail:  Most significant nibble: SN command bits, remainder: tone counter bits in SN order
    Purpose: Contains the last frequency word written to each channel by the SN playback
    Note:    1. The SFX player uses this value to restore frequencies when an SFX ends
             2. songNote[3] contains the noise type in the most significant bit, and songActive in the least
             3. The SongActive byte defines these bits:
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
                SONGACTIVEMUTE1  0x80   Voice 0 is being muted (externally set)
                SONGACTIVEMUTE2  0x40   Voice 1 is being muted (externally set)
                SONGACTIVEMUTE3  0x20   Voice 2 is being muted (externally set)
                SONGACTIVEMUTE4  0x10   Voice 3 is being muted (externally set)

ay_songVol[4]
    Header:  ColecoAYPlay.h
    Size:    BYTE
    Detail:  Most significant nibble: volume level (opposite to SN), least significant nibble: 0
    Purpose: Contains the last volume byte written to each channel's sustain register

ay_songNote[4]
    Header:  ColecoAYPlay.h
    Size:    WORD
    Detail:  Most significant byte: LO frequency, Least significant byte: HI frequency
    Purpose: Contains the last frequency word written to each channel
    Note:    1. songNote[3] contains songActive in the MSB, and the LSB is unused
             2. The SongActive byte defines these bits
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
                SONGACTIVEMUTE1  0x80   Voice 0 is being muted (externally set)
                SONGACTIVEMUTE2  0x40   Voice 1 is being muted (externally set)
                SONGACTIVEMUTE3  0x20   Voice 2 is being muted (externally set)

sfx_songActive
    Header:  ColecoSfxPlay.h
    Size:    WORD
    Detail:  Most significant byte: SongActive, least significant byte: current priority (if playing)
    Purpose: Contains the SongActive bits and the SFX priority value
    Note:    1. The SFX player /sets/ mute bits instead of muting on them
             2. On successful play, the SFX player copies its mute bits to the SN player's mutes
             3. The SongActive byte defines these bits:
                SONGACTIVEACTIVE 0x01   Song is currently playing (active)
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

ay_strDat
    Header:  None
    Size:    STREAM * 9
    Detail:  Contains 9 stream structures - first 4 are tone, next 4 are volume, last is timestream
    Purpose: Contains the data for decompressing each stream for SID player

sfx_strDat
    Header:  None
    Size:    STREAM * 9
    Detail:  Contains 9 stream structures - first 4 are tone, next 4 are volume, last is timestream
    Purpose: Contains the data for decompressing each stream for SFX player

workBuf
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for SN

ay_workBuf
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for AY

sfx_workBuf
    Header:  None
    Size:    WORD
    Purpose: Contains the address of the currently playing song for SFX


--------------------------------
ADVANCED: Stream unpack function
--------------------------------

There is a single shared function used for unpacking all the data by all the players.

getCompressedByte(STREAM *str, uint8 *buf);
    Header:  multiple
    File:    CPlayerCommon.c
    Return:  BYTE
    Inputs:  str - pointer to the stream object being decoded
             buf - pointer to the root of the song file (optional in most case)
    Purpose: extract the 'next' byte from a stream. If the stream ends, str->mainPtr is set to 0

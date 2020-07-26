// Player for SBF files
// Plays one song - use two players for sound effects (they aren't that big!)
// You can also use multiple players to support simultaneous PSG and AY
// Note there is minimal error checking in here, as this is meant for
// performance on low end micros. Make sure your song data is valid
// or it will probably crash.

// rather than building directly, create a wrapper file that provides
// USE_xx_PSG, BUILD_xxx, WRITE_BYTE_TO_SOUND_CHIP, and optionally SONG_PREFIX,
// then include this file in that one.

// You do need to build CPlayerCommon.c separately, though it only needs BUILD_xxx.

// In detail:

// set this define (externally) to give the data and functions
// unique prefixes. This is helpful for multiple songs playing, 
// such as when you need to mix PSG and AY together, or sound effects.
// Set the define and build CPlayer.c for each one. This
// uses a little extra code, but on the TI and the Z80 both,
// absolute data is far more efficient than data behind a
// pointer variable.
//#define SONG_PREFIX sfx_

// enable exactly one of these three defines - affects the tone table and
// the noise channel slightly
//#define USE_SN_PSG
//#define USE_AY_PSG
//#define USE_SID_PSG

// enable exactly one of these targets - affects data types and
// any specific build deviancies
//#define BUILD_TI99
//#define BUILD_COLECO
//#define BUILD_PC

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
// TODO: an optimization might be to group the two tone and the volume
// writes under only one test against muted channel.

#include "CPlayer.h"

#if !defined(USE_SN_PSG) && !defined(USE_AY_PSG) & !defined(USE_SID_PSG)
#error Define USE_SN_PSG or USE_AY_PSG or USE_SID_PSG
#endif
#if defined(USE_SN_PSG) && defined(USE_SID_PSG)
#error Define USE_SN_PSG **OR** USE_AY_PSG **OR** USE_SID_PSG
#endif
#if defined(USE_SN_PSG) && defined(USE_AY_PSG)
#error Define USE_SN_PSG **OR** USE_AY_PSG **OR** USE_SID_PSG
#endif
#if defined(USE_AY_PSG) && defined(USE_SID_PSG)
#error Define USE_SN_PSG **OR** USE_AY_PSG **OR** USE_SID_PSG
#endif

#ifndef SONG_PREFIX
#define SONG_PREFIX
#endif

// double indirection trick needed for expansion of SONG_PREFIX
#define PASTER(x,y) x ## y
#define EVALUATOR(x,y) PASTER(x,y)
#define MAKE_FN_NAME(x) EVALUATOR(SONG_PREFIX,x)
#define StartSong MAKE_FN_NAME(StartSong)
#define StopSong  MAKE_FN_NAME(StopSong)
#define SongLoop  MAKE_FN_NAME(SongLoop)
#define songVol   MAKE_FN_NAME(songVol)
#define songNote  MAKE_FN_NAME(songNote)
#define workBufName MAKE_FN_NAME(workBuf)

// this array contains the current volume of each voice
// Sound chip specific, but in both cases that means 0x0 is maximum and 0xF
// is muted. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command in the least
// significant nibble. The most significant nibble, again, has a trigger
// flag - 0xe0 to load, 0x00 to ignore.
uint8 songVol[4];

// this array contains the current note on each voice
// sound chip specific. Noise channel contains data
// in the MSB, and the LSB is used for songActive
// and mute flags.
uint16 songNote[4];

// this holds onto the currently playing song pointer
const uint8* workBufName;

// local stream data
// 4 tone, 4 vol, 1 time
static STREAM strDat[9];

// buf - pointer to sbf buffer
// y - tone to look up
static inline uint16 tonetable(uWordSize y) {
    // lookup from buf, or look up from variable, it amounts to the same code
    uint16 toneoffset = (workBufName[2]<<8) + workBufName[3];
#ifdef USE_AY_PSG
    // note that I am flipping the byte order here so that the trigger
    // nibble is in the MSB returned. This means the first byte (nibble) is
    // actually for the SECOND register.
    //             LSB, coarse reg, 1st - MSB, fine reg, 2nd (4 bits)
    return (uint16)workBufName[toneoffset+y*2] + ((workBufName[toneoffset+y*2+1]&0xf)<<8);
#endif
#ifdef USE_SN_PSG
    //             MSB, fine tune, 1st (4 bits)     LSB, coarse tune, 2nd
    return ((uint16)(workBufName[toneoffset+y*2]&0xf)<<8) + (workBufName[toneoffset+y*2+1]);
#endif
#ifdef USE_SID_PSG
    // Note I am flipping the order here to properly read the little endian data
    // 16-bit LE     MSB, Freq HI, 8 bits              LSB, Freq LO, 8 bits
    return ((uint16)(workBufName[toneoffset+y*2+1])<<8) + (workBufName[toneoffset+y*2]);
#endif
}

static uint8 getDatZero(STREAM *str, const uint8 *buf) {
    (void)str;
    (void)buf;
    return 0;
}

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(const unsigned char *buf, uWordSize sbfsong) {
    // load all the initial pointers for a song and set songActive
    uint16 streamoffset=((uint16)buf[0]<<8)+buf[1];

    // point to the first stream of 9
    streamoffset+=18*sbfsong;

    // load the stream pointers
    for (int idx=0; idx<9; ++idx) {
        int stream = buf[streamoffset+idx*2]*256+buf[streamoffset+idx*2+1];
        if (stream == 0) {
            strDat[idx].mainPtr = 0;
        } else {
            strDat[idx].mainPtr = buf + stream;
        }
        strDat[idx].curPtr = 0;
        strDat[idx].curBytes = 0;
        strDat[idx].curType = getDatZero;
        strDat[idx].framesLeft = 0;
    }

    // default settings
    for (int idx=0; idx<4; ++idx) {
        songNote[idx] = 1;
#ifdef USE_SN_PSG
        songVol[idx] = (0x9F+idx*0x20);
#else
        songVol[idx] = 0;
#endif
    }
	// fix noise tone so LSB is set up for song active
    // note that we also clear the mute flags, on purpose!
    songNote[3] = 0x0100 | SONGACTIVEACTIVE;
    workBufName = buf;
}

// Call this to stop the current song
void StopSong() {
    // also clears the mute bits
    songNote[3] &= 0xff00;
}

// this needs to be called 60 times per second by your system
// if this function can be interrupted, don't manipulate songActive
// in any function that can do so, or your change will be lost.
// the songActive mute bits are NOT honored here, they should be
// managed in your WRITE_BYTE_TO_SOUND_CHIP macro - this way if
// you don't need mutes, you don't need to process for them.
void SongLoop() {
    uint8 x,y;
    uWordSize str;
    uWordSize outSongActive;

    if (!(songNote[3]&SONGACTIVEACTIVE)) return;

    // assume false unless something needs processing
    outSongActive = false;

    if (strDat[8].mainPtr != 0) {
        // check the timestream
        if (strDat[8].framesLeft) {
            --strDat[8].framesLeft;
            outSongActive = true;
        } else {
            // timestream data
            x = getCompressedByte(&strDat[8], workBufName);
            if (strDat[8].mainPtr) {
                outSongActive = true;
                // song not over, x is valid
                strDat[8].framesLeft = x & 0xf;
                if (x&0x80) {
                    // voice 0
                    if (strDat[0].mainPtr) {
                        y = getCompressedByte(&strDat[0], workBufName);
                        if (strDat[0].mainPtr) {
                            // look up frequency table
                            songNote[0] = tonetable(y);
                        } else {
#ifdef USE_SN_PSG
                            songNote[0] = 0x0100;
#endif
#ifdef USE_AY_PSG
                            songNote[0] = 0x0001;
#endif
#ifdef USE_SID_PSG
                            songNote[0] = 0x0001;  // SID has no high frequency, but this may be low enough to be unnoticable?
#endif
                        }
#ifdef USE_SN_PSG
                        songNote[0] |= 0x8000;
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 0, songNote[0]&0xff);
#endif
#ifdef USE_SID_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 0, songNote[0]&0xff);
#endif
                    }
                }
                if (x&0x40) {
                    // voice 1
                    if (strDat[1].mainPtr) {
                        y = getCompressedByte(&strDat[1], workBufName);
                        if (strDat[1].mainPtr) {
                            // look up frequency table
                            songNote[1] = tonetable(y);
                        } else {
#ifdef USE_SN_PSG
                            songNote[1] = 0x0100;
#endif
#ifdef USE_AY_PSG
                            songNote[1] = 0x0001;
#endif
#ifdef USE_SID_PSG
                            songNote[1] = 0x0001;  // SID has no high frequency, but this may be low enough to be unnoticable?
#endif
                        }
#ifdef USE_SN_PSG
                        songNote[1] |= 0xa000;
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 2, songNote[1]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 2, songNote[1]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 3, songNote[1]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 2, songNote[1]&0xff);
#endif
#ifdef USE_SID_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 8, songNote[1]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 7, songNote[1]&0xff);
#endif
                    }
                }
                if (x&0x20) {
                    // voice 2
                    if (strDat[2].mainPtr) {
                        y = getCompressedByte(&strDat[2], workBufName);
                        if (strDat[2].mainPtr) {
                            // look up frequency table
                            songNote[2] = tonetable(y);
                        } else {
#ifdef USE_SN_PSG
                            songNote[2] = 0x0100;
#endif
#ifdef USE_AY_PSG
                            songNote[2] = 0x0001;
#endif
#ifdef USE_SID_PSG
                            songNote[2] = 0x0001;  // SID has no high frequency, but this may be low enough to be unnoticable?
#endif
                        }
#ifdef USE_SN_PSG
                        songNote[2] |= 0xc000;
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 3, songNote[2]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 3, songNote[2]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 5, songNote[2]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 4, songNote[2]&0xff);
#endif
#ifdef USE_SID_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 0xf, songNote[2]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 0xe, songNote[2]&0xff);
#endif
                    }
                }
#ifndef USE_SID_PSG
                if (x&0x10) {
                    // noise
                    if (strDat[3].mainPtr) {
                        y = getCompressedByte(&strDat[3], workBufName);
                        if (strDat[3].mainPtr) {
#ifdef USE_SN_PSG
                            // PSG - store command in frequency
                            // we set the command bits to indicate trigger
                            y |= 0xe0;
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 4, y);
                            // update the MSB only
                            songNote[3] = (songNote[3]&0xff) | (y<<8);
#endif
#ifdef USE_AY_PSG
                            // AY - frequency is frequency (still fits in a byte, but 5 bits!)
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 6, y);
                            // update the MSB only
                            songNote[3] = (songNote[3]&0xff) | (y<<8);
#endif
                        }   // no 'else' case on purpose, there's no usefully silent noise!
                    }
                }
#endif // not SID
            }
        }
    }

    // now handle the volume streams
#ifdef USE_SN_PSG
    unsigned char flag=0x90;
    for (str=4; str<8; ++str) {
#else
    for (str=4; str<7; ++str) {
#endif
        if (strDat[str].mainPtr != 0) {
            // check the RLE
            if (strDat[str].framesLeft) {
                --strDat[str].framesLeft;
                outSongActive = true;
            } else {
                x = getCompressedByte(&strDat[str], workBufName);
                if (strDat[str].mainPtr) {
                    outSongActive = true;
                    strDat[str].framesLeft = x&0xf;
                    x = ((x>>4)&0xf);
                } else {
#ifdef USE_SN_PSG
                    x = 0x0f;
#else
                    x = 0;
#endif
                }
#ifdef USE_SN_PSG
                x |= flag;
                WRITE_BYTE_TO_SOUND_CHIP(songNote[3], str-3, x);
                songVol[str-4] = x;
#endif
#ifdef USE_AY_PSG
                WRITE_BYTE_TO_SOUND_CHIP(songNote[3], str+4, x);
                songVol[str-4] = x;
#endif
#ifdef USE_SID_PSG
                // we CAN flag the trigger for SID though, though it's less useful on volume
                WRITE_BYTE_TO_SOUND_CHIP(songNote[3], ((str-4)*7+6), (x<<4));   // sustain register in top bits
                songVol[str-4] = x;
#endif
            }
        }
#ifdef USE_SN_PSG
        flag += 0x20;
#endif
    }

#ifdef USE_AY_PSG
    // special handling for noise channel (str == 7)
    if (strDat[7].mainPtr != 0) {
        // check the RLE
        if (strDat[7].framesLeft) {
            --strDat[7].framesLeft;
            outSongActive = true;
        } else {
            x = getCompressedByte(&strDat[7], workBufName);
            if (strDat[7].mainPtr) {
                outSongActive = true;
                strDat[7].framesLeft = x&0xf;
                x = ((x>>2)&0x3c);
            } else {
                x = 0x38;
            }
            // special case - write to mixer
            // bits go to noise mixer, all tones active
            // (just luck that stream 7 goes to reg 7)
            // this extra shift is a little miffy, we already shifted above...
            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 7, x);
            songVol[7-4] = x|0x80;
        }
    }
#endif

    if (!outSongActive) {
        songNote[3] &= 0xff00;     // clear the active bit AND clear all mutes
    }
}

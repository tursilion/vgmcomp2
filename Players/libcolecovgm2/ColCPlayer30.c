// Player for SBF files - 30Hz version
// This is an adaptation of CPlayerC specific for the ColecoVision
// The theory is that the Z80 is very fast at copying memory, and
// rather poor at accessing structures through pointers, so we are
// going to test copying streams into a fixed structure for processing
// and see if the CPU payoff is worth the extra RAM.
//
// Most of the optional stuff has been removed to make the code more readable.
// But you should still set the defines! BUILD_COLECO is the only assumed one.
//
// rather than building directly, create a wrapper file that provides
// USE_xx_PSG, WRITE_BYTE_TO_SOUND_CHIP, and optionally SONG_PREFIX,
// then include this file in that one.

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

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.

#include "ColCPlayer30.h"
#include <string.h>

#if !defined(USE_SN_PSG) && !defined(USE_AY_PSG)
#error Define USE_SN_PSG or USE_AY_PSG
#endif
#if defined(USE_SN_PSG) && defined(USE_AY_PSG)
#error Define USE_SN_PSG **OR** USE_AY_PSG
#endif

#ifndef SONG_PREFIX
#define SONG_PREFIX
#endif

// double indirection trick needed for expansion of SONG_PREFIX
#define PASTER(x,y) x ## y
#define EVALUATOR(x,y) PASTER(x,y)
#define MAKE_FN_NAME(x) EVALUATOR(SONG_PREFIX,x)
#define StartSong MAKE_FN_NAME(StartSong30)
#define StopSong  MAKE_FN_NAME(StopSong30)
#define SongLoop  MAKE_FN_NAME(SongLoop30)
#define songVol   MAKE_FN_NAME(songVol30)
#define songNote  MAKE_FN_NAME(songNote30)
#define workBufName MAKE_FN_NAME(workBuf30)

// this array contains the current volume of each voice
// Sound chip specific, but in both cases that means 0x0 is maximum and 0xF
// is muted. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command in the least
// significant nibble. The most significant nibble, again, has a trigger
// flag - 0xe0 to load, 0x00 to ignore.
volatile uint8 songVol[4];

// this array contains the current note on each voice
// sound chip specific. Noise channel contains data
// in the MSB, and the LSB is used for songActive
// and mute flags.
volatile uint16 songNote[4];

// this holds onto the currently playing song pointer
const uint8* workBufName;
// pointer to the tone table, calculating it each time is expensive
static const uint8* tonetableptr;

// local stream data
// 4 tone, 4 vol, 1 time
static STREAM strDat[9];

// buf - pointer to sbf buffer
// y - tone to look up
// much cheaper on Z80 to precalcuate toneoffset
static uint16 tonetable(uWordSize y) {
    const uint8 *pDat = tonetableptr + y*2;
#ifdef USE_AY_PSG
    // note that I am flipping the byte order here so that the trigger
    // nibble is in the MSB returned. This means the first byte (nibble) is
    // actually for the SECOND register.
    //             LSB, coarse reg, 1st - MSB, fine reg, 2nd (4 bits)
    return (uint16)(*pDat) | ((*(pDat+1)&0x0f)<<8);
#endif
#ifdef USE_SN_PSG
    //             MSB, fine tune, 1st (4 bits)     LSB, coarse tune, 2nd
    return ((uint16)((*pDat)&0x0f)<<8) | (*(pDat+1));
#endif
}

static uint8 getDatZero() {
    return 0;
}

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(const unsigned char *buf, uWordSize sbfsong) {
    // load all the initial pointers for a song and set songActive
    uint16 streamoffset=((uint16)buf[0]<<8)+buf[1];
    tonetableptr = buf + ((uint16)buf[2]<<8) + buf[3];

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
    static uint8 oldTimeStream;

    if (!(songNote[3]&SONGACTIVEACTIVE)) return;

    // assume false unless something needs processing
    outSongActive = false;

    // The noise channel complicates things slightly.
    // AY must process channel 1 (A) and noise (0x80 and 0x10) so they
    // happen in the same frame in case noise takes over A. The SN must
    // process channel 3 and noise (0x20 and 0x10) in the same frame.
    // So there's a bit of swishing around with ifdefs here...

    // absolute references SDCC can already figure out, but we do need
    // to copy it over for the getCompressedByte call
    if (strDat[8].mainPtr != 0) {
        // figure out what we are doing this frame
        if (songNote[3]&SONGACTIVEHALF) {
            // second half of processing frame
            x = oldTimeStream;

            // for AY, process voice A (0x80)
            // for SN, process voice 3 (0x20)
#ifdef USE_AY_PSG
            if (x&0x80) {
                // voice 0
                if (strDat[0].mainPtr) {
                    memcpy(&globalStr, &strDat[0], sizeof(STREAM));
                    y = getCompressedByteRaw();
                    memcpy(&strDat[0], &globalStr, sizeof(STREAM));
                    if (strDat[0].mainPtr) {
                        // look up frequency table
                        songNote[0] = tonetable(y);
                    } else {
                        songNote[0] = 0x0001;
                    }
                    WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]>>8);
                    WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 0, songNote[0]&0xff);
                }
            }
#endif
#ifdef USE_SN_PSG
            if (x&0x20) {
                // voice 2
                if (strDat[2].mainPtr) {
                    memcpy(&globalStr, &strDat[2], sizeof(STREAM));
                    y = getCompressedByteRaw();
                    memcpy(&strDat[2], &globalStr, sizeof(STREAM));
                    if (strDat[2].mainPtr) {
                        // look up frequency table
                        songNote[2] = tonetable(y);
                    } else {
                        songNote[2] = 0x0100;
                    }
                    songNote[2] |= 0xc000;
                    WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 3, songNote[2]>>8);
                    WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 3, songNote[2]&0xff);
                }
            }
#endif
            // and handle noise
            if (x&0x10) {
                // noise
                if (strDat[3].mainPtr) {
                    memcpy(&globalStr, &strDat[3], sizeof(STREAM));
                    y = getCompressedByteRaw();
                    memcpy(&strDat[3], &globalStr, sizeof(STREAM));
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
        } else {
            // first frame
            // check the timestream
            if (strDat[8].framesLeft) {
                --strDat[8].framesLeft;
                outSongActive = true;
            } else {
                // timestream data
                // AY processes channels 2 and 3, so 1 and noise can be done together
                // SN processes channels 1 and 2
                memcpy(&globalStr, &strDat[8], sizeof(STREAM));
                x = oldTimeStream = getCompressedByteRaw();
                memcpy(&strDat[8], &globalStr, sizeof(STREAM));
                if (strDat[8].mainPtr) {
                    outSongActive = true;
                    // song not over, x is valid
                    strDat[8].framesLeft = x & 0xf;
                    if (x&0x40) {
                        // voice 1
                        if (strDat[1].mainPtr) {
                            memcpy(&globalStr, &strDat[1], sizeof(STREAM));
                            y = getCompressedByteRaw();
                            memcpy(&strDat[1], &globalStr, sizeof(STREAM));
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
                        }
                    }
#ifdef USE_AY_PSG
                    if (x&0x20) {
                        // voice 2
                        if (strDat[2].mainPtr) {
                            memcpy(&globalStr, &strDat[2], sizeof(STREAM));
                            y = getCompressedByteRaw();
                            memcpy(&strDat[2], &globalStr, sizeof(STREAM));
                            if (strDat[2].mainPtr) {
                                // look up frequency table
                                songNote[2] = tonetable(y);
                            } else {
                                songNote[2] = 0x0001;
                            }
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 5, songNote[2]>>8);
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 4, songNote[2]&0xff);
                        }
                    }
#endif
#ifdef USE_SN_PSG
                    if (x&0x80) {
                        // voice 0
                        if (strDat[0].mainPtr) {
                            memcpy(&globalStr, &strDat[0], sizeof(STREAM));
                            y = getCompressedByteRaw();
                            memcpy(&strDat[0], &globalStr, sizeof(STREAM));
                            if (strDat[0].mainPtr) {
                                // look up frequency table
                                songNote[0] = tonetable(y);
                            } else {
                                songNote[0] = 0x0100;
                            }
                            songNote[0] |= 0x8000;
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]>>8);
                            WRITE_BYTE_TO_SOUND_CHIP(songNote[3], 1, songNote[0]&0xff);
                        }
                    }
#endif
                }
            }
        }
    }

    // now handle the volume streams
    // again, check which ones to process
    // AY: 56, 47
    // SN: 45, 67
    if (songNote[3]&SONGACTIVEHALF) {
        // second half - update songActive
        songNote[3] &= ~SONGACTIVEHALF;

#ifdef USE_SN_PSG
        STREAM *pStr = &strDat[6];
        unsigned char flag=0x90;
        for (str=6; str<8; ++str, ++pStr) {
#endif
#ifdef USE_AY_PSG
        STREAM *pStr = &strDat[4];
        // AY handles noise separately to avoid an inline compare
        str = 4; // no loop, just one channel
        {
#endif
            memcpy(&globalStr, pStr, sizeof(STREAM));
            if (globalStr.mainPtr != 0) {
                // check the RLE
                if (globalStr.framesLeft) {
                    --globalStr.framesLeft;
                    outSongActive = true;
                } else {
                    x = getCompressedByteRaw();
                    if (globalStr.mainPtr) {
                        outSongActive = true;
                        globalStr.framesLeft = x&0xf;
                        x = ((x>>4)&0xf);
                    } else {
#ifdef USE_SN_PSG
                        x = 0x0f;
#endif
#ifdef USE_AY_PSG
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
                }
            }
            memcpy(pStr, &globalStr, sizeof(STREAM));
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
                memcpy(&globalStr, &strDat[7], sizeof(STREAM));
                x = getCompressedByteRaw();
                memcpy(&strDat[7], &globalStr, sizeof(STREAM));
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
    } else {
        // first half - update songActive
        // AY: 56, 47
        // SN: 45, 67
        songNote[3] |= SONGACTIVEHALF;

    #ifdef USE_SN_PSG
        STREAM *pStr = &strDat[4];
        unsigned char flag=0x90;
        for (str=4; str<6; ++str, ++pStr) {
    #else
        STREAM *pStr = &strDat[5];
        for (str=5; str<7; ++str, ++pStr) {
    #endif
            memcpy(&globalStr, pStr, sizeof(STREAM));
            if (globalStr.mainPtr != 0) {
                // check the RLE
                if (globalStr.framesLeft) {
                    --globalStr.framesLeft;
                    outSongActive = true;
                } else {
                    x = getCompressedByteRaw();
                    if (globalStr.mainPtr) {
                        outSongActive = true;
                        globalStr.framesLeft = x&0xf;
                        x = ((x>>4)&0xf);
                    } else {
#ifdef USE_SN_PSG
                        x = 0x0f;
#endif
#ifdef USE_AY_PSG
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
                }
            }
            memcpy(pStr, &globalStr, sizeof(STREAM));
#ifdef USE_SN_PSG
            flag += 0x20;
#endif
        }
    }

    if (!outSongActive) {
        songNote[3] &= 0xff00;     // clear the active bit AND clear all mutes
    }
}

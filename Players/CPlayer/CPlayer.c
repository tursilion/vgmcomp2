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

// enable exactly one of these two defines - affects the tone table and
// the noise channel slightly
//#define USE_SN_PSG
//#define USE_AY_PSG

// enable exactly one of these targets - affects data types and
// any specific build deviancies
//#define BUILD_TI99
//#define BUILD_COLECO
//#define BUILD_PC

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. For
// the SN PSG, this takes a single argument which is the byte to write.
// For the AY PSG, this takes two arguments, the first is the index
// of the register to write to, the second is the byte to write. See
// the CPlayerXX.c files for examples.

#include <stdlib.h>
#include "CPlayer.h"

#if !defined(USE_SN_PSG) && !defined(USE_AY_PSG)
#error Define USE_SN_PSG or USE_AY_PSG
#endif
#if defined(USE_SN_PSG) && defined(USE_AY_PSG)
#error Define USE_SN_PSG **OR** USE_AY_PSG
#endif

#ifdef SONG_PREFIX
#define MAKE_FN_NAME(x) SONG_PREFIX ## x
#define StartSong MAKE_FN_NAME(StartSong)
#define StopSong  MAKE_FN_NAME(StopSong)
#define SongLoop  MAKE_FN_NAME(SongLoop)
#define songVol   MAKE_FN_NAME(songVol)
#define songNote  MAKE_FN_NAME(songNote)
#define songActive  MAKE_FN_NAME(songActive)
#endif

// this array contains the current volume of each voice
// Sound chip specific, but in both cases that means 0x0 is maximum and 0xF
// is muted. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command in the least
// significant nibble. The most significant nibble, again, has a trigger
// flag - 0xe0 to load, 0x00 to ignore.
uint8 songVol[4];

// this array contains the current note on each voice
// sound chip specific
uint16 songNote[4];

// this flag contains 1 if playing, zero if stopped
// you can also stop (or pause!) a song by setting it to zero
// it goes to zero when the timestream and all volume streams run out
uWordSize songActive;

// this holds onto the currently playing song pointer
static uint8* workBuf;

// local stream data
// 4 tone, 4 vol, 1 time
static STREAM strDat[9];

// convenience variables for table offsets
uint16 streamoffset;
uint16 toneoffset;

// buf - pointer to sbf buffer
// toneoffset - offset to tone table
// y - tone to look up
static inline uint16 tonetable(uint8 *buf, uint16 toneoffset, uWordSize y) {
#ifdef USE_AY_PSG
    // note that I am flipping the byte order here so that the trigger
    // nibble is in the MSB returned. This means the first byte (nibble) is
    // actually for the SECOND register.
    //             LSB, coarse reg, 1st - MSB, fine reg, 2nd (4 bits)
    return (uint16)buf[toneoffset+y*2] + ((buf[toneoffset+y*2+1]&0xf)<<8);
#endif
#ifdef USE_SN_PSG
    //             MSB, fine tune, 1st (4 bits)     LSB, coarse tune, 2nd
    return ((uint16)(buf[toneoffset+y*2]&0xf)<<8) + (buf[toneoffset+y*2+1]);
#endif
}

static uint8 getDatZero(STREAM *str, uint8 *buf) {
    (void)str;
    (void)buf;
    return 0;
}

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(unsigned char *buf, uWordSize sbfsong) {
    // load all the initial pointers for a song and set songActive
    streamoffset=((uint16)buf[0]<<8)+buf[1];
    toneoffset=((uint16)buf[2]<<8)+buf[3];

    // point to the first stream of 9
    streamoffset+=18*sbfsong;

    // load the stream pointers
    for (int idx=0; idx<9; ++idx) {
        strDat[idx].mainPtr = buf[streamoffset+idx*2]*256+buf[streamoffset+idx*2+1];
        strDat[idx].curPtr = 0;
        strDat[idx].curBytes = 0;
        strDat[idx].curType = getDatZero;
        strDat[idx].framesLeft = 0;
    }

    // default settings
    for (int idx=0; idx<4; ++idx) {
        songNote[idx] = 1;
        songVol[idx] = 0;
    }

    workBuf = buf;
    songActive = 1;
}

// Call this to stop the current song
void StopSong() {
    songActive = 0;
}

// this needs to be called 60 times per second by your system
// if this function can be interrupted, don't manipulate songActive
// in any function that can do so, or your change will be lost.
void SongLoop() {
    uint8 x,y;
    uWordSize str;

    if (!songActive) return;

    // assume false unless something needs processing
    songActive = false;

    if (strDat[8].mainPtr != 0) {
        // check the timestream
        if (strDat[8].framesLeft) {
            --strDat[8].framesLeft;
            songActive = true;
        } else {
            // timestream data
            x = getCompressedByte(&strDat[8], workBuf);
            if (strDat[8].mainPtr) {
                songActive = true;
                // song not over, x is valid
                strDat[8].framesLeft = x & 0xf;
                if (x&0x80) {
                    // voice 0
                    if (strDat[0].mainPtr) {
                        y = getCompressedByte(&strDat[0], workBuf);
                        if (strDat[0].mainPtr) {
                            // look up frequency table
                            songNote[0] = tonetable(workBuf, toneoffset, y);
                        } else {
#ifdef USE_SN_PSG
                            songNote[0] = 0x0100;
#endif
#ifdef USE_AY_PSG
                            songNote[0] = 0x0001;
#endif

                        }
#ifdef USE_SN_PSG
                        songNote[0] |= 0x8000;
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[0]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[0]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(1, songNote[0]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(0, songNote[0]&0xff);
                        songNote[0] |= 0x8000;
#endif
                    }
                }
                if (x&0x40) {
                    // voice 1
                    if (strDat[1].mainPtr) {
                        y = getCompressedByte(&strDat[1], workBuf);
                        if (strDat[1].mainPtr) {
                            // look up frequency table
                            songNote[1] = tonetable(workBuf, toneoffset, y);
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
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[1]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[1]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(3, songNote[1]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(2, songNote[1]&0xff);
                        songNote[1] |= 0xa000;
#endif
                    }
                }
                if (x&0x20) {
                    // voice 2
                    if (strDat[2].mainPtr) {
                        y = getCompressedByte(&strDat[2], workBuf);
                        if (strDat[2].mainPtr) {
                            // look up frequency table
                            songNote[2] = tonetable(workBuf, toneoffset, y);
                        } else {
#ifdef USE_SN_PSG
                            songNote[2] = 0x0100;
#endif
#ifdef USE_AY_PSG
                            songNote[2] = 0x0001;
#endif
                        }
#ifdef USE_SN_PSG
                        songNote[2] |= 0xc000;
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[2]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(songNote[2]&0xff);
#endif
#ifdef USE_AY_PSG
                        WRITE_BYTE_TO_SOUND_CHIP(5, songNote[2]>>8);
                        WRITE_BYTE_TO_SOUND_CHIP(4, songNote[2]&0xff);
                        songNote[2] |= 0xc000;
#endif
                    }
                }
                if (x&0x10) {
                    // noise
                    if (strDat[3].mainPtr) {
                        y = getCompressedByte(&strDat[3], workBuf);
                        if (strDat[3].mainPtr) {
#ifdef USE_SN_PSG
                            // PSG - store command in frequency
                            // we set the command bits to indicate trigger
                            y |= 0xe0;
                            WRITE_BYTE_TO_SOUND_CHIP(y);
                            songNote[3] = y;
#endif
#ifdef USE_AY_PSG
                            // AY - frequency is frequency (still fits in a byte, but 5 bits!)
                            WRITE_BYTE_TO_SOUND_CHIP(6, y);
                            songNote[3] = y | 0xE0;
#endif
                        }   // no 'else' case on purpose, there's no usefully silent noise!
                    }
                }
            }
        }
    }

    // now handle the volume streams
    unsigned char flag=0x90;
    for (str=4; str<8; ++str) {
        if (strDat[str].mainPtr != 0) {
            // check the RLE
            if (strDat[str].framesLeft) {
                --strDat[str].framesLeft;
                songActive = true;
            } else {
                x = getCompressedByte(&strDat[str], workBuf);
                if (strDat[str].mainPtr) {
                    songActive = true;
                    strDat[str].framesLeft = x&0xf;
                    x = ((x>>4)&0xf);
                } else {
                    x = 0x0f;
                }
#ifdef USE_SN_PSG
                x |= flag;
                WRITE_BYTE_TO_SOUND_CHIP(x);
                songVol[str-4] = x;
#endif
#ifdef USE_AY_PSG
                if (str == 7) {
                    // special case - write to mixer
                    // bits go to noise mixer, all tones active
                    // (just luck that stream 7 goes to reg 7)
                    WRITE_BYTE_TO_SOUND_CHIP(7, (x<<2)&0x3C);
                } else {
                    WRITE_BYTE_TO_SOUND_CHIP(str+4, x);
                }
                songVol[str-4] = x | flag;
#endif
            }
        }
        flag += 0x20;
    }
}

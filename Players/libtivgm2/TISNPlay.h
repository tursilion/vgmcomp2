#ifndef INCLUDE_TISNPLAY_H
#define INCLUDE_TISNPLAY_H

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

#ifndef DEFINED_TI_TYPES
#define DEFINED_TI_TYPES
typedef int int16;              // must be 16 bit or larger, signed is okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned char uWordSize;// most efficient word size, 8 bits or more unsigned
#endif

// structure for unpacking a stream of data
typedef struct STRTYPE STREAM;
struct STRTYPE {
    uint8 *curPtr;       // where are are currently getting data from
    uint8 *mainPtr;      // the main index in the decompression. If 0, we are done.
    uint8 (*curType)(STREAM*, uint8*);    // function pointer to get data for the type
    uWordSize curBytes;   // current bytes left
    // post compression data
    uWordSize framesLeft; // number of frames left on the current RLE (not used for tone channels)
};

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void StopSong();

// this needs to be called 60 times per second by your system
void SongLoop();

// Don't call this, it's for use by the unpack codes
uint8 getCompressedByte(STREAM *str, uint8 *buf);

// this array contains the current volume of each voice (ignoring mutes)
// Sound chip specific, but in both cases that means 0x0 is maximum and 0xF
// is muted. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command. if it is 0xff
// then no channels are playing noise.
// You, the caller, need to strip the trigger nibble when you are done
// (if it matters to your software, that is)
extern uint8 songVol[4];

// this array contains the current note on each voice (ignoring mutes)
// The most significant nibble is set to the PSG command nibble when a
// new trigger has occurred. 
// Note that these command nibbles are not meaningful to the AY PSG
// You, the caller, need to strip the trigger nibble when you are done
// (if it matters to your software, that is)
// However, do not strip them if you are using the SFX player, as the SFX
// player requires those nibbles to write the data back.
extern uint16 songNote[4];

// songActive is the LSB of songNote[3]
// this flag contains 1 if playing, zero if stopped
// you can also stop (or pause!) a song by setting it to zero
// This byte also continues the mute bits as defined below.

// we define bits for songActive per LSB 8-bits
#define SONGACTIVEACTIVE 0x01
#define SONGACTIVEMUTE1  0x80
#define SONGACTIVEMUTE2  0x40
#define SONGACTIVEMUTE3  0x20
#define SONGACTIVEMUTE4  0x10


#endif  // file include

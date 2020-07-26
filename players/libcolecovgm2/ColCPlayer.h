#ifndef INCLUDE_COLPLAYER_H
#define INCLUDE_COLPLAYER_H

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// Assuming Coleco
typedef int int16;              // must be 16 bit or larger, signed is okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned char uWordSize; // most efficient word size, 8 bits or more unsigned

// structure for unpacking a stream of data - ALL data comes from globalStr
typedef struct STRTYPE STREAM;
struct STRTYPE {
    const uint8 *curPtr;       // where are are currently getting data from
    const uint8 *mainPtr;      // the main index in the decompression. If 0, we are done.
    uint8 (*curType)();        // function pointer to get data for the type
    uWordSize curBytes;        // current bytes left
    // post compression data
    uWordSize framesLeft;      // number of frames left on the current RLE (not used for tone channels)
};

// global stream object for direct access
extern STREAM globalStr;

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(const unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void StopSong();

// this needs to be called 60 times per second by your system
void SongLoop();

// the Coleco version of getCompressedByte
uint8 getCompressedByteRaw();

// This is a wrapper so that the test code works
uint8 getCompressedByte(STREAM *str, const uint8 *buf);

// this array contains the current volume of each voice (ignoring mutes)
// Sound chip specific. Note that on the AY, the noise channel does not have a dedicated
// volume. In that case, this entry contains the mixer command.
// do not modify these bytes if you are using the SFX player, they are
// used to recover after an SFX. If you aren't using SFX, you can do
// what you like.
extern uint8 songVol[4];

// this array contains the current note on each voice (ignoring mutes)
// do not modify these bytes if you are using the SFX player, they are
// used to recover after an SFX. If you aren't using SFX, you can do
// what you like.
extern uint16 songNote[4];

// songActive is the LSB of songNote[3]
#define songActive (songNote[3]&0xff)

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

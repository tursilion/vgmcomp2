#ifndef INCLUDE_CPLAYER30_H
#define INCLUDE_CPLAYER30_H

#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// For instance, the TI native size is int, but on the Z80
// unsigned char results in more efficient code.
// the code assumes all types are only 8 or 16 bit,
// but it's fine to use ints and chars on bigger machines
#ifdef BUILD_TI99
typedef int int16;              // must be 16 bit or larger, signed is okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned char uWordSize;// most efficient word size, 8 bits or more unsigned
#elif defined(BUILD_COLECO)
typedef int int16;              // must be 16 bit or larger, signed is okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned char uWordSize; // most efficient word size, 8 bits or more unsigned
#elif defined(BUILD_PC)
typedef int int16;              // we're really 32 bit, that's okay
typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
typedef unsigned char uint8;    // must be 8 bit unsigned
typedef unsigned int uWordSize; // most efficient word size, 8 bits or more unsigned
#else
#error Define a machine type!
#endif

// structure for unpacking a stream of data
typedef struct STRTYPE STREAM;
struct STRTYPE {
    const uint8 *curPtr;       // where are are currently getting data from
    const uint8 *mainPtr;      // the main index in the decompression. If 0, we are done.
    uint8 (*curType)(STREAM*, const uint8*);    // function pointer to get data for the type
    uWordSize curBytes;   // current bytes left
    // post compression data
    uWordSize framesLeft; // number of frames left on the current RLE (not used for tone channels)
};

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSong(const unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void StopSong();

// this needs to be called 60 times per second by your system
// this is the only renamed (or changed!) function.
void SongLoop30();

// Don't call this, it's for use by the unpack codes
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
#define SONGACTIVEACTIVE 0x01   // song is playing
#define SONGACTIVEHALF   0x02   // flag to track which half is being processed
#define SONGACTIVEMUTE1  0x80   // mute channel 1 (more of a disable, mute isn't forced)
#define SONGACTIVEMUTE2  0x40   // mute channel 2
#define SONGACTIVEMUTE3  0x20   // mute channel 3
#define SONGACTIVEMUTE4  0x10   // mute noise


#endif  // file include

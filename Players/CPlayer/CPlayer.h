#ifndef INCLUDE_CPLAYER_H
#define INCLUDE_CPLAYER_H

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
typedef unsigned int uWordSize; // most efficient word size, 8 bits or more unsigned
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
    uint16 mainPtr;      // the main index in the decompression. If 0, we are done.
    uint16 curPtr;       // where are are currently getting data from
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
// new trigger is required - if not set you should not write the data
// to the sound chip. For tones, this is just performance, but for the
// noise channel it will affect the quality of the noise.
// Note that these command nibbles are not meaningful to the AY PSG
// and you may need to strip them off to write to the AY. When stripping,
// remember that the AY noise register is /5/ bits, not 4.
// TODO: need to check that. Good chance it can just ignore them.. ;)
// You, the caller, need to strip the trigger nibble when you are done
// (if it matters to your software, that is)
extern uint16 songNote[4];

// this flag contains 1 if playing, zero if stopped
// you can also stop (or pause!) a song by setting it to zero
extern uWordSize songActive;

#endif

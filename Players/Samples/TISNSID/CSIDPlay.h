#ifndef INCLUDE_CSIDPLAY_H
#define INCLUDE_CSIDPLAY_H

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

// this code is only built for the TI assembly today
// For instance, the TI native size is int, but on the Z80
// unsigned char results in more efficient code.
// the code assumes all types are only 8 or 16 bit,
// but it's fine to use ints and chars on bigger machines
#ifdef BUILD_TI99
// take the typedefs from cplayer.h instead...
//typedef int int16;              // must be 16 bit or larger, signed is okay
//typedef unsigned int uint16;    // must be 16 bit or larger, unsigned
//typedef unsigned char uint8;    // must be 8 bit unsigned
//typedef unsigned char uWordSize;// most efficient word size, 8 bits or more unsigned
#else
#error Only defined for TI99
#endif

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
void StartSID(unsigned char *pSbf, uWordSize songNum);

// Call this to stop the current song
void StopSID();

// this needs to be called 60 times per second by your system
void SongSID();

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


#endif  // file include

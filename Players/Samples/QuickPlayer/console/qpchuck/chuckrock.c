// Chuck Rock based visualizer
// gfx converted by Tursi, owned by probably too many people

#include "vdp.h"
#include "sound.h"
#include "player.h"
#ifdef BUILD_TI99
#include <TISNPlay.h>
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
#endif

#include "chuckcol.h"
#include "chuckpat.h"
void *memcpy(void *destination, const void *source, int num);
void *memset(void *s, int c, int n);

#ifdef BUILD_TI99
// best word for counting is int
typedef unsigned int idx_t;
// we have almost no memory free, so reuse the player's stream data
extern STREAM strDat[9];
#endif
#ifdef BUILD_COLECO
// best word for counting is char
typedef unsigned char idx_t;
// we can't easily get at the player's strDat (it's static), but
// we have lots of RAM free since the 1k RAM is separate from code space
STREAM strDat[2];
#endif

// turn this into a macro for the tables below
#ifdef BUILD_TI99
#define MAKE_SCREEN_POS(r,c) (((r)<<5)+(c))
#endif
#ifdef BUILD_COLECO
#define MAKE_SCREEN_POS(r,c) ((((r)<<5)+(c))&0xff),((((r)<<5)+(c))>>8)
#endif

extern const unsigned char TrueLowerCase[];

// address of music
unsigned int chucktune;		// SN only
 
// trampoline functions for chaining
#ifdef BUILD_TI99
// TI trampoline expects a cartridge header and boots the first program
const unsigned char tramp[] = {
    0xC8,0x00,          //  mov r0,@>6000           * page select (hard code page here)
    0x60,0x00,          //
    0xC0,0x20,          //  mov @>6006,r0           * pointer to program list
    0x60,0x06,          //
    0x05,0xC0,          //  inct r0                 * point to start address of first program
    0xC0,0x10,          //  mov *r0,r0              * get start address
    0x04,0x50           //  b *r0                   * execute it
};
#endif
#ifdef BUILD_COLECO
// Coleco trampoline simply jumps to the root of the page
const unsigned char tramp[] = {
    0x3A, 0xFF, 0xFF,   //  LD   a,($ffff)   ; page select (hard code page here)
    0xC3, 0x00, 0xC0    //  JP   $c000       ; execute it
};
#endif

// Windows app formats a full screen into here
// the four bytes after it are the pointers to
// the first song, and then the second. The first
// song is usually at >A000 and the second is
// whereever it fits. Either can be 0 for no song.
// data is in appropriate endian for the system,
// and the second chip is AY or SID depending on system.
// The next 3 bytes are the SID configuration registers,
// used by the TI only. Coleco does not use these at all.
// The fourth byte is a loop flag - 0 for no,
// anything else for yes. Loops is ignored if chaining.
// Bytes 5 and 6 are pointers to an address that contains
// information about chaining. It's externally set if
// there is a loader that handles playing. In both versions
// the value is a cartridge page number, which the code
// jumps to. (The TI version will parse the cart header).
// This structure is the same in all player programs.
// This one doesn't need to be volatile because we make
// no decisions based on its contents
const unsigned char textout[4*32] = {
    "~~~~DATAHERE~~~~4\0"		// allow only 4 lines of text
};

// 00: six bytes of flag (~~FLAG)
// 06: two bytes for SN music (can be 0)
// 08: two bytes for SID or AY music (can be 0)
// 0A: three bytes of SID flags
// 0D: one byte of loop
// 0E: two bytes of pointer to a memory address for chaining
// This struct must exist in all player programs
// The older player doesn't have a ~~FLAG section, it's part of ~~~~DATAHERE~~~~,
// so if you can't find ~~FLAG then you don't have chaining.
#ifdef BUILD_TI99
const unsigned char flags[18] __attribute__((aligned (2))) = {
    "~~FLAGxxyySSSL\0\0:"
};
#endif
#ifdef BUILD_COLECO
const unsigned char flags[18] = {
    "~~FLAGxxyySSSL\0\0:"
};
#endif

// iirc, this is a map that defines where to draw characters and
// whether those characters are animated
const char filter[][33] = {
	"00000000021102000000000000000000",
	"00110000021112200000000000011100",
	"00111011122112211100022200011100",
	"01111011122212211110022210011110",
	"10111001111111111001222210011122",
	"22111001111111111002222210022222",
	"22222001111111111000212210022210",
	"01222001111111111000112210012100",
	"01122100111111111000022200001100",
	"01111101111111111000011100001100"
};

#ifdef BUILD_TI99
// writes the 8 bytes directly to VDPWD from compressed stream
void __attribute__ ((noinline)) writeCompressedByte8(STREAM *str) {
	// r8 is not touched by getCompressedByte, since it's expected to hold the
	// address of the sound chip. So we can use that as a counter
	// r10 is the stack pointer, so that's also safe
    __asm__(                                                        \
        "mov r1,r15\n\t"                                            \
        "dect r10\n\t"                                              \
        "mov r11,*r10\n\t"                                          \
        "li r6,>0100\n\t"                                           \
		"li r8,8\n\t"												\
		"\nlbl%=:\n\t"												\
        "bl @getCompressedByte\n\t"                                 \
		"movb r1,@>8c00\n\t"										\
		"dec r8\n\t"												\
		"jne lbl%=\n\t"												\
        "mov *r10+,r11\n"                                           \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r15","cc"   \
        );
}
#endif
#ifdef BUILD_COLECO
#define COLECO_FONT (unsigned char*)0x15A3
// coleco lib is a little behind the TI one... set_bitmap matches the new _raw
#define set_bitmap_raw set_bitmap

void writeCompressedByte8(STREAM *str) {
	for (idx_t idx = 0; idx<8; ++idx) {
		VDPWD = getCompressedByte(str);
	}
}
#endif

// pulls 8 bytes from each stream and loads as requested into VDP
// assumes that there is no jumping around the compressed stream
void unpackchar(int vdpOff) {
	VDP_SET_ADDRESS_WRITE(gPattern+vdpOff);
	writeCompressedByte8(&strDat[0]);

	VDP_SET_ADDRESS_WRITE(gColor+vdpOff);
	writeCompressedByte8(&strDat[1]);
}

void chuckinit() {
	unsigned char x = set_bitmap_raw(VDP_SPR_8x8);		// set graphics mode
	VDP_SET_REGISTER(VDP_REG_COL,COLOR_BLACK);	// set background color black

	// initialize the screen image table - rather than normal bitmap,
	// we are treating it as a high color character display
	vdpmemset(0, 0, 512);

	// the third block is for text (so, space characters)
	vdpmemset(0+512, 32, 256);

	// load the character set to bank three of the bitmap screen (and default the colors)
	vdpmemcpy(gPattern+0x1300, TrueLowerCase, 216);

#ifdef BUILD_TI99
	gplvdp(0x0018, gPattern+0x1100-1, 64);	// the rest of the character set (-1 is a hacky way to shift them up by one to match the lowercase font)
#endif
#ifdef BUILD_COLECO
	vdpmemcpy(gPattern+0x1100, COLECO_FONT, 64*8);
#endif
	// set the color table for the lower third to white on black
	vdpmemset(gColor+0x1000, 0xf0, 0x800);

	// load chuck's data. Unfortunately, the two frames combined give 258 characters,
	// so we have to be aware of the split between the top third and the second third
	// the image is 10 lines high, so we put 5 lines in each third
	unsigned char nChar=0;		// displayed char (will pre-increment)
	int nVRAMOff=8;		// VRAM offset

	// we need to use two streams here...
	memset(strDat, 0, sizeof(strDat));
	strDat[0].mainPtr = (unsigned char*)chuckpat;
	strDat[1].mainPtr = (unsigned char*)chuckcol;

	// extract the space character
	unpackchar(0);

	for (idx_t nFrame=0; nFrame<2; nFrame++) {
		for (idx_t idx=0; idx<10; idx++) {
			for (idx_t c=0; c<32; c++) {
				if (((nFrame==0)&&(filter[idx][c] > '0')) || ((nFrame==1)&&(filter[idx][c]=='2'))) {
					// we have a valid character!
					++nChar;			// increment the character index
					unpackchar(nVRAMOff);		// extract the data
					vdpscreenchar(((idx+3)<<5)+c,nChar);	// put the char on screen
					nVRAMOff+=8;
				}
			}
			if (idx==4) {
				if (nFrame) {
					nVRAMOff=0xB80;		// restart on the next character set
					nChar=111;
				} else {
					nVRAMOff=0x808;		// restart on the next character set
					vdpmemset(gPattern+0x800, 0, 8);
					vdpmemset(gColor+0x800, 0, 8);
					nChar=0;
				}
			}
		}

		// handle frame 2
		nChar=86;			// displayed char (will pre-increment)
		nVRAMOff=0x2B8;		// VRAM offset
	}

	VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display
}

// animation functions per character
// note they assume that the base graphics are drawn and update only the
// ones that actually change!
void processList(const idx_t *pList) {
#ifdef BUILD_TI99
	while (*pList) {
		vdpwritescreeninc(*pList, *(pList+1), *(pList+2));
		pList+=3;	// 3 words
#endif
#ifdef BUILD_COLECO
	while (*((const unsigned int*)pList)) {
		// warning: on COLECO the MAKE_SCREEN_POS macro drops /two/ bytes, so this cast
		// is intentional!
		vdpwritescreeninc(*((const unsigned int*)pList), *(pList+2), *(pList+3));
		pList+=4;	// 4 bytes
#endif
	}
}

// dino (channel 1)
const idx_t dinof1dat[] = {
	MAKE_SCREEN_POS(8,0), 1, 2,
	MAKE_SCREEN_POS(9,0), 27, 5,
	MAKE_SCREEN_POS(10,2), 52, 3,
	MAKE_SCREEN_POS(11,3), 75, 2,

	MAKE_SCREEN_POS(0,0)
};
inline void dinof1() {
	processList(dinof1dat);
}

const idx_t dinof2dat[] = {
	MAKE_SCREEN_POS(8,0), 112, 2,
	MAKE_SCREEN_POS(9,0), 124, 5,
	MAKE_SCREEN_POS(10,2), 135, 3,
	MAKE_SCREEN_POS(11,3), 141, 2,

	MAKE_SCREEN_POS(0,0)
};
inline void dinof2() {
	processList(dinof2dat);
}

// gary gritter (channel 3)

const idx_t garyf1dat[] = {
	MAKE_SCREEN_POS(3,9), 1, 1,
	MAKE_SCREEN_POS(3,13), 4, 1,
	MAKE_SCREEN_POS(4,9), 7, 1,
	MAKE_SCREEN_POS(4,13), 11, 2,
	MAKE_SCREEN_POS(5,9), 22, 2,
	MAKE_SCREEN_POS(5,13), 26, 2,
	MAKE_SCREEN_POS(6,9), 44, 3,
	MAKE_SCREEN_POS(6,13), 48, 2,

	MAKE_SCREEN_POS(0,0)
};
inline void garyf1() {
	processList(garyf1dat);
}

const idx_t garyf2dat[] = {
	MAKE_SCREEN_POS(3,9), 87, 1,
	MAKE_SCREEN_POS(3,13), 88, 1,
	MAKE_SCREEN_POS(4,9), 89, 1,
	MAKE_SCREEN_POS(4,13), 90, 2,
	MAKE_SCREEN_POS(5,9), 92, 2,
	MAKE_SCREEN_POS(5,13), 94, 2,
	MAKE_SCREEN_POS(6,9), 99, 3,
	MAKE_SCREEN_POS(6,13), 102, 2,

	MAKE_SCREEN_POS(0,0)
};
inline void garyf2() {
	processList(garyf2dat);
}

// ophelia (channel 2)
const idx_t opheliaf1dat[] = {
	MAKE_SCREEN_POS(7,30), 85, 2,
	MAKE_SCREEN_POS(8,27), 22, 5,
	MAKE_SCREEN_POS(9,27), 47, 3,
	MAKE_SCREEN_POS(10,28), 71, 1,

	MAKE_SCREEN_POS(0,0)
};
inline void opheliaf1() {
	processList(opheliaf1dat);
}

const idx_t opheliaf2dat[] = {
	MAKE_SCREEN_POS(7,30), 111, 2,
	MAKE_SCREEN_POS(8,27), 119, 5,
	MAKE_SCREEN_POS(9,27), 132, 3,
	MAKE_SCREEN_POS(10,28), 140, 1,

	MAKE_SCREEN_POS(0,0)
};

inline void opheliaf2() {
	processList(opheliaf2dat);
}

// chuck rock (channel 0)
const idx_t chuckf1dat[] = {
	MAKE_SCREEN_POS(5,21), 31, 3,
	MAKE_SCREEN_POS(6,21), 54, 3,
	MAKE_SCREEN_POS(7,20), 77, 4,
	MAKE_SCREEN_POS(8,19), 16, 5,
	MAKE_SCREEN_POS(9,20), 42, 1,
	MAKE_SCREEN_POS(9,22), 44, 2,
	MAKE_SCREEN_POS(10,22), 67, 2,
	MAKE_SCREEN_POS(11,21), 87, 3,

	MAKE_SCREEN_POS(0,0)
};
inline void chuckf1() {
	processList(chuckf1dat);
}

const idx_t chuckf2dat[] = {
	MAKE_SCREEN_POS(5,21), 96, 3,
	MAKE_SCREEN_POS(6,21), 104, 3,
	MAKE_SCREEN_POS(7,20), 107, 4,
	MAKE_SCREEN_POS(8,19), 114, 5,
   	MAKE_SCREEN_POS(9,20), 129, 1,
	MAKE_SCREEN_POS(9,22), 130, 2,
	MAKE_SCREEN_POS(10,22), 138, 2,
	MAKE_SCREEN_POS(11,21), 143, 3,

	MAKE_SCREEN_POS(0,0)
};
inline void chuckf2() {
	processList(chuckf2dat);
}

unsigned char nOldVol[4], nState[4];
unsigned int nOldVoice[4];

// main entry point
int main() {

	chuckinit();

	for (idx_t idx=0; idx<4; idx++) {
		nOldVol[idx]=0xff;
		nOldVoice[idx]=0;
		nState[idx]=0;
	}

	// we support 4 lines of text from the text input
	vdpmemcpy(gImage+(17*32+0), textout, 32*4);

	// get the song pointer
    chucktune = *((unsigned int*)&flags[6]);

	// const is fine for easy storage and patching, but for the decision making on
    // the loop value, we need to pretend it's volatile
    volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

	do {
		unsigned char done;
		unsigned int *chain;

        if (0 != chucktune) {
            StartSong((unsigned char*)chucktune, 0);
        }

		// main play loop 
		done = 0;
		while (!done) {
			done = 1;
			vdpwaitvint();
            if (0 != chucktune) {
                if (isSNPlaying) {
                    CALL_PLAYER_SN;
                    done = 0;
                }
            }

			if (songNote[0] != nOldVoice[0]) {
				// update chuck
				nOldVol[0]=songVol[0];
				nOldVoice[0]=songNote[0];
				nState[0]=!nState[0];
				if (nState[0]) {
					chuckf2();
				} else {
					chuckf1();
				}
			}
			if (songNote[1] != nOldVoice[1]) {
				// update dino
				nOldVol[1]=songVol[1];
				nOldVoice[1]=songNote[1];
				nState[1]=!nState[1];
				if (nState[1]) {
					dinof2();
				} else {
					dinof1();
				}
			}
			if (songNote[2] != nOldVoice[2]) {
				// update ophelia
				nOldVol[2]=songVol[2];
				nOldVoice[2]=songNote[2];
				nState[2]=!nState[2];
				if (nState[2]) {
					opheliaf2();
				} else {
					opheliaf1();
				}
			}
			if (songVol[3]+4 < nOldVol[3]) {
				// update gary
				nOldVoice[3]=songNote[3];
				nState[3]=!nState[3];
				if (nState[3]) {
					garyf2();
				} else {
					garyf1();
				}
			}
			nOldVol[3]=songVol[3];
		}

		// mute the sound chip
		SOUND=0x9F;
		SOUND=0xBF;
		SOUND=0xDF;
		SOUND=0xFF;

        // loop, chain, or reset if it is finished
        // chain is normally set by a loader and takes priority
        chain = (unsigned int *)(*((volatile unsigned int*)(&flags[14])));
        if (chain) {
            // look up the value to chain to
            unsigned int chained = *chain;
            if (chained) {
                // need to trampoline to the specified cartridge page
                // TI and Coleco each have different code and targets
    #ifdef BUILD_TI99
                memcpy((void*)0x8320, tramp, sizeof(tramp));   // 0x8320 so we don't overwrite the C registers
                *((unsigned int*)0x8322) = chained;     // patch the pointer
                ((void(*)())0x8320)();                  // call the function, never return
    #endif
    #ifdef BUILD_COLECO
                memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
                *((unsigned int*)0x7001) = chained;     // patch the pointer, chained should be on the stack
                ((void(*)())0x7000)();                  // call the function, never return
    #endif
            }
		}
	} while (*pLoop);

    // otherwise, reboot
#ifdef BUILD_TI99
    // GCC syntax (though technically my code will reset on exit anyway)
    __asm__ ("limi 0\n\tblwp @>0000");
#endif
#ifdef BUILD_COLECO
    // SDCC syntax
    __asm
        rst 0x00
    __endasm;
#endif

	return 2;
}

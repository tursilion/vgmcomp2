// piano keyboard visualizer
// By Tursi

#include "vdp.h"
#include "sound.h"
#ifdef BUILD_TI99
#include <TISNPlay.h>
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
#endif
void *memcpy(void *destination, const void *source, int num);

#ifdef BUILD_TI99
// best word for counting is int
typedef unsigned int idx_t;
#endif
#ifdef BUILD_COLECO
// best word for counting is char
typedef unsigned char idx_t;
#endif

unsigned int firstSong;     // for SN
// TODO: this might be able to handle the second chip as well...
 
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
#ifdef BUILD_TI99
const unsigned char __attribute__((aligned (2))) textout[4*32] = {
#endif
#ifdef BUILD_COLECO
const unsigned char textout[4*32] = {
#endif
    "~~~~DATAHERE~~~~4\0"		// allow only 4 lines of text
};

// 00: six bytes of flag (~~FLAG)
// 06: two bytes for SN music (can be 0)
// 08: two bytes for SID or AY music (can be 0)
// 0A: three bytes of SID flags
// 0D: one byte of loop
// 0E: two bytes of pointer to a cartridge page for chaining
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

// screen (start at 0x60, for 9 rows)
const unsigned char Screen[] = {
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
	136,192,193,194,195,196,197,198,199,136,136,136,136,136,136,136,
	136,136,136,136,136,136,136,136,200,201,202,203,204,205,136,136,
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
	137,138,139,140,141,142,140,141,142,143,144,142,143,144,145,146,
	144,145,146,138,139,140,138,139,140,141,142,140,141,142,143,147,
	148,149,150,151,152,153,151,152,153,154,155,153,154,155,156,157,
	155,156,157,149,150,151,149,150,151,152,153,151,152,153,154,158,
	148,149,150,151,152,153,151,152,153,154,155,153,154,155,156,157,
	155,156,157,149,150,151,149,150,151,152,153,151,152,153,154,158,
	148,159,160,161,159,160,161,159,160,161,159,160,161,159,160,161,
	159,160,161,159,160,161,159,160,161,159,160,161,159,160,161,158,
	162,163,164,165,163,164,165,163,164,165,163,164,165,163,164,165,
	163,164,165,163,164,165,163,164,165,163,164,165,163,164,165,166,
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
	136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,136,
};

// char definitions (136 - 205)
// load at 0x0440, for 560 bytes
const unsigned char Patterns[] = {
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xFF,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0xFF,0x78,0x78,0x78,0x78,0x78,0x78,0x78,
	0xFF,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,
	0xFF,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
	0xFF,0x31,0x31,0x31,0x31,0x31,0x31,0x31,
	0xFF,0xE7,0xE7,0xE7,0xE7,0xE7,0xE7,0xE7,
	0xFF,0x8C,0x8C,0x8C,0x8C,0x8C,0x8C,0x8C,
	0xFF,0x79,0x79,0x79,0x79,0x79,0x79,0x79,
	0xFF,0xE3,0xE3,0xE3,0xE3,0xE3,0xE3,0xE3,
	0xFF,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,
	0xFF,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
	0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,
	0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,0xC7,
	0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,0x9E,
	0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31,
	0xE7,0xE7,0xE7,0xE7,0xE7,0xE7,0xE7,0xE7,
	0x8C,0x8C,0x8C,0x8C,0x8C,0x8C,0x8C,0x8C,
	0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79,
	0xE3,0xE3,0xE3,0xE3,0xE3,0xE3,0xE3,0xE3,
	0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,0x1E,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,
	0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,
	0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,
	0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x07,
	0x30,0x30,0x30,0x30,0x30,0x30,0x30,0xFF,
	0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0xFF,
	0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0xFF,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0xE0,
	0x00,0x3C,0x40,0x40,0x5C,0x44,0x44,0x38,
	0x00,0x44,0x44,0x44,0x7C,0x44,0x44,0x44,
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0x00,
	0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0x00,0x00,
	0x70,0x70,0x70,0x70,0x70,0x70,0x70,0xF0,
	0x60,0x60,0x60,0x60,0x60,0x60,0x60,0xF0,
	0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xF0,
	0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,
	0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,
	0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,0xE0,
	0x00,0x78,0x44,0x44,0x78,0x50,0x48,0x44,
	0x00,0x38,0x44,0x40,0x38,0x04,0x44,0x38,
	0x1C,0x22,0x22,0x00,0x22,0x22,0x1C,0x00,
	0x00,0x02,0x02,0x00,0x02,0x02,0x00,0x00,
	0x1C,0x02,0x02,0x1C,0x20,0x20,0x1C,0x00,
	0x1C,0x02,0x02,0x1C,0x02,0x02,0x1C,0x00,
	0x00,0x22,0x22,0x1C,0x02,0x02,0x00,0x00,
	0x1C,0x20,0x20,0x1C,0x02,0x02,0x1C,0x00,
	0x1C,0x20,0x20,0x1C,0x22,0x22,0x1C,0x00,
	0x1C,0x02,0x02,0x00,0x02,0x02,0x00,0x00,
	0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C,0x00,
	0x1C,0x22,0x22,0x1C,0x02,0x02,0x1C,0x00,
	0x00,0x00,0x10,0x28,0x44,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
	0x02,0xCE,0x08,0x08,0x08,0x08,0x28,0x18,
	0x12,0x12,0x12,0x12,0x12,0x12,0x86,0x7C,
	0x04,0x1A,0x12,0x06,0x1A,0x12,0x92,0x76,
	0x04,0x02,0x7E,0x84,0xFA,0x02,0x86,0x7C,
	0x08,0x04,0x04,0x04,0x04,0x04,0x2C,0x18,
	0x12,0x16,0x0C,0x04,0x02,0x12,0x92,0x7E,
	0x02,0x0E,0x18,0x02,0x1E,0x08,0x82,0x7E,
	0x12,0x12,0x12,0x1A,0x86,0x44,0x04,0x3C,
	0x04,0x1A,0x12,0x12,0x12,0x12,0x06,0xFC,
	0x04,0x1A,0x12,0x06,0x1A,0x12,0x92,0x76,
	0x12,0x12,0x12,0x12,0x12,0x12,0x86,0x7C,
	0x04,0x2A,0x2A,0x2A,0x2A,0x2A,0xAA,0x7E,
	0x00,0x04,0x04,0x1C,0x04,0x04,0x1C,0x00,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

// number of keys defined (from the E/A note table)
#define NUM_KEYS 69

// key 'type' for sprite patterns (69 keys)
const unsigned char KeyTypes[] = {
	3 , 4 , 1 , 3 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 1 ,
	3 , 4 , 2 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 2 , 4 , 1 , 3 ,
	4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 , 1 , 3 , 4 , 2 , 4 ,
	2 , 4 , 1 , 3 , 4 , 2 , 4 , 1 , 0
};

// sprite character offsets for each 'type', top to bottom
const unsigned char KeySprites[][5] = {
	{	0,0,0,0,2	},
	{	6,6,3,0,2	},
	{	7,7,4,0,2	},
	{	8,8,5,0,2	},
	{	0,0,1,255,255	}		// black key needs just three sprites
};

// X position for each sprite to start.
const unsigned char KeyPos[] = {
	 6 , 9 , 12 , 18 , 21 , 24 , 27 , 30 , 36 , 39 , 42 , 45 , 48 , 51 , 54 , 60 ,
	 63 , 66 , 69 , 72 , 78 , 81 , 84 , 87 , 90 , 93 , 96 , 102 , 105 , 108 , 111 ,
	 114 , 120 , 123 , 126 , 129 , 132 , 135 , 138 , 144 , 147 , 150 , 153 , 156 ,
	 162 , 165 , 168 , 171 , 174 , 177 , 180 , 186 , 189 , 192 , 195 , 198 , 204 ,
	 207 , 210 , 213 , 216 , 219 , 222 , 228 , 231 , 234 , 237 , 240 , 246 
};

// table of frequencies for each note. When searching, count down from the top.
// if you are greater than or equal the count, you are in the right place
const unsigned int Freqs[] = {
	1017,		// A0
	960,
	906,
	855,
	807,
	762,
	719,
	679,
	641,
	605,
	571,
	539,
	508,
	480,
	453,
	428,
	404,
	381,
	360,
	339,
	320,
	302,
	285,
	269,
	254,
	240,
	226,
	214,
	202,
	190,
	180,
	170,
	160,
	151,
	143,
	135,
	127,
	120,
	113,
	107,
	101,
	95,
	90,
	85,
	80,
	76,
	71,
	67,
	64,
	60,
	57,
	53,
	50,
	48,
	45,
	42,
	40,
	38,
	36,
	34,
	32,
	30,
	28,
	27,
	25,
	24,
	22,
	21,
	20,		// F6
	0		// marks end of table so the search doesn't need to count
};

// colors for the three tone channels
const unsigned char Color[3] = { COLOR_MEDRED, COLOR_MEDGREEN, COLOR_LTBLUE };

void pianoinit() {
	unsigned char x = set_graphics_raw(VDP_SPR_8x8);		// set graphics mode
	VDP_SET_REGISTER(VDP_REG_COL,COLOR_DKBLUE);	// set background color
	charsetlc();								// load default character set with lowercase

	// clear the screen
	vdpmemset(gImage, 32, 768);

	// set color table from 0x11 to 0x19 to 0x1f
	vdpmemset(gColor, 0xf0, 32);
	vdpmemset(gColor+0x11, 0x1e, 9);

	// screen (start at 0x60, for 9 rows)
	vdpmemcpy(gImage+0x60, Screen, 32*9);

	// dumbly enable 16 sprites (and put them all off screen)
	vdpmemset(gSprite, 0xd1, 16*4);
	vdpchar(gSprite+16*4, 0xd0);		// turn off the rest

	// char definitions (136 - 205)
	// load at 0x0440, for 560 bytes
	// note this setup assumes that sprite patterns overlap chaarcter patterns
	vdpmemcpy(gPattern+0x440, Patterns, 560);

	VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display
}

void drawkey(idx_t voice, idx_t key) {
	idx_t spr=voice*5;
	idx_t type=KeyTypes[key];
	idx_t cc=KeyPos[key];
	sprite(spr++, KeySprites[type][0]+169, Color[voice], 48, cc);		// sprite offset character is 169
	sprite(spr++, KeySprites[type][1]+169, Color[voice], 56, cc);
	sprite(spr++, KeySprites[type][2]+169, Color[voice], 64, cc);
	if (type == 4) {
		delsprite(spr++);
		delsprite(spr++);
	} else {
		sprite(spr++, KeySprites[type][3]+169, Color[voice], 72, cc);
		sprite(spr++, KeySprites[type][4]+169, Color[voice], 80, cc);
	}
}

int main() {
    unsigned char done;
    unsigned int chain;

	pianoinit();

    // draw the user text - just four lines
    vdpmemcpy(17*32, textout, 32*4);

    // get the song pointers
    firstSong = *((unsigned int*)&flags[6]);

    // const is fine for easy storage and patching, but for the decision making on
    // the loop value, we need to pretend it's volatile
    volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

    do {
		StartSong((unsigned char*)firstSong,0);

		// main play loop
		done = 0;
		while (!done) {
			done = 1;
			vdpwaitvint();

			if (isSNPlaying) {
				CALL_PLAYER_SN;
				done = 0;
			}

			// update tone channels
			for (idx_t idx=0; idx<3; idx++) {
				if ((songVol[idx]&0x0f) < 0x0f) {
					unsigned int note=songNote[idx];		// note: mangled! 0x8312 == 0x0123
					note=((note&0xff)<<4)|((note&0x0f00)>>8);	// unmangle
					idx_t k=0;
					// force worst case for testing performance (seems okay)
					//note=0;
					while (Freqs[k] > note) k++;
					if (k > NUM_KEYS-1) k=NUM_KEYS-1;
					drawkey(idx, k);
				} else {
					idx_t spr=idx*5;
					for (idx_t idx2=0; idx2<5; idx2++) {
						delsprite(spr+idx2);
					}
				}
			}

			// update noise channel
			{
				idx_t nVol = songVol[3]&0x0f;
				if (nVol == 0x0f) {
					// turn off digit by making it black
					vdpchar(gSprite+15*4+3, 0x00);
				} else {
					// set it to the right LED value
					idx_t note=(songNote[3]&0x0f00)>>8;
					note+=181;	// start at character '1', not 0 (which is 180)
					sprite(15, note, COLOR_MEDRED, 32, 232);
				}
			}
		}

		// mute the sound chip
		SOUND=0x9F;
		SOUND=0xBF;
		SOUND=0xDF;
		SOUND=0xFF;

        // loop, chain, or reset if it is finished
        // chain is normally set by a loader and takes priority
        chain = (unsigned int)(*((volatile unsigned int*)(&flags[14])));
        // look up the value to chain to
        if (chain) {
            // need to trampoline to the specified cartridge page
            // TI and Coleco each have different code and targets
#ifdef BUILD_TI99
            memcpy((void*)0x8320, tramp, sizeof(tramp));   // 0x8320 so we don't overwrite the C registers
            *((unsigned int*)0x8322) = chain;     // patch the pointer
            ((void(*)())0x8320)();                  // call the function, never return
#endif
#ifdef BUILD_COLECO
            memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
            *((unsigned int*)0x7001) = chain;     // patch the pointer, chained should be on the stack
            ((void(*)())0x7000)();                  // call the function, never return
#endif
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

    // never reached
    return 2;
}


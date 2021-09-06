// attempt to port blinkenlights to C

// uses libti99 and libtivgm2
#include "vdp.h"
#include "sound.h"
#ifdef BUILD_TI99
#include "system.h"
#include <TISNPlay.h>
#include <TISIDPlay.h>
typedef unsigned int idx_t;
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
#include <ColecoAYPlay.h>
typedef unsigned char idx_t;
#endif

void *memcpy(void *destination, const void *source, int num);

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
const unsigned char textout[64] = {
    "~~~~DATAHERE~~~~2\0"  // allow 2 rows of text
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

const unsigned char ball[8] = {
	0x00,0x3c,0x7e,0x7e,0x7e,0x3c,0x00
};
const unsigned char colortable[16] = {
	0x41,0x51,0x61,0x81,0x91,0xc1,0x21,0x31,
	0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0,0xe0
};

#ifdef BUILD_TI99
// put the screen buffer in scratchpad on TI
unsigned char *scrnbuf = (unsigned char*)0x8320;
#endif
#ifdef BUILD_COLECO
unsigned char scrnbuf[8];
#endif

unsigned int firstSong;     // for SN
unsigned int secondSong;    // for SID or AY

void ballz(unsigned int n) {
	// n is a 16-bit address, we need to scale it down to fit
    // the screen size is 704 positions (768-2 rows for top and bottom)
    // I suppose we can just do a mod...
    n = n % 704 + 32;
    vdpchar(n, 64);

    // prepare to mirror
    unsigned int row  = n&0x3e0;    // keep the shift
	unsigned int row24=0x2e0-row;   // address of last row - faster than shifting?
    idx_t col  = n&0x01f;
	idx_t col32=32-col;

    vdpchar(row24+col,64);
    vdpchar(row+col32,64);
    vdpchar(row24+col32,64);
}

void updateRow() {
	// read and fade 8 bytes at a time, unrolled
	static unsigned int scrnpos = 32;

    unsigned char *p = scrnbuf;

    VDP_SET_ADDRESS(scrnpos);
    VDP_SAFE_DELAY;

    *p = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;
    *(++p) = VDPRD;
    if (*p) *p-=4;

    vdpmemcpy(scrnpos, scrnbuf, 8);

    scrnpos+=8;
    if (scrnpos >= 736) scrnpos = 32;
}

void drawtext(unsigned int scrn, const unsigned char *txt, idx_t cnt) {
    // we aren't going to filter most characters, but we will
    // make sure space is 0, rather than 32, since 32 got used.
    VDP_SET_ADDRESS_WRITE(scrn);
    while (cnt--) {
        if (*txt != 32) {
            VDPWD = *(txt++);
        } else {
            VDPWD = 0;
            ++txt;
        }
    }
}

int main() {
    unsigned char done;
    unsigned int *chain;
	idx_t idx;
	unsigned char x = set_graphics_raw(VDP_SPR_8x8);	// set graphics mode with 8x8 sprites
	vdpchar(gSprite, 0xd0);					// all sprites disabled
    vdpmemset(gImage, 0, 768);				// clear screen to char 0
	
	vdpmemcpy(gColor, colortable, 16);      // color table
	
    charsetlc();
	vdpmemset(gPattern, 0, 8);				// char 0 is blank
	// set up a ball character every 4 characters, starting at 4
	for (idx=4; idx<65; idx+=4) {
		vdpmemcpy(gPattern+(int)(idx)*8, ball, 8);
	}

    VDP_SET_REGISTER(VDP_REG_COL, COLOR_BLACK);	// background color
    VDP_SET_REGISTER(VDP_REG_MODE1, x);		// enable the display and interrupts

	// the idea at this point is just a mirrored vis with sort of a fade
	// through the colors. Top row and bottom row contain the text.
	
	// draw the first two lines of text
	drawtext(gImage, textout, 32);
	drawtext(gImage+(23*32), textout+32, 32);
	
    // get the song pointers
    firstSong = *((unsigned int*)&flags[6]);
    secondSong = *((unsigned int*)&flags[8]);

#ifdef BUILD_TI99 
    if (0 != secondSong) {
        // this is a SID song, so read the SID registers too
        SidCtrl1 = flags[10];
        SidCtrl2 = flags[11];
        SidCtrl3 = flags[12];
    }
#endif

    // const is fine for easy storage and patching, but for the decision making on
    // the loop value, we need to pretend it's volatile
    volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

    do {
        // start the song(s)
        if (0 != firstSong) {
            StartSong((unsigned char*)firstSong, 0);
        }
        if (0 != secondSong) {
    #ifdef BUILD_TI99
            StartSID((unsigned char*)secondSong, 0);
    #endif
    #ifdef BUILD_COLECO
            ay_StartSong((unsigned char*)secondSong, 0);
    #endif
        }

        // and play it on the interrupt
        done = 0;
        while (!done) {
            done = 1;
			// wait for an interrupt - while waiting process rows on the screen
#ifdef BUILD_TI99
			// check for quit and reboot
			// will be checkquit() in libti99 eventually
			__asm__("li r12,>0024\n\tldcr @>0012,3\n\tsrc r12,7\n\tli r12,6\n\tstcr r0,8\n\tandi r0,>1100\n\tjne 4\n\tblwp @>0000" : : : "r12","r0");
            // loop waiting for an int, calling updateRow
			__asm__( "\nmike%=:\n\tbl @updateRow\n\tclr r12\n\ttb 2\n\tjeq mike%=\n\tmovb @>8802,r12" : : : "r12" );
#endif
#ifdef BUILD_COLECO
			while ((vdpLimi&0x80) == 0) {
				updateRow();
			}
			VDP_CLEAR_VBLANK;
#endif
            if (0 != firstSong) {
                if (isSNPlaying) {
                    CALL_PLAYER_SN;
                    done = 0;
                }
            }
            if (0 != secondSong) {
        #ifdef BUILD_TI99
                if (isSIDPlaying) {
                    CALL_PLAYER_SID;
                    done = 0;
                }
        #endif
        #ifdef BUILD_COLECO
                if (isAYPlaying) {
                    CALL_PLAYER_AY;
                    done = 0;
                }
        #endif
            }
            
            // draw new balls for the current notes, just plot character 64
            // row from 0-11, col from 0-15
            if (0 != firstSong) {
				// notes are swizzled: CLMM, actual value is 0MML
				// volumes are CV. Make it MMLV
				ballz(((songNote[0]&0xff)<<8)|((songNote[0]&0x0f00)>>4)|(songVol[0]&0xf));
				ballz(((songNote[1]&0xff)<<8)|((songNote[1]&0x0f00)>>4)|(songVol[1]&0xf));
				ballz(((songNote[2]&0xff)<<8)|((songNote[2]&0x0f00)>>4)|(songVol[2]&0xf));
				// noise tone is CN00, make it CN0V
				ballz((songNote[3]&0xff00)|(songVol[3]&0xf));
			}
			if (0 != secondSong) {
#ifdef BUILD_TI99
				// sid notes: LLMM, actual values is MMLL
				// volume is N0 (and this is inverted compared to SN)
                // make it MMLL+N
				ballz((((sidNote[0]&0xff)<<8)|(sidNote[0]>>8))+(sidVol[0]>>4));
				ballz((((sidNote[1]&0xff)<<8)|(sidNote[1]>>8))+(sidVol[1]>>4));
				ballz((((sidNote[2]&0xff)<<8)|(sidNote[2]>>8))+(sidVol[2]>>4));
#endif
#ifdef BUILD_COLECO
				// AY notes: LL0M, actual value is 0MLL
				// volume is V0 (and this is inverted compared to SN)
                // Make it MLLV
				ballz(((ay_songNote[0]&0xff)>>4)|((ay_songNote[0]&0xf)<<12)|(ay_songVol[0]>>4));
				ballz(((ay_songNote[1]&0xff)>>4)|((ay_songNote[1]&0xf)<<12)|(ay_songVol[1]>>4));
				ballz(((ay_songNote[2]&0xff)>>4)|((ay_songNote[2]&0xf)<<12)|(ay_songVol[2]>>4));
#endif
			}
        }

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

    // never reached
    return 2;

}

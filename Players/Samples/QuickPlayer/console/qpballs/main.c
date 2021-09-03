// Fourth musical animation - Tursi 2020
// Fourth musical animation - Tursi 2020
// ported to quickplayer system 2021
// TI only at this point

// uses libti99 and libtivgm2
#include "vdp.h"
#include "sound.h"
#ifdef BUILD_TI99
#include "system.h"
#include <TISNPlay.h>
typedef unsigned int idx_t;
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
typedef unsigned char idx_t;
#endif

#include "ani1.c"
void *memcpy(void *destination, const void *source, int num);

#define FIRST_TONE_CHAR 2
#define FIRST_DRUM_CHAR 178
#define FIRST_TONE_COLOR 0
#define FIRST_DRUM_COLOR 22

// oversubscribe the sprite table a little to reduce starvation caused by
// the delay system - only active sprites are actually copied to the sprite table
#ifdef BUILD_TI99
#define MAX_SPRITES 40
#endif
#ifdef BUILD_COLECO
// coleco code isn't quite as efficient, in particular with the fixed-point math
#define MAX_SPRITES 34
#endif

struct SPRPATH {
	unsigned int x,y;
	int xstep,ystep;

	// z does double duty as a delay counter - when positive, it's a delay,
	// when it reaches zero, the ball launches and it is set negative by zstep.
	// when it becomes zero or positive again, the ball is done.
#ifdef BUILD_TI99
	int zstep;
	int z;
#endif
#ifdef BUILD_COLECO
	signed char zstep;
	signed char z;
#endif

	unsigned char col;
} sprpath[MAX_SPRITES];

// buffer for working with color table in CPU RAM
unsigned char colortab[32];

// ring buffer for delaying audio - multiple of 15 steps
// sadly, yes, 15 and not 16, which we could mask

// now works on both systems
#define DELAYTIME 30

unsigned char delayvol[4][DELAYTIME];
unsigned int delaytone[4][DELAYTIME];
idx_t delaypos, finalcount;

#ifdef BUILD_TI99
unsigned int status;
#endif

// we use 8 channels instead of 4 by assuming arpeggio all the
// time. By tracking even and odd frames separately, we can treat
// it as two notes instead of a constant stream of balls. The stream
// looks great, but we don't have enough sprites and it slows the
// code down dispatching them all the time.
idx_t nOldTarg[8];	// will double up old targ
idx_t nOldVol[4];	// won't double up volume, arp should be close

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

// Windows app formats a full screen into here
// we try to show 3 lines of it with a limited font
const unsigned char textout[3*32] = {
    "~~~~DATAHERE~~~~3\0"		// allow only 3 lines of text
};

// 00: six bytes of flag (~~FLAG)
// 06: two bytes for SN music (can be 0)
// 08: two bytes for SID or AY music (not supported here)
// 0A: three bytes of SID flags
// 0D: one byte of loop
// 0E: two bytes of pointer to a memory address for chaining
// This struct must exist in all player programs
// The older player doesn't have a ~~FLAG section, it's part of ~~~~DATAHERE~~~~,
// so if you can't find ~~FLAG then you don't have chaining.
// THIS is accessed as word, so must be aligned
#ifdef BUILD_TI99
const unsigned char flags[18] __attribute__((aligned (2))) = {
#endif
#ifdef BUILD_COLECO
const unsigned char flags[18] = {
#endif
    "~~FLAGxxyySSSL\0\0:"
};

unsigned int firstSong;     // for SN

idx_t nextSprite = 0;
idx_t frame = 0;
idx_t idx, i2;

#define sx (124)
#define sy (15*8)

int main() {
	// init the screen
	{
		unsigned char x = set_graphics_raw(VDP_SPR_8x8);	// set graphics mode with 8x8 sprites
		VDP_SET_REGISTER(VDP_REG_CT, 0x0f);		// move color table to 0x03c0
		gColor = 0x03c0;
		vdpmemset(gColor, 0x10, 32);			// all colors to black on transparent
		vdpmemset(gPattern, 0, 8);				// char 0 is blank
		vdpmemset(gImage, 0, 768);				// clear screen to char 0
		vdpchar(gSprite, 0xd0);					// all sprites disabled
		VDP_SET_REGISTER(VDP_REG_COL, COLOR_MAGENTA);	// background color

        // preload the buffer with mutes
        for (idx=0; idx<4; ++idx) {
            for (i2=0; i2<DELAYTIME; ++i2) {
                delayvol[idx][i2] = 0x0f | (0x90+(idx*0x20));
                delaytone[idx][i2] = idx*0x2000+0x8001;
            }
        }

		// set up the characters. each of the first 30 color sets
		// are one target (to make lighting them easy)
		// Each graphic is 6 chars, so we will skip the first 2
		// for now (to let char 0 be blank)
		// that leaves the last 16 chars (only) for text!
		// We could do some tricks to get a full character set
		// instead or use bitmap mode, but whatever. ;)
		// (we could just use the spare characters and
		// live with the occasional color flash...)
		for (idx = 2; idx < 178; idx += 8) {
			vdpmemcpy(gPattern+(idx*8), tonehit, 6*8);
		}
		for (idx = 178; idx < 242; idx += 8) {
			vdpmemcpy(gPattern+(idx*8), drumhit, 6*8);
		}
		// ball sprite on 1
		vdpmemcpy(gPattern+8, ballsprite, 8);
		
		// and load the limited font
		vdpmemcpy(gPattern+(240*8), font, 16*8);
		vdpmemcpy(gPattern+(176*8), fontjl, 2*8);
		vdpmemcpy(gPattern+(184*8), fontnp, 2*8);
		vdpmemcpy(gPattern+(192*8), fontqt, 2*8);
		vdpmemcpy(gPattern+(200*8), fontvy, 2*8);
		vdpmemcpy(gPattern+(232*8), fontxz, 2*8);

		// now draw the graphics
		// Probably need some background frames, like a scaffold
		for (idx=0; idx<22; idx++) {
			idx_t r=tones[idx*2];
			idx_t c=tones[idx*2+1];
			idx_t ch=idx*8+FIRST_TONE_CHAR;

			vdpscreenchar(VDP_SCREEN_POS(r-1,c-1), ch);
			vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch+1);

			vdpscreenchar(VDP_SCREEN_POS(r-1,c), ch+2);
			vdpscreenchar(VDP_SCREEN_POS(r,c), ch+3);

			vdpscreenchar(VDP_SCREEN_POS(r-1,c+1), ch+4);
			vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+5);
		}

		for (idx=0; idx<8; idx++) {
			idx_t r=drums[idx*2];
			idx_t c=drums[idx*2+1];
			idx_t ch=idx*8+FIRST_DRUM_CHAR;

			vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c-1), ch+1);

			vdpscreenchar(VDP_SCREEN_POS(r,c), ch+2);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c), ch+3);

			vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+4);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c+1), ch+5);
		}

		VDP_SET_REGISTER(VDP_REG_MODE1, x);		// enable the display
#ifdef BUILD_TI99
		VDP_REG1_KSCAN_MIRROR = x;				// must have a copy of VDP Reg 1 if you ever use KSCAN
#endif
	}

    // get the init color table into CPU memory
    vdpmemread(gColor, colortab, 32);

#if 1
	// display the first three lines of the text data - note we need to use a lookup table
	// as our font has only 16 characters in it... (to have more characters would cause
	// letters to flash with this setup)...
	VDP_SET_ADDRESS_WRITE(gImage+(21*32));
	for (idx=0; idx<32*3; ++idx) {
		unsigned char c = textout[idx];
		if (c>96) c-=32;	// make uppercase
		if ((c<32)||(c>90)) {
			VDPWD=132;
		} else {
			VDPWD=charmap[c-32];
		}
	}
#endif

	// look up the song's address (should be 0xA000)
	firstSong = *((unsigned int*)&flags[6]);
	// const is fine for easy storage and patching, but for the decision making on
	// the loop value, we need to pretend it's volatile
	volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

	do {
		// post-song delay to allow it to finish
		finalcount = 30;

		for (idx=0; idx<4; idx++) {
			nOldVol[idx]=0xff;
			nOldTarg[idx]=255;
			nOldTarg[idx+4]=255;
			for (i2=0; i2<DELAYTIME; i2++) {
				// mute the history 
				delayvol[idx][i2]=(idx*0x20) + 0x9f;
			}
		}

		for (idx=0; idx<MAX_SPRITES; idx++) {
			//sprpath[idx].y=SPRITE_OFF<<4;
			sprpath[idx].z=0;
		}

		if (0 != firstSong) {
			StartSong((unsigned char*)firstSong,0);
			// the init for the notes isn't quite right, they are all set
			// to >0001, with no command nibble. Most programs don't care,
			// but we actually do, so reset the defaults (only matters if
			// the song has unused channels, then they interfere...)
			songNote[0]=0x8001;
			songNote[1]=0xa001;
			songNote[2]=0xc001;
			songNote[3]=0xe101;
		}
		delaypos = 0;

		while (finalcount) {
			// no need for a coleco reboot
#ifdef BUILD_TI99
			// check for quit and reboot
			// will be checkquit() in libti99 eventually
			__asm__("li r12,>0024\n\tldcr @>0012,3\n\tsrc r12,7\n\tli r12,6\n\tstcr r0,8\n\tandi r0,>1100\n\tjne 4\n\tblwp @>0000" : : : "r12","r0");
#endif

#ifdef BUILD_TI99
			VDP_WAIT_VBLANK_CRU_STATUS(status);		// waits for int and clears it
#endif
#ifdef BUILD_COLECO
			VDP_WAIT_VBLANK_CRU;					// wait for int
			VDP_CLEAR_VBLANK;						// clear it
#endif

			// copy active sprites into the sprite table
			VDP_SET_ADDRESS_WRITE(gSprite);
			for (idx=0; idx<MAX_SPRITES; ++idx) {
				if (sprpath[idx].z < 0) {
					VDPWD=(sprpath[idx].y>>4)+(sprpath[idx].z);	// integer y position plus z bounce
					VDPWD=sprpath[idx].x>>4;
					VDP_SAFE_DELAY;	// need delay cause no calculation on next one
					VDPWD=1;		// char 1 is the ball
					VDPWD=sprpath[idx].col;
				}
			}
			VDPWD = 0xd0;	// terminate the sprite table

			// now that the screen is set, NOW we can play
			CALL_PLAYER_SN;

			if (!(songActive&SONGACTIVEACTIVE)) {
				// count down frames after song ends
				--finalcount;
			}

			++frame;

			// implement frame
			for (idx=0; idx<4; idx++) {
				idx_t arpvoice = idx + ((frame&1)<<2);	// start at 0 or 4

				// volume is exactly the same as the old player
				delayvol[idx][delaypos] = songVol[idx];
				// tone data is no longer byte swapped
				unsigned int x = songNote[idx];
				delaytone[idx][delaypos] = x;

				// trigger animation
				if (idx != 3) {
					// de-swizzle the note to get frequency code
					x=((x<<4)&0x03f0)|((x>>8)&0x0f);
				}

				// do nothing if muted channel
				idx_t targ = 255;
				if (((songVol[idx]&0x0f) < 0x0f)) {
					if (idx == 3) {
						targ=((x>>8)&0x07);		//+FIRST_DRUM_COLOR;	// added just to be unique?
					} else {
						targ=tonetarget[x];		//+FIRST_TONE_COLOR;	// added to just be unique?
					}

					if ((songVol[idx]+4 < nOldVol[idx])||((targ!=nOldTarg[idx])&&(targ!=nOldTarg[arpvoice]))) {
						// trigger new ball (if available - we let them finish)
						if (sprpath[nextSprite].z == 0) {
#ifdef BUILD_TI99
							int d;
#endif
#ifdef BUILD_COLECO
							signed char d;
#endif
							idx_t y = nextSprite++;
							if (nextSprite >= MAX_SPRITES) nextSprite = 0;	// wraparound
							// find target and source
							idx_t tx,ty;
							// source at the middle
							if (idx == 3) {
								idx_t p = targ*2;
								tx=drums[p+1]*8;
								ty=drums[p]*8;
								d = delayDrums[targ];
							} else {
								idx_t p = targ * 2;
								tx=tones[p+1]*8;
								ty=tones[p]*8-4;
								d = delayTones[targ];
							}
#if DELAYTIME == 15
							d >>= 1;
#endif
							// fill in the path data (12.4 fixed point)
							// paths are not pre-calculated - we calculate the trajectory here
							// calculate distance and delay - total time is based on distance, max DELAYTIME
							sprpath[y].z = (DELAYTIME+1)-d;	// so it's always at least 1

#ifdef BUILD_TI99
							// work around signed divide issues on the TI compiler
							if (tx >= sx) {
								sprpath[y].xstep = ((int)(tx-sx)<<4) / d;
							} else {
								sprpath[y].xstep = -(((int)(sx-tx)<<4) / d);
							}
							if (ty >= sy) {
								sprpath[y].ystep = ((int)(ty-sy)<<4) / d;
							} else {
								sprpath[y].ystep = -(((int)(sy-ty)<<4) / d);
							}
#endif
#ifdef BUILD_COLECO
							// signed division should be fine on the Coleco compiler
							sprpath[y].xstep = ((int)(tx-sx)<<4) / d;
							sprpath[y].ystep = ((int)(ty-sy)<<4) / d;
#endif

#if DELAYTIME == 30
							// biggest negative z should be -120 (30 down to 1)
							sprpath[y].zstep = -(d/2);	// half time up, half time down
#elif DELAYTIME == 15
							sprpath[y].zstep = -d;	// still half time up, half time down, but step farther
#endif

							sprpath[y].x = (sx<<4);
							sprpath[y].y = (sy<<4);	// need to set this after the delay
							//sprpath[y].z = 0;		// used above for delay

							sprpath[y].col=colorchan[idx]>>4;
						} else {
							// we missed this cycle, but scan ahead for the next one!
							nextSprite++;
							if (nextSprite >= MAX_SPRITES) nextSprite = 0;	// wraparound
						}
					}
				}

				// remember these values
				nOldVol[idx]=songVol[idx];
				nOldTarg[arpvoice]=targ;	// this may be idx or idx+4
			}
			// next position
			++delaypos;
			if (delaypos>=DELAYTIME) delaypos=0;

			// fade out any colors
			{
				static idx_t lastFade = 0;
				lastFade &= 0x1f;
				for (idx=0; idx<4; ++idx) {
					colortab[lastFade]=colorfade[colortab[lastFade]>>4];
					++lastFade;
				}

				// play it out (and set any fresh colors)
				for (idx=0; idx<3; idx++) {
					// tone is in correct order here
					unsigned int x = delaytone[idx][delaypos];
#ifdef BUILD_TI99
					__asm__(
						"movb %0,@>8400\n\t"    
						"swpb %0\n\t"
						"movb %0,@>8400\n\t"
						"swpb %0"
						:
						: "r"(x)
					);
#endif
#ifdef BUILD_COLECO
					SOUND = x>>8;
					SOUND = x&0xff;
#endif
					// and volume just is
					SOUND = delayvol[idx][delaypos];

					if ((delayvol[idx][delaypos]&0x0f) < 0x0f) {
						// de-swizzle the note
						x=((x<<4)&0x03f0)|((x>>8)&0x0f);
						unsigned char set = tonetarget[x] + FIRST_TONE_COLOR;
						colortab[set] = colorchan[idx];
					}
				}

				// noise channel - similar but different
				{
					// noise is in msb here
					unsigned int x = delaytone[3][delaypos];
#ifdef BUILD_TI99
					__asm__(
						"movb %0,@>8400"
						:
						: "r"(x)
					);
#endif
#ifdef BUILD_COLECO
					SOUND = x>>8;
#endif
					// and volume just is
					SOUND = delayvol[3][delaypos];

					if ((delayvol[3][delaypos]&0x0f) < 0x0f) {
						idx_t set = ((x>>8)&0x7)+FIRST_DRUM_COLOR;
						colortab[set] = colorchan[3];
					}
				}

				// write it back
				vdpmemcpy(gColor, colortab, 32);
			}

			// and animate the sprites
			for (idx = 0; idx<MAX_SPRITES; idx++) {
				// if it's not active at all
				if (sprpath[idx].z == 0) continue;

				// if it's being delayed
				if (sprpath[idx].z > 0) {
					--sprpath[idx].z;
					// the 1 offset was causing late balls
					if (sprpath[idx].z > 1) {
						continue;
					} else {
						// reset it, since otherwise we'll be off by 1
						sprpath[idx].z = sprpath[idx].zstep;
					}
				} else {
					// otherwise move it
					sprpath[idx].z += sprpath[idx].zstep;
				}

				// end when it hits bottom of arc
				if (sprpath[idx].z >= 0) {
					sprpath[idx].z = 0;
					continue;
				}

				// update step for gravity
#if DELAYTIME == 30
				++sprpath[idx].zstep;
#elif DELAYTIME == 15
				sprpath[idx].zstep += 2;
#endif

				// move towards the target - this is a straight line move, z fakes the arc for us
				sprpath[idx].x += sprpath[idx].xstep;
				sprpath[idx].y += sprpath[idx].ystep;
				// no need to check, since we use the arc to end it
			}
		}
		
        // loop, chain, or reset if it is finished
        // chain is normally set by a loader and takes priority
		unsigned int *chain = (unsigned int *)(*((unsigned int*)(&flags[14])));
        if (chain) {
            // look up the value to chain to
            unsigned int chained = *chain;
            if (chained) {
                // need to trampoline to the specified cartridge page
                // TI and Coleco each have different code and targets
                memcpy((void*)0x8320, tramp, sizeof(tramp));   // 0x8320 so we don't overwrite the C registers
                *((unsigned int*)0x8322) = chained;     // patch the pointer
                ((void(*)())0x8320)();                  // call the function, never return
            }
        }
		
	} while (*pLoop);

	// mute the sound chip
	SOUND=0x9F;
	SOUND=0xBF;
	SOUND=0xDF;
	SOUND=0xFF;

	// else reboot
	return 0;
}

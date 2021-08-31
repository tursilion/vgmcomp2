// Fourth musical animation - Tursi 2020
// ported to quickplayer system 2021
// TI only at this point

// uses libti99 and libtivgm2
#include "vdp.h"
#include "sound.h"
#include "system.h"
#include <TISNPlay.h>
#include "ani1.c"
void *memcpy(void *destination, const void *source, int num);

#define FIRST_TONE_CHAR 2
#define FIRST_DRUM_CHAR 178
#define FIRST_TONE_COLOR 0
#define FIRST_DRUM_COLOR 22

#define SPRITE_OFF 193

// MUST be power of 2
#define MAX_SPRITES 32
struct SPRITE {
	// hardware struct - order matters
	unsigned char y,x;
	unsigned char ch,col;
} spritelist[MAX_SPRITES];

struct SPRPATH {
	unsigned int delay;
	int xstep,ystep,zstep;
	unsigned int x,y;
	int z;
} sprpath[MAX_SPRITES];

// buffer for working with color table in CPU RAM
unsigned char colortab[32];

// ring buffer for delaying audio - multiple of 15 steps
// sadly, yes, 15 and not 16, which we could mask
#define DELAYTIME 30
unsigned char delayvol[4][DELAYTIME];
unsigned int delaytone[4][DELAYTIME];
int delaypos, finalcount;
unsigned int status;
unsigned int nOldVol[4];
unsigned int nOldVoice[4];

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
// TODO: we should display at least some of it...
const unsigned char textout[768] = {
    "~~~~DATAHERE~~~~\0"
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
const unsigned char flags[18] = {
    "~~FLAGxxyySSSL\0\0:"
};

unsigned int firstSong;     // for SN

int main() {
	int nextSprite = 0;

	// init the screen
	{
		int x = set_graphics_raw(VDP_SPR_8x8);	// set graphics mode with 8x8 sprites
		vdpmemset(gColor, 0x10, 32);			// all colors to black on transparent
		vdpmemset(gPattern, 0, 8);				// char 0 is blank
		vdpmemset(gImage, 0, 768);				// clear screen to char 0
		vdpchar(gSprite, 0xd0);					// all sprites disabled
		VDP_SET_REGISTER(VDP_REG_COL, COLOR_MAGENTA);	// background color

        // before we go too far, patch the music library NOT to play audio
        // by writing to ROM instead. There's just one place to change, and
        // we'll search for it rather than assuming. This is more reliable
        // than a binary patch but it DOES mean we can't run this from ROM.
        {
            unsigned int *pSrch = (unsigned int*)SongLoop;
            unsigned int x = 50;   // somewhere in the first 50 words
            while (x--) {
                if (*pSrch == 0x8400) {
                    // here it is
                    *pSrch = 0;
                    break;
                }
                ++pSrch;
            }
            // imperfect test, but there's no reason to expect this to change...
            if (*pSrch) {
                // do we even have a character set? ;)
                vdpmemcpy(0,"bad lib", 7);
                VDP_SET_REGISTER(VDP_REG_MODE1, x);		// enable the display
                halt();
            }
        }
        
        // preload the buffer with mutes
        for (int idx=0; idx<4; ++idx) {
            for (int i2=0; i2<DELAYTIME; ++i2) {
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
		for (int idx = 2; idx < 178; idx += 8) {
			vdpmemcpy(gPattern+(idx*8), tonehit, 6*8);
		}
		for (int idx = 178; idx < 242; idx += 8) {
			vdpmemcpy(gPattern+(idx*8), drumhit, 6*8);
		}
		// ball sprite on 1
		vdpmemcpy(gPattern+8, ballsprite, 8);

		// now draw the graphics
		// Probably need some background frames, like a scaffold
		for (int idx=0; idx<22; idx++) {
			int r=tones[idx*2];
			int c=tones[idx*2+1];
			int ch=idx*8+FIRST_TONE_CHAR;

			vdpscreenchar(VDP_SCREEN_POS(r-1,c-1), ch);
			vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch+1);

			vdpscreenchar(VDP_SCREEN_POS(r-1,c), ch+2);
			vdpscreenchar(VDP_SCREEN_POS(r,c), ch+3);

			vdpscreenchar(VDP_SCREEN_POS(r-1,c+1), ch+4);
			vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+5);
		}

		for (int idx=0; idx<8; idx++) {
			int r=drums[idx*2];
			int c=drums[idx*2+1];
			int ch=idx*8+FIRST_DRUM_CHAR;

			vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c-1), ch+1);

			vdpscreenchar(VDP_SCREEN_POS(r,c), ch+2);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c), ch+3);

			vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+4);
			vdpscreenchar(VDP_SCREEN_POS(r+1,c+1), ch+5);
		}

		VDP_SET_REGISTER(VDP_REG_MODE1, x);		// enable the display
		VDP_REG1_KSCAN_MIRROR = x;				// must have a copy of VDP Reg 1 if you ever use KSCAN
	}

    // get the init color table into CPU memory
    vdpmemread(gColor, colortab, 32);

	// look up the song's address (should be 0xA000)
	firstSong = *((unsigned int*)&flags[6]);
	// const is fine for easy storage and patching, but for the decision making on
	// the loop value, we need to pretend it's volatile
	volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

	do {
		// post-song delay to allow it to finish
		finalcount = 30;

		for (int idx=0; idx<4; idx++) {
			nOldVol[idx]=0xff;
			nOldVoice[idx]=0;
			for (int i2=0; i2<DELAYTIME; i2++) {
				// mute the history 
				delayvol[idx][i2]=(idx*0x20) + 0x9f;
			}
		}

		for (int idx=0; idx<MAX_SPRITES; idx++) {
			spritelist[idx].y=SPRITE_OFF;
			spritelist[idx].ch=1;
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
			// check for quit and reboot
			// will be checkquit() in libti99 eventually
			__asm__("li r12,>0024\n\tldcr @>0012,3\n\tsrc r12,7\n\tli r12,6\n\tstcr r0,8\n\tandi r0,>1100\n\tjne 4\n\tblwp @>0000" : : : "r12","r0");

			VDP_WAIT_VBLANK_CRU_STATUS(status);		// waits for int and clears it
			vdpmemcpy(gSprite, (unsigned char*)&spritelist[0], 128);

			// now that the screen is set, NOW we can play
			CALL_PLAYER_SN;

			if (!(songActive&SONGACTIVEACTIVE)) {
				// count down frames after song ends
				--finalcount;
			}

			// implement frame
			for (int idx=0; idx<4; idx++) {
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
				int targ = -1;
				if (((songVol[idx]&0x0f) < 0x0f)) {
					if (idx == 3) {
						targ=((x>>8)&0x07)+FIRST_DRUM_COLOR;
					} else {
						targ=tonetarget[x] + FIRST_TONE_COLOR;
					}

					if ((songVol[idx]+4 < nOldVol[idx])||(targ!=nOldVoice[idx])) {
						// trigger new ball (if available - we let them finish)
						unsigned int y = nextSprite++;
						nextSprite &= (MAX_SPRITES-1);  // wraparound
						// this could steal delayed sprites...
						if (spritelist[y].y == SPRITE_OFF) {
							// find target and source
							unsigned int tx,ty;
							// source at the middle
							const unsigned int sx=124,sy=15*8;
							if (idx == 3) {
								unsigned int p = ((x>>8)&0x07)*2;
								tx=drums[p+1]*8;
								ty=drums[p]*8;
							} else {
								unsigned int p = tonetarget[x] * 2;
								tx=tones[p+1]*8;
								ty=tones[p]*8-4;
							}

							// fill in the path data (12.4 fixed point)
							// paths are not pre-calculated - we calculate them here
							// calculate distance and delay - total time is DELAYTIME
							// a bit wasteful we have to cue the ball up but not launch it,
							// but I guess that's okay
							{
								int d;

								if (tx >= sx) {
									d = tx-sx;
								} else {
									d = sx-tx;
								}
								if (ty >= sy) {
									d += ty-sy;
								} else {
									d += sy-ty;
								}
								// now we have a+b=d, not quite a^2+b^2=c^2, but close enough
								// based on math, a divider of 8 should give a max count of 15. Yay!
								d >>= 2;	// 3 for DELAYTIME=15
								if (d > DELAYTIME) d=DELAYTIME;
								sprpath[y].delay = DELAYTIME-d;

								// work around signed divide issues
								if (tx >= sx) {
									sprpath[y].xstep = ((tx-sx)<<4) / d;
								} else {
									sprpath[y].xstep = -(((sx-tx)<<4) / d);
								}
								if (ty >= sy) {
									sprpath[y].ystep = ((ty-sy)<<4) / d;
								} else {
									sprpath[y].ystep = -(((sy-ty)<<4) / d);
								}
								sprpath[y].zstep = -(d/2);	// half time up, half time down
								sprpath[y].x = (sx<<4);
								sprpath[y].y = (sy<<4);
								sprpath[y].z = 0;

								spritelist[y].col=colorchan[idx]>>4;
								spritelist[y].y=SPRITE_OFF+1;	// prepare to show on first frame
							}
						} 
					}
				}

				// remember these values
				nOldVol[idx]=songVol[idx];
				nOldVoice[idx]=targ;
			}
			// next position
			++delaypos;
			if (delaypos>=DELAYTIME) delaypos=0;

			// fade out any colors
			{
				static int lastFade = 0;
				lastFade &= 0x1f;
				for (int idx=0; idx<4; ++idx) {
					colortab[lastFade]=colorfade[colortab[lastFade]>>4];
					++lastFade;
				}

				// play it out (and set any fresh colors)
				for (int idx=0; idx<3; idx++) {
					// tone is in correct order here
					unsigned int x = delaytone[idx][delaypos];
					__asm__(
						"movb %0,@>8400\n\t"    
						"swpb %0\n\t"
						"movb %0,@>8400\n\t"
						"swpb %0"
						:
						: "r"(x)
					);
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
					__asm__(
						"movb %0,@>8400"
						:
						: "r"(x)
					);
					// and volume just is
					SOUND = delayvol[3][delaypos];

					if ((delayvol[3][delaypos]&0x0f) < 0x0f) {
						unsigned int set = ((x>>8)&0x7)+FIRST_DRUM_COLOR;
						colortab[set] = colorchan[3];
					}
				}

				// write it back
				vdpmemcpy(gColor, colortab, 32);
			}

			// and animate the sprites
			for (int idx = 0; idx<MAX_SPRITES; idx++) {
				if (sprpath[idx].delay) {
					--sprpath[idx].delay;
					if (0 != sprpath[idx].delay) {
						continue;
					}
				} else {
					if (spritelist[idx].y == SPRITE_OFF) continue;
				}

				sprpath[idx].z += sprpath[idx].zstep;
				++sprpath[idx].zstep;

				// end when it hits bottom of arc
				if (sprpath[idx].z >= 0) {
					spritelist[idx].y = SPRITE_OFF;
					continue;
				}

				sprpath[idx].x += sprpath[idx].xstep;
				sprpath[idx].y += sprpath[idx].ystep;
				// no need to check, since we use the arc to end it

				spritelist[idx].y=(sprpath[idx].y>>4)+(sprpath[idx].z);
				spritelist[idx].x=(sprpath[idx].x>>4);
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

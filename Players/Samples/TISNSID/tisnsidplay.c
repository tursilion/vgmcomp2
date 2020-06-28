// example standalone player for the TI
//
// This is the music player for the combined SN and SID chip.
// It's just a slightly modified version of the TI SN player.
// there are far more examples and notes in that one.
// six tone channels at once!

// Sorry, I know it's a bit discordant... the data is not
// properly tuned and even played fully on the SN it's
// not completely right - in fact I think it's most the SN
// that's out of tune. But at least it shows two players running.

#include <TISNPlay.h>
#include <TISIDPlay.h>
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char snsong[];
extern const unsigned char sidSong[];

// to be called from the user interrupt hook
// Note that this call, since it shares the workspace with C,
// will blow away ALL the registers except R0 and R10, so make
// sure you only call it from a tightly controlled area where
// there's no state to preserve, or likewise only enable
// interrupts in such a condition. If you don't know, then
// use the inline version that tags the variables as trashed,
// and GCC will figure out what needs to be saved. 
// Naturally, if you are using the slower C version and not
// the hand-editted versions, then you don't need to worry
// about that, GCC takes care of the registers.

// Option 3: use the hand tuned asm code directly with register preservation
// Have to mark all regs as clobbered. Determine vblank any way you like
// (I recommend VDP_WAIT_VBLANK_CRU), and then include this define "CALL_PLAYER;"
// This is probably the safest for the hand-tuned code
// This calls BOTH functions SN and SID.
#define CALL_PLAYER \
    __asm__(                                                        \
        "bl @SongLoop"                                              \
        "\n\tbl @SongSID"                                           \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

inline void faster_hexprint2(int x) {
    faster_hexprint(x&0xff);
    faster_hexprint(x>>8);
}
inline void faster_hexprint3(int x) {
    faster_hexprint(x>>8);
    faster_hexprint(x&0xff);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
	
	// make sure SN noise is muted
    SOUND = 0xff;

    // turn off unused console interrupt flags (just saves a little CPU time)
    VDP_INT_CTRL = VDP_INT_CTRL_DISABLE_SPRITES | VDP_INT_CTRL_DISABLE_SOUND;

    // set up the sprites - with six, some may flicker. We'll see.
    for (int idx=0; idx<6; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*40+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

    // start the song
    // ab_sid has two tone channels and one noise, so set that up
    SidCtrl1 = SID_TONE_PULSE;
    SidCtrl2 = SID_TONE_PULSE;
    SidCtrl3 = SID_TONE_PULSE;
    StartSID((unsigned char*)sidSong, 0);
    // and the SN one too
    StartSong((unsigned char*)snsong, 0);

    // now play it - space to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER;		// calls both song loops

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        // only 5 chars per channel now...
        // this gets a bit slow and the bar graphs are prettier...
#if 0
        // SN first
        for (int idx=0; idx<3; ++idx) {
            int row = songNote[idx];
            // print note and volume data
            faster_hexprint3(row);
            row = songVol[idx]&0xf;
            if (row > 9) {
				VDPWD = row+'0'+7;
			} else {
				VDPWD = row+'0';
			}
        }
        // then SID
        for (int idx=0; idx<3; ++idx) {
            int row = sidNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            row = (sidVol[idx]>>4)&0xf;
            if (row > 9) {
				VDPWD = row+'0'+7;
			} else {
				VDPWD = row+'0';
			}
        }
#endif

        // do a simple little viz
        // SN first
        for (int idx=0; idx<3; ++idx) {
            unsigned int row = songNote[idx];
			// turn note into a sprite altitude
			// *2/9 is the same as /4.5
			row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;

            vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = songVol[idx]&0xf;
            vchar(7, (idx*5)+4, 32, row);
            vchar(row+7, idx*5+4, 43, 15-row);
        }
        // then SID
        for (int idx=0; idx<3; ++idx) {
			// rough range from 1500 to 5500, and invert
            unsigned int row = (5500 - ((sidNote[idx]>>8)+(sidNote[idx]<<8))) - 1500;

			// turn note into a sprite altitude
            row /= 24;
            vdpchar(gSprite+((idx+3)<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = (sidVol[idx]&0xf0)>>4;
            vchar(7, (idx*5)+4+15, 32, row);
            vchar(row+7, (idx*5)+4+15, 43, 15-row);
        }

        // check if still playing - either should do - they SHOULD be the same size...
        if (!(sidNote[3]&SONGACTIVEACTIVE)) {
            // mute it!!
            *((volatile unsigned char *)0x5808) = 0;
            *((volatile unsigned char *)0x5816) = 0;
            *((volatile unsigned char *)0x5824) = 0;
            // just in case, same with SN
            SOUND = 0xdf;
            SOUND = 0xbf;
            SOUND = 0x9f;
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(5);
            if (KSCAN_KEY == ' ') {
				StartSID((unsigned char*)sidSong, 0);
				StartSong((unsigned char*)snsong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
            }
        }
    }

    return 0;
}

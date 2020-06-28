// example standalone player for the TI
//
// This is the music player for the TI SID chip.
// It's just a slightly modified version of the TI SN player.
// there are far more examples and notes in that one.

#include "CSIDPlay.h"
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char mysong[];

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
// After further testing, we do have to flag ALL registers clobbered, however
// GCC can still deal with this without saving everything every time.
// Determine vblank any way you like (I recommend VDP_WAIT_VBLANK_CRU), 
// and then include this define "CALL_PLAYER;"
// This is probably the safest for the hand-tuned code
#define CALL_PLAYER \
    __asm__(                                                        \
        "bl @SongSID"                                               \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","r12","r13","r14","r15","cc"   \
        )

inline void faster_hexprint2(int x) {
    faster_hexprint(x&0xff);
    faster_hexprint(x>>8);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);

    // turn off unused console interrupt flags (just saves a little CPU time)
    VDP_INT_CTRL = VDP_INT_CTRL_DISABLE_SPRITES | VDP_INT_CTRL_DISABLE_SOUND;

    // set up the sprites
    for (int idx=0; idx<3; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

    // start the song
    // ab_sid has two tone channels and one noise, so set that up
    SidCtrl1 = SID_TONE_PULSE;
    SidCtrl2 = SID_TONE_PULSE;
    SidCtrl3 = SID_TONE_NOISE;
    StartSID((unsigned char*)mysong, 0);

    // now play it - space to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER;

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (int idx=0; idx<3; ++idx) {
            int row = sidNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            VDPWD = ' ';
            faster_hexprint(sidVol[idx]);
            VDPWD = ' ';
        }

        // do a simple little viz
        for (int idx=0; idx<3; ++idx) {
            unsigned int row = 65535 - ((sidNote[idx]>>8)+(sidNote[idx]<<8));

			// turn note into a sprite altitude
            // SID uses bigger numbers for higher notes, so invert it
            row /= 390;
            vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = (sidVol[idx]&0xf0)>>4;
            vchar(7, (idx<<3)+6, 32, row);
            vchar(row+7, (idx<<3)+6, 43, 15-row);
        }

        // check if still playing
        if (!(sidNote[3]&SONGACTIVEACTIVE)) {
            // mute it!!
            *((volatile unsigned char *)0x5808) = 0;
            *((volatile unsigned char *)0x5816) = 0;
            *((volatile unsigned char *)0x5824) = 0;
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(5);
            if (KSCAN_KEY == ' ') {
                StartSID((unsigned char*)mysong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
            }
        }
    }

    return 0;
}

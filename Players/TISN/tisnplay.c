// example standalone player for the TI

#include "../CPlayer/CPlayer.h"
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

#if 0
// Option 1: Use the C version of SongLoop from the interrupt hook
// this version can be used to call the C version of SongLoop from the
// user interrupt if your code is running C. To use this, just
// add "VDP_INT_HOOK = wrapInt;" to your code, and use either "vdpwaitvint()"
// or "VDP_INT_POLL" in your main loop to make it fire. Note this version
// uses a second workspace at >8320 to preserve the C workspace.
void wrapInt() {
    __asm__(
        "mov @0x8314,@0x8334\n\t" // move C stack pointer (r10) to new workspace stack pointer
        "lwpi 0x8320\n\t"        // new workspace at >8320

        "bl @SongLoop\n\t"      // call the player

        "lwpi 0x83e0\n\t"        // back to GPLWS
        "b *r11"                // back to interrupt
    );  // don't need to tell C about any clobbered registers, its our workspace
}
#elif 0
// Option 2: call SongLoop directly from your main loop.
// No setup needed. Poll for vblank any way you like (I recommend VDP_WAIT_VBLANK_CRU)
// and then just call SongLoop() once per frame.
// This is the safest way for C code to call the C version
#define CALL_PLAYER SongLoop()
#elif 1
// Option 3: use the hand tuned asm code directly with register preservation
// This version tags all the GCC registers except 12-15 as clobbered, and GCC
// will have to decide whether to save them based on your code.
// Note it DOES clobber 12-15, I just don't think they need to be preserved.
// If I'm wrong, please do add them. Determine vblank any way you like
// (I recommend VDP_WAIT_VBLANK_CRU), and then include this define "CALL_PLAYER;"
// This is probably the safest for the hand-tuned code
#define CALL_PLAYER \
    __asm__(                                                        \
        "bl @SongLoop"                                              \
        : /* no outputs */                                          \
        : /* no arguments */                                        \
        : "r0","r1","r2","r3","r4","r5","r6","r7","r8","r9","r11","cc"   \
        )
#endif

extern unsigned int cntInline,cntRle,cntRle16,cntRle24,cntRle32,cntBack;

inline void faster_hexprint2(int x) {
    faster_hexprint(x>>8);
    faster_hexprint(x&0xff);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);

#if 1
    // see what's broken
    PlayerUnitTest();
    for (;;) {
        kscan(5);
        if (KSCAN_KEY == 32) break;
    }
    vdpmemset(gImage, ' ', 768);
#endif

    // turn off unused console interrupt flags (just saves a little CPU time)
    VDP_INT_CTRL = VDP_INT_CTRL_DISABLE_SPRITES | VDP_INT_CTRL_DISABLE_SOUND;

    // set up the sprites (noise doesn't need one)
    for (int idx=0; idx<4; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

    // start the song
    StartSong((unsigned char*)mysong, 0);

    // now play it - space to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER;

#if 1
        VDP_SET_ADDRESS_WRITE(gImage);
        faster_hexprint2(cntInline);
        VDPWD=' ';
        faster_hexprint2(cntRle);
        VDPWD=' ';
        faster_hexprint2(cntRle16);
        VDPWD=' ';
        faster_hexprint2(cntRle24);
        VDPWD=' ';
        faster_hexprint2(cntRle32);
        VDPWD=' ';
        faster_hexprint2(cntBack);
#endif

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (int idx=0; idx<4; ++idx) {
            int row = songNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            VDPWD = ' ';
            faster_hexprint(songVol[idx]);
            VDPWD = ' ';
        }

        // do a simple little viz (noise note isn't visible, so doesn't matter what we write there)
        for (int idx=0; idx<4; ++idx) {
            int row = songNote[idx];

			if (idx != 3) {
				// turn note into a sprite altitude
				// *2/9 is the same as /4.5
				row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
			} else {
			    // noise is just 0-15
			    row = ((row&0x0f00)>>8)*11;
			}
			vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = songVol[idx]&0xf;
            vchar(7, (idx<<3)+6, 32, row);
            vchar(row+7, (idx<<3)+6, 43, 15-row);
        }

        if (!(songNote[3]&SONGACTIVEACTIVE)) {
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(5);
            if (KSCAN_KEY == ' ') {
                StartSong((unsigned char*)mysong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
            }
        }
    }

    return 0;
}

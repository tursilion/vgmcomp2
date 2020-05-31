// example standalone player for the TI

#include "../CPlayer/CPlayer.h"
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char mysong[];

// to be called from the user interrupt hook
// Note when we are calling from the user interrupt, it's not
// safe to use the C registers, unless we tell GCC here to save
// ALL of them (which is pretty expensive and uses at least
// as much memory off the stack as a separate WS would).
// However, if you poll and call SongLoop from your main code,
// the GCC will take care of the register usage for you.
void wrapInt() {
    __asm__(
        "mov @>8314,@>8334\n\t" // move C stack pointer (r10) to new workspace stack pointer
        "lwpi >8320\n\t"        // new workspace at >8320
        "bl @SongLoop\n\t"      // call the player
        "lwpi >83e0\n\t"        // back to GPLWS
        "b *r11"                // back to interrupt
    );  // don't need to tell C about any clobbered registers, its our workspace
}


int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);

    // set interrupt hook
    VDP_INT_HOOK = wrapInt;
    // turn off unused console interrupt flags (just saves a little CPU time)
    VDP_INT_CTRL = VDP_INT_CTRL_DISABLE_SPRITES | VDP_INT_CTRL_DISABLE_SOUND;

    // set up the sprites (noise doesn't need one)
    for (int idx=0; idx<3; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

    // start the song
    StartSong((unsigned char*)mysong, 0);

    // now play it - no looping (songActive printed though)
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (int idx=0; idx<4; ++idx) {
            int row = songNote[idx];
            // print note and volume data
            faster_hexprint(row>>8);
            faster_hexprint(row&0xff);
            VDPWD = ' ';
            faster_hexprint(songVol[idx]);
            VDPWD = ' ';
        }

        // do a simple little viz (noise note isn't visible, so doesn't matter what we write there)
        for (int idx=0; idx<4; ++idx) {
            int row = songNote[idx];

            // turn note into a sprite altitude
            // *2/9 is the same as /4.5
            row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
            vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = songVol[idx]&0xf;
            vchar(7, (idx<<3)+6, 32, row);
            vchar(row+7, (idx<<3)+6, 43, 15-row);
        }

        if (!songActive) {
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

// example standalone player for the TI
//
// This is the music player for the TI SID chip.
// It's just a slightly modified version of the TI SN player.
// there are far more examples and notes in that one.

#include <TISIDPlay.h>
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char mysong[];

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
    VDPWD(0xd0);

    // start the song
    // ab_sid has two tone channels and one noise, so set that up
    SidCtrl1 = SID_TONE_PULSE;
    SidCtrl2 = SID_TONE_PULSE;
    SidCtrl3 = SID_TONE_NOISE;
    StartSID((unsigned char*)mysong, 0);

    // now play it - space to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER_SID;

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (int idx=0; idx<3; ++idx) {
            int row = sidNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            VDPWD(' ');
            faster_hexprint(sidVol[idx]);
            VDPWD(' ');
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
        if (!isSIDPlaying) {
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

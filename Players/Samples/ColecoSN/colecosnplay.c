// example standalone player for the Coleco
// This is the music player for the TI SN chip.
// requires libti99coleco and libcolecovgm2

#include <ColecoSNPlay.h>
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char mysong[];
extern void PlayerUnitTest();

inline void faster_hexprint2(int x) {
    faster_hexprint(x>>8);
    faster_hexprint(x&0xff);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
    charset();
    vdpmemset(gColor, 0xe0, 32);

#if 0
    // see what's broken
    PlayerUnitTest();
    for (;;) {
        kscan(1);
        if (KSCAN_KEY == JOY_FIRE) break;
    }
    vdpmemset(gImage, ' ', 768);
#endif

    // set up the sprites
    for (unsigned char idx=0; idx<4; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

	// screen color
	VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);

    // start the song
    StartSong((unsigned char*)mysong, 0);

    // now play it - space to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER_SN;

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (unsigned char idx=0; idx<4; ++idx) {
            int row = songNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            VDPWD = ' ';
            faster_hexprint(songVol[idx]);
            VDPWD = ' ';
        }

        // do a simple little viz
        for (unsigned char idx=0; idx<4; ++idx) {
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

        // check if still playing
        if (!isSNPlaying) {
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(1);
            if (KSCAN_KEY == JOY_FIRE) {
                StartSong((unsigned char*)mysong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
            }
        }
    }

    return 0;
}

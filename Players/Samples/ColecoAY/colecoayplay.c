// example standalone player for the Coleco
// This is the music player for the AY-3-8910 chip (SGM)
// requires libti99coleco and libcolecovgm2

#include <ColecoAYPlay.h>
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char mysong[];

inline void faster_hexprint2(int x) {
    faster_hexprint(x>>8);
    faster_hexprint(x&0xff);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    charset();
    vdpmemset(gColor, 0xe0, 32);

    // set up the sprites
    for (unsigned char idx=0; idx<4; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

	// screen color
	VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);

    // start the song
    ay_StartSong((unsigned char*)mysong, 0);

    // now play it - fire to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER_AY;

        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // do the note text first so it doesn't need the VDP address reset
        for (unsigned char idx=0; idx<4; ++idx) {
            int row = ay_songNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            VDPWD = ' ';
            faster_hexprint(ay_songVol[idx]);
            VDPWD = ' ';
        }

        // do a simple little viz
        for (unsigned char idx=0; idx<3; ++idx) {
			// turn note into a sprite altitude
            unsigned int row = ay_songNote[idx] / 6;
			vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = ay_songVol[idx]&0xf;
            vchar(7, (idx<<3)+6, 32, row);
            vchar(row+7, (idx<<3)+6, 43, 15-row);
        }
        // special case for noise - pull volume from elsewhere
        {
            unsigned int row = ay_songNote[3];

			// noise is just 0-15
			row = ((row&0x0f00)>>8)*11;
            vdpchar(gSprite+(3<<2), row);    // first value in each sprite is row

            // this is the ACTUAL mixer command written
            unsigned char mix = ay_songVol[3];
            if ((mix&0x20) == 0) row = ay_songVol[2];
            else if (0 == (mix&0x10)) row = ay_songVol[1];
            else if (0 == (mix&0x08)) row = ay_songVol[0];
            else row = 0;
            row &= 0xf;

            // draw a bargraph over the volume
            vchar(7, (3<<3)+6, 32, row);
            vchar(row+7, (3<<3)+6, 43, 15-row);
        }

        // check if still playing
        if (!isAYPlaying) {
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(1);
            if (KSCAN_KEY == JOY_FIRE) {
                ay_StartSong((unsigned char*)mysong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
            }
        }
    }

    return 0;
}

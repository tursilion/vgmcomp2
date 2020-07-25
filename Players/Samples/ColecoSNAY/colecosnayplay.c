// example standalone player for the Coleco
// This is the music player for the SN76489 AND AY-3-8910 chip (SGM) together
// requires libti99coleco and libcolecovgm2

// Unfortunately, this is currently too slow to be very practical,
// it barely manages to pull off this viz...

#include <ColecoSNPlay.h>
#include <ColecoAYPlay.h>
#include <sound.h>
#include <vdp.h>
#include <kscan.h>

extern const unsigned char snsong[];
extern const unsigned char aysong[];

inline void faster_hexprint2(int x) {
    faster_hexprint(x>>8);
    faster_hexprint(x&0xff);
}

int main() {
    // set screen
    set_graphics(VDP_MODE1_SPRMAG);
    charset();
    vdpmemset(gColor, 0xe0, 32);
    SOUND = 0xff;       // make sure SN noise is muted

    // set up the sprites
    for (unsigned char idx=0; idx<6; ++idx) {
        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*40+16);
    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
    VDPWD = 0xd0;

	// screen color
	VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);

    // start the song
    ay_StartSong((unsigned char*)aysong, 0);
    StartSong((unsigned char*)snsong, 0);

    // now play it - fire to loop
    for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER_AY;
        CALL_PLAYER_SN;

        // we don't do the text, as it slows things down
#if 0
        // output some proof we're running
        // note faster_hexprint doesn't update (or use!) the cursor position
        VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);

        // SN first
        for (unsigned char idx=0; idx<3; ++idx) {
            int row = songNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            unsigned char t = songVol[idx]&0x0f;
            if (t > 9) {
                VDPWD = t + '0' + 7;
            } else {
                VDPWD = t + '0';
            }
        }

        // then AY
        for (unsigned char idx=0; idx<3; ++idx) {
            unsigned int row = ay_songNote[idx];
            // print note and volume data
            faster_hexprint2(row);
            unsigned char t = ay_songVol[idx]&0x0f;
            if (t > 9) {
                VDPWD = t + '0' + 7;
            } else {
                VDPWD = t + '0';
            }
        }
#endif

        // do a simple little viz
        // SN first
        for (unsigned char idx=0; idx<3; ++idx) {
            unsigned char row = songNote[idx]&0xff; // MSB is in the LSB position
            row <<= 1;
			vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            row = songVol[idx]&0xf;
            unsigned char col = (idx*5)+4;
            vchar(7, col, 32, row);
            vchar(row+7, col, 43, 15-row);
        }
        // then AY
        for (unsigned char idx=0; idx<3; ++idx) {
			// turn note into a sprite altitude
            unsigned char row = ay_songNote[idx] >> 3;
			vdpchar(gSprite+((idx+3)<<2), row);    // first value in each sprite is row

            // draw a bargraph over the volume
            unsigned char t = 15-(ay_songVol[idx]&0xf);
            unsigned char col = (idx*5)+4+15;
            vchar(7, col, 32, t);
            vchar(t+7, col, 43, 15-t);
        }

        // check if still playing - either should do!
        if (!isAYPlaying) {
            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(1);
            if (KSCAN_KEY == JOY_FIRE) {
                ay_StartSong((unsigned char*)aysong, 0);
                StartSong((unsigned char*)snsong, 0);
                VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
            }
        }
    }

    return 0;
}

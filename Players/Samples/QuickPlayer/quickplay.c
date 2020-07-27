// Quickplayer for GCC using libti99 and libtivgm2
// Just a quick example that the Windows tool fills in for easy usage.

#include <vdp.h>
#include <sound.h>
#ifdef BUILD_TI99
#include <TISNPlay.h>
#include <TISIDPlay.h>
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
#include <ColecoAYPlay.h>
#endif

// Windows app formats a full screen into here
// the four bytes after it are the pointers to
// the first song, and then the second. The first
// song is usually at >A000 and the second is
// whereever it fits. Either can be 0 for no song.
// data is in appropriate endian for the system,
// and the second chip is AY or SID depending on system.
// The next 3 bytes are the SID configuration registers,
// used by the TI only. Coleco does not use these at all.
// Finally, the last byte is a loop flag - 0 for no,
// anything else for yes.
const unsigned char textout[768+4+4] = {
    "~~~~DATAHERE~~~~\0"
};
unsigned int firstSong;
unsigned int secondSong;

int main() {
    // setup the display
    set_graphics(VDP_SPR_8x8);
    charsetlc();
    vdpmemset(gColor, 0xf4, 32);
    VDP_SET_REGISTER(VDP_REG_COL, 0x04);

    // draw the user text - windows app has prepared a
    // full 768 byte screen for us
    vdpmemcpy(gImage, textout, 768);

    // get the song pointers
    firstSong = *((unsigned int*)&textout[768]);
    secondSong = *((unsigned int*)&textout[768+2]);

#ifdef BUILD_TI99 
    if (0 != secondSong) {
        // this is a SID song, so read the SID registers too
        SidCtrl1 = textout[768+4];
        SidCtrl2 = textout[768+5];
        SidCtrl3 = textout[768+6];
    }
#endif

loopStart:
    // start the song(s)
    if (0 != firstSong) {
        StartSong((unsigned char*)firstSong, 0);
    }
    if (0 != secondSong) {
#ifdef BUILD_TI99
        StartSID((unsigned char*)secondSong, 0);
#endif
#ifdef BUILD_COLECO
        ay_StartSong((unsigned char*)secondSong, 0);
#endif
    }

    // and play it on the interrupt
    for (;;) {
        unsigned char done = 1;

		vdpwaitvint();	// waits for a console interrupts, allows quit/etc
        if (0 != firstSong) {
            if (isSNPlaying) {
                CALL_PLAYER_SN;
                done = 0;
            }
        }
        if (0 != secondSong) {
    #ifdef BUILD_TI99
            if (isSIDPlaying) {
                CALL_PLAYER_SID;
                done = 0;
            }
    #endif
    #ifdef BUILD_COLECO
            if (isAYPlaying) {
                CALL_PLAYER_AY;
                done = 0;
            }
    #endif
        }
    
        // loop if it is finished
        if ((done)&&(textout[768+4+3])) goto loopStart;
    }

    // never reached
    return 2;

}

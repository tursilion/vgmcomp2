// Running the GBA player takes a little more work, as you need to service the
// audio interrupt to feed the audio buffers with the SN emulation.

// TODO: Journey to Silius is not working, I should implement more of libti99
// so that the rest of the demo can be run.

#include "tursigb.h"
#include "GBASNPlay.h"
//#include <sound.h>
//#include <vdp.h>
//#include <kscan.h>

extern const unsigned char mysong[];
extern const unsigned char outpack1[];
extern void PlayerUnitTest();
volatile int myJiffies;

#if 0
// requires CPlayerCommonHandEdit.asm to be built with these counters
extern unsigned int cntInline,cntRle,cntRle16,cntRle24,cntRle32,cntBack;
#endif

//inline void faster_hexprint2(int x) {
//    faster_hexprint(x>>8);
//    faster_hexprint(x&0xff);
//}

// snsim sends SN byte commands to the emulated chip (optional)
extern void snsim(unsigned char x);
// snupdateaudio fills the audio buffers when needed by timer 1 interrupt (we have to call this)
extern void snupdateaudio();

volatile unsigned char KSCAN_KEY = 0xff;
void vdpwaitvint() {
    WAITSYNC(1);
//    vsync();
//    while (VCOUNT == 160) { }
}
void kscan(int x) {
    (void)x;

    if ((REG_KEYINPUT & BTN_A) == 0) {
        KSCAN_KEY = ' ';
    } else {
        KSCAN_KEY = 0xff;
    }
}

void InterruptProcess(void)
{
    register u32 tmp=REG_IF;

    // disable interrupts
    REG_IME = 0;

    if (tmp & INT_VBLANK) {
        myJiffies++;
        // TODO: we could call the SN player, but that's better handled inline
   	}

    /* acknowledge to BIOS, for IntrWait() */
    *(volatile unsigned short *)0x3007FF8 |= tmp;

    // clear *all* flagged ints except timer 2, which we check below (writing a 1 clears that bit)
    // When we clear the bits, those ints are allowed to trigger again   	
    REG_IF = (~INT_TIMER2);

    // Re-enable interrupts
    REG_IME = 1;

    // Timer 2 handles the SN emulation
    // This one can take a while - we'll allow other ints now
	if (tmp & INT_TIMER2) {
        unsigned short oldbg = *(volatile unsigned short*)BG_PALETTE;

        *(volatile unsigned short*)BG_PALETTE = 0x03e0;  // green - working on SN emulation
        snupdateaudio();
        *(volatile unsigned short*)BG_PALETTE = oldbg;

        // clear the interrupt only after we're done. If we were too slow,
        // we may have missed interrupts!
        REG_IF=INT_TIMER2;  // writing zeros is ignored
    }
}

void waitkey() {
    while (KSCAN_KEY == ' ') kscan(5);
    while (KSCAN_KEY != ' ') kscan(5);
}

// intrwrap.s contains this small handler
extern void intrwrap();
void gbainit() {
    // setup vblank
    REG_IME = 0;
    INT_VECTOR = intrwrap; // assembly wrapper for interrupt handler
    myJiffies = 0;
    REG_DISPSTAT = VBLANK_IRQ;
    REG_IE = INT_TIMER2 | INT_VBLANK;   // TIMER1 is used by the SN emulator
    
    // set up the GBA sound hardware
    gbasninit();

    // master interrupt enable
    REG_IME = 1;
}

int main() {
    // set screen
//    set_graphics(VDP_MODE1_SPRMAG);
//    VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);

    // turn on video so we can use scanline measurements
    REG_DISPCNT = 0;    // no screens are visible
    *(volatile unsigned short*)BG_PALETTE = 0x7c00;  // blue

#if 0
    // requires CPlayerCommonHandEdit.asm to be built with these counters
    cntInline=cntRle=cntRle16=cntRle24=cntRle32=cntBack=0;
#endif

#if 0
    // see what's broken
    PlayerUnitTest();
    for (;;) {
        kscan(5);
        if (KSCAN_KEY == 32) break;
    }
    vdpmemset(gImage, ' ', 768);
#endif

    // prepare the gameboy
    gbainit();
    
    // mute the sound chip
    snsim(0x9f);
    snsim(0xbf);
    snsim(0xdf);
    snsim(0xff);
    waitkey();
    
    // play a tone on channel 1
    snsim(0x80);
    snsim(0x10);
    snsim(0x90);
    waitkey();
    
    // switch to channel 2
    snsim(0x9f);
    snsim(0xa0);
    snsim(0x20);
    snsim(0xb0);
    waitkey();
    
    // switch to channel 3
    snsim(0xbf);
    snsim(0xc0);
    snsim(0x30);
    snsim(0xd0);
    waitkey();
    
    // switch to noise
    snsim(0xdf);
    snsim(0xe5);
    snsim(0xf0);
    waitkey();
    
    // mute
    snsim(0xff);
    while (KSCAN_KEY == ' ') {
        kscan(5);
    }

    // turn off unused console interrupt flags (just saves a little CPU time)
//    VDP_INT_CTRL = VDP_INT_CTRL_DISABLE_SPRITES | VDP_INT_CTRL_DISABLE_SOUND;

    // set up the sprites
//    for (int idx=0; idx<4; ++idx) {
//        sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
//    }
    // reterminate the sprite list - sprite leaves the VDP address in the right place
//    VDPWD = 0xd0;

    // start the song
    StartSong((unsigned char*)mysong, 0);

    // now play it - space to loop
    int song = 0;
    for (;;) {
        *(volatile unsigned short*)BG_PALETTE = 0x7c00;  // blue - wait vint
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it

        *(volatile unsigned short*)BG_PALETTE = 0x001f;  // red - working on player
        CALL_PLAYER_SN;
        
        *(volatile unsigned short*)BG_PALETTE = 0x03e0;  // black - check keyboard

        kscan(5);
        if (KSCAN_KEY == ' ') {
            StopSong();
            ++song;
            if (song >3) {
                song = 0;
                StartSong((unsigned char*)mysong, 0);
            } else {
                StartSong((unsigned char*)outpack1, song-1);
            }
            while (KSCAN_KEY == ' ') {
                kscan(5);
            }
        }

#if 0
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

        // do a simple little viz
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
#endif

        // check if still playing
        if (!isSNPlaying) {
//            VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTGREEN);
            kscan(5);
            if (KSCAN_KEY == ' ') {
                StartSong((unsigned char*)mysong, 0);
//                VDP_SET_REGISTER(VDP_REG_COL,COLOR_LTBLUE);
            }
        }

#if 0
        // requires CPlayerCommonHandEdit.asm to be built with these counters
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

    }

    return 0;
}

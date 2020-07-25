// Simple Sample for compressed VGM (SPF) playback
// By Tursi

#include <vdp.h>
#include <TISNPlay.h>
#include <TISfxPlay.h>
#include <kscan.h>
#include <sound.h> 

extern const unsigned char music[];

int main() {
	unsigned char oldkey = 0;					// prevent key repeat
	unsigned char isPlaying = 0;				// remember if we are playing a song so we can call mute on end

	int x = set_text_raw(); 					// good old fashioned text mode
	VDP_REG1_KSCAN_MIRROR = x;					// save the value from kscan
	VDP_SET_REGISTER(VDP_REG_COL,(COLOR_WHITE<<4) | COLOR_DKBLUE);	// set foreground white and background color blue

	// load characters
	charsetlc();

	// put up the text menu
	putstring("Music V2:\n\n");
	putstring(" A - Antarctic Adventure\n");
	putstring(" B - California Games Title\n");
	putstring(" C - Double Dragon\n");
	putstring(" D - Moonwalker\n");
	putstring(" E - Alf\n\n");
	putstring("Low Priority SFX:\n\n");
	putstring(" 1 - Flag\n");
	putstring(" 2 - Hole\n");
	putstring(" 3 - Jump\n\n");
	putstring("High Priority SFX:\n\n");
	putstring(" 4 - Flag\n");
	putstring(" 5 - Hole\n");
	putstring(" 6 - Jump\n\n");
	putstring("7-stop music  8-stop sfx  9-stop all\n");

	VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display

	// disable the music
	MUTE_SOUND();	// a good idea to also mute the sound registers

    // it's possible to use the interrupt routine, we'll run it manually
    // as recommended above. See TISN's sample for other options.

	// now we can main loop
	for (;;) {
        vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
        CALL_PLAYER_SFX;    // must be first
        CALL_PLAYER_SN;

		// check for end of song, and mute audio if so (only if we were
		// playing, this way it doesn't interfere with sound effects)
		// mostly I need this because some of the Sega tunes leave the
		// sound generators running at the end. I did this instead of
		// patching the songs to demonstrate how to deal with it.
		if (isPlaying) {
            if (!isSNPlaying) {
				isPlaying=0;
				MUTE_SOUND();
			}
		}

		// use the fast inline KSCAN (hard to get 60 hz with the console kscan)
		kscanfast(0);

		if (KSCAN_KEY != oldkey) {
			switch(KSCAN_KEY) {
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
				MUTE_SOUND();	// in case an old song was still playing (no need to stop it)
                StartSong((unsigned char*)music, KSCAN_KEY-'A');
				isPlaying = 1;
				break;

			case '1':
			case '2':
			case '3':
                // low priority
				StartSfx((unsigned char*)music, KSCAN_KEY-'1'+5, 1);
				break;

			case '4':
			case '5':
			case '6':
                // high priority
				StartSfx((unsigned char*)music, KSCAN_KEY-'4'+5, 127);
				break;

			case '7':
				StopSong();
				MUTE_SOUND();
				break;

			case '8':
				StopSfx();
				break;

			case '9':
                StopSong();     // if we stop song first, stopSfx won't waste time restoring sound channels
                StopSfx();
				MUTE_SOUND();
				break;

			// no need for default
			}
			oldkey=KSCAN_KEY;
		}

		// and just as a bit of visualization, and to demonstrate them,
		// display the sound channel feedback on the top row
		// AAAA AA BBBB BB CCCC CC DD DD EEEE
		VDP_SET_ADDRESS(gImage);
		faster_hexprint(songNote[0]&0xff);	// song channel 1 tone
		faster_hexprint(songNote[0]>>8);
		VDPWD=' ';
		faster_hexprint(songVol[0]);		// song channel 1 volume
		VDPWD=' ';

		faster_hexprint(songNote[1]&0xff);	// song channel 2 tone
		faster_hexprint(songNote[1]>>8);
		VDPWD=' ';
		faster_hexprint(songVol[1]);		// song channel 2 volume
		VDPWD=' ';

		faster_hexprint(songNote[2]&0xff);	// song channel 3 tone
		faster_hexprint(songNote[2]>>8);
		VDPWD=' ';
		faster_hexprint(songVol[2]);		// song channel 3 volume
		VDPWD=' ';

		faster_hexprint(songNote[3]>>8);	// song noise channel
		VDPWD=' ';
		faster_hexprint(songVol[3]);     	// song noise volume
		VDPWD=' ';

		faster_hexprint(songNote[3]&0xff);	// song playing status
		faster_hexprint(sfxActive>>8);	    // sfx playing status
	}
}

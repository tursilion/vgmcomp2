// Although this is for Windows, porting to Linux would mostly be just
// replacing the Sleep function with usleep, and replacing the DirectSound
// output with SDL or something.

// This player should work with both PSG and AY samples, depending on
// which switch it was built with. WARNING!! Although there are two
// projects, they differ only by the preprocessor options and they
// share files! If you change one, you change them both! :)

// Due to the AY code I pulled in, this player is forced to be
// licensed under GPL. This does NOT apply to the rest of the software
// suite, which is not linked with that code in any way, and remains
// in the much more free public domain.

// build headers
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <errno.h>

// we'd need a makefile to do the multiple includes needed for SFX and so on, but that's okay
// we are referencing the CPlayer c files directly as well. Defines are in the project settings.
// For this Windows port only, I'm forcing the C files to compile as C++
#include "..\CPlayer\CPlayer.h"

// if you want to run unit tests
extern void PlayerUnitTest();

// -----------------------------------------
// Sound emulation, not needed on a machine with a real sound chip
// -----------------------------------------
// we use some of Classic99's code for the AY as well as the SN
#include "sound.h"
extern LPDIRECTSOUNDBUFFER soundbuf;	// sound chip audio buffer
struct StreamData soundDat;             // sound emulation object only

#ifdef USE_SN_PSG
extern void writeSound(int c);          // so we can send the mute commands
#endif

#ifdef USE_AY_PSG
#include "ayemu_8912.h"
extern void writeSound(int reg, int c); // so we can send the mute commands
extern void ay_update(short *buf, double nAudioIn, int nSamples);
extern ayemu_ay_t psg;
extern ayemu_ay_reg_frame_t regs;
#endif

// -----------------------------------------

// storage to load the SBF into
unsigned char buf[64*1024]; // maximum legal size

// -----------------------------------------
// main code - all the interface is in here
// -----------------------------------------
int main(int argc, char *argv[]) {
//  PlayerUnitTest();

    if (argc<2) {
        printf("Example SBF player for Windows. Pass in the name of an SBF file\n");
#ifdef USE_SN_PSG
        printf("SN PSG version\n");
#endif
#ifdef USE_AY_PSG
        printf("AY PSG version\n");
#endif
        return 1;
    }

    // read the file in - assumes success
    FILE *fp = fopen(argv[1], "rb");
    if (NULL == fp) {
        printf("Failed to open '%s', code %d\n", argv[1], errno);
        return 1;
    }
    memset(buf, 0, sizeof(buf));
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

#ifdef USE_SN_PSG
    // set up audio (sound emulation)
    sound_init(22050);
    
    // mute channels (your program startup does this)
    writeSound(0x9f);   
    writeSound(0xbf);
    writeSound(0xdf);
    writeSound(0xff);
#endif
#ifdef USE_AY_PSG
    // set up audio (sound emulation)
    memset(&regs, 0xff, sizeof(regs));
    sound_init(22050);
    ayemu_init(&psg);

    ayemu_set_chip_type(&psg, AYEMU_AY, NULL);
    ayemu_set_stereo(&psg, AYEMU_MONO, NULL);
    ayemu_set_sound_format (&psg, 22050, 1, 16);
    ayemu_reset(&psg);

    // mute channels (your program startup does this)
    writeSound(8,0);    // volume to zero
    writeSound(9,0);
    writeSound(10,0);
    writeSound(7,0xf8); // mixer to no noise
#endif

    // prepare the song (pass the buffer, and the song index)
    StartSong(buf, 0);

    // fake a 60hz play until finished
    // Rather than a loop, on many systems you
    // can call SongLoop from your vertical blank interrupt
    while (songActive) {
        // process the song - call every frame
        SongLoop();

#ifdef USE_SN_PSG
        // run the sound chip
       	UpdateSoundBuf(soundbuf, sound_update, &soundDat);
#endif
#ifdef USE_AY_PSG
        // run the sound chip
       	UpdateSoundBuf(soundbuf, ay_update, &soundDat);
#endif

        // sleep for about 16 ms
        Sleep(16);
    }

    // all done!
    return 0;
}

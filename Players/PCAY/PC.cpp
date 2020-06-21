// Although this is for Windows, porting to Linux would mostly be just
// replacing the Sleep function with usleep, and replacing the DirectSound
// output with SDL or something.

// This player should work with PSG, AY and SID samples, depending on
// which switch it was built with. WARNING!! Although there are three
// projects, they differ only by the preprocessor options and they
// share files! If you change one, you change them all! :)

// Due to the AY and SID code I pulled in, this player is forced to be
// licensed under GPL. This does NOT apply to the rest of the software
// suite, which is not linked with that code in any way, and remains
// in the much more free public domain, as do the common CPlayer files,
// when distributed WITHOUT the AY and SID code.

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

#ifdef USE_SID_PSG
#include "..\PCSID\sid.h"
extern void writeSound(int reg, int c); // so we can send the mute commands
extern void sid_update(short *buf, double nAudioIn, int nSamples);
extern SIDChip psg;
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
#ifdef USE_SID_PSG
        printf("SID PSG version\n");
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
#ifdef USE_SID_PSG
    // set up audio (sound emulation)
    // it's the job of the player to initialize the SID for the
    // desired tune, since we are emulating an SN PSG we ONLY
    // send the frequency and volume (to sustain, with release 0).
    // This means you can do SOME fun things with the envelopes
    // and attack/decay (but NOT sustain/release), as well as
    // the filters, but you need to sort that out separate from
    // the player. As this is just an example, we are just going
    // to hard code two tone channels and one noise channel. However,
    // the song data is not restricted to that.
    sound_init(22050);
    psg.begin();

    psg.set_register(0,1);
    psg.set_register(1,0);  // frequency 1
    psg.set_register(2,0);
    psg.set_register(3,8);  // pwm 0x800 (50%)
    psg.set_register(4, 0x41);  // square wave, gate
    psg.set_register(5,0);  // attack/decay fastest
    psg.set_register(6,0);  // zero sustain, fastest release

    psg.set_register(7,1);
    psg.set_register(8,0);  // frequency 1
    psg.set_register(9,0);
    psg.set_register(10,8);  // pwm 0x800 (50%)
    psg.set_register(11,0x41);  // square wave, gate
    psg.set_register(12,0);  // attack/decay fastest
    psg.set_register(13,0);  // zero sustain, fastest release

    psg.set_register(14,1);
    psg.set_register(15,0);  // frequency 1
    psg.set_register(16,0);
    psg.set_register(17,8);  // pwm 0x800 (50%)
    psg.set_register(18, 0x81);  // noise wave, gate
    psg.set_register(19,0);  // attack/decay fastest
    psg.set_register(20,0);  // zero sustain, fastest release

    psg.set_register(21, 0);
    psg.set_register(22, 0);    // minimum filter cutoff (only 11 actual bits)
    psg.set_register(23, 0);    // don't filter
    psg.set_register(24, 0xf);  // no notch filters, maximum master volume
#endif

    // prepare the song (pass the buffer, and the song index)
    StartSong(buf, 0);

    // fake a 60hz play until finished
    // Rather than a loop, on many systems you
    // can call SongLoop from your vertical blank interrupt
    while (songNote[3]&SONGACTIVEACTIVE) {
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
#ifdef USE_SID_PSG
        // TODO: this is not working quite correctly... lots of
        // noise. Likely I've missed a part that needs porting. It
        // is recognizable, though...
        // For one thing, channels with the noise gate cleared are
        // still making noise... should they??

        // run the sound chip
       	UpdateSoundBuf(soundbuf, sid_update, &soundDat);

        int x=psg.get_register(6);
        printf("%02X ", x);
        x=psg.get_register(0x0d);
        printf("%02X ", x);
        x=psg.get_register(0x14);
        int out = psg.get_output();
        printf("%02X --> %02X\n", x, out);

#endif

        // sleep for about 16 ms
        Sleep(16);
    }

    // all done!
    return 0;
}

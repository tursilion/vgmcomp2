#ifndef INCLUDE_GBASNPLAY_H
#define INCLUDE_GBASNPLAY_H

// As long as BUILD_GBA is set, we can just install this header directly
#include <CPlayer.h>

#define CALL_PLAYER_SN \
    SongLoop();

// helpful wrapper
#define isSNPlaying ((songNote[3]&SONGACTIVEACTIVE) != 0)

// call this to set up the initial registers
void gbasninit();

// call this to start or stop the audio DMA. When starts, you MUST call snupdateaudio() every vertical blank
void gbastartaudio();
void gbastopaudio();

// call this every vertical blank to load the audio registers - if you miss it will cause noise
void snupdateaudio();

// snsim sends SN byte commands to the emulated chip (optional)
void snsim(unsigned char x);
// snupdateaudio fills the audio buffers when needed by timer 1 interrupt (we have to call this)
void snupdateaudio();


#endif  // file include

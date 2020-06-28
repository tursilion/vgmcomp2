#ifndef INCLUDE_CPLAYERSFX_H
#define INCLUDE_CPLAYERSFX_H

// most of the important defines need to be called from CPlayer.h
// which must be included before this one

// Call this function to prepare to play
// pSbf - pointer to song block data
// songNum - which song to play (starts at 0)
// pri - priority to play (lower number = higher priority)
void StartSfx(unsigned char *pSbf, uWordSize songNum, unsigned char pri);

// Call this to stop the current sfx
void StopSfx();

// this needs to be called 60 times per second by your system
void SfxLoop();

// MSB is the active bit with the mutes, LSB is current priority
extern unsigned int sfxActive;

#endif  // file include

// testPlayPSG.cpp : This program reads in the PSG data files and plays them
// so you can hear what it currently sounds like.

// We need to read in the channels, and there can be any number of channels.
// They have the following naming schemes:
// voice channels are <name>_tonXX.60hz
// noise channels are <name>_noiXX.60hz
// simple ASCII format, values stored as hex (but import should support decimal!), row number is implied frame

// TODO: the "60hz" can also be "50hz", "30hz" or "25hz". We want to support all of those here.
// We also want the program to be able to /find/ the channels given a prefix, or
// be given specific channel numbers.

#include "stdafx.h"
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "sound.h"

extern LPDIRECTSOUNDBUFFER soundbuf;		// sound chip audio buffer
struct StreamData soundDat;

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool isNoise[MAXCHANNELS];

bool clipbass = false;                      // whether to clip bass

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
};



// input: 8 bit unsigned audio (centers on 128)
// output: 15 (silent) to 0 (max)
int mapVolume(int nTmp) {
	int nBest = -1;
	int nDistance = INT_MAX;
	int idx;

	// testing says this is marginally better than just (nTmp>>4)
	for (idx=0; idx < 16; idx++) {
		if (abs(volumeTable[idx]-nTmp) < nDistance) {
			nBest = idx;
			nDistance = abs(volumeTable[idx]-nTmp);
		}
	}

    // don't return mute unless the input was mute
    if ((nBest == 15) && (nTmp != 0)) --nBest;

	// return the index of the best match
	return nBest;
}



int main(int argc, char *argv[])
{
    char namebuf[128];
    char buf[1024];
    char ext[16];
    int delay = 16;

	if (argc < 2) {
		printf("testPlayPSG [-clipbass] <file prefix>\n");
		printf(" -clipbass - restrict bass notes to the range of the TI PSG\n");
		printf(" <file prefix> - PSG file prefix (usually the name of the original VGM).\n");
        printf("Will search for 60hz, 50hz, 30hz, and 25hz in that order\n");
		return -1;
	}

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-clipbass")) {
			clipbass=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    strncpy_s(namebuf, argv[arg], sizeof(namebuf));
    namebuf[sizeof(namebuf)-1]='\0';

    printf("Working with prefix '%s'... ", namebuf);

    // work out which extension - channel 0 must exist
    for (int idx=0; idx<5; ++idx) {
        switch (idx) {
            case 0: strcpy(ext,"60hz"); delay=1000/60; break;
            case 1: strcpy(ext,"50hz"); delay=1000/50; break;
            case 2: strcpy(ext,"30hz"); delay=1000/30; break;
            case 3: strcpy(ext,"25hz"); delay=1000/25; break;
            case 4: printf("\nCan't find the song. Channel 00 must exist as ton or noi!\n"); return 1;
        }
        sprintf_s(buf, "%s_ton00.%s", namebuf, ext);
        FILE *fp = fopen(buf, "rb");
        if (NULL != fp) {fclose(fp); break;}

        sprintf_s(buf, "%s_noi00.%s", namebuf, ext);
        fp = fopen(buf, "rb");
        if (NULL != fp) {fclose(fp); break;}
    }

    printf("Found extension %s\n", ext);

    // dumbest loader ever...
    // first try all the tones
    int chan = 0;
    int cnt = 0;    // TODO: check that all files have the same cnt
    for (int idx=0; idx<99; idx++) {
        sprintf_s(buf, "%s_ton%02d.%s", namebuf, idx, ext);
        FILE *fp = fopen(buf, "rb");
        if (NULL != fp) {
            printf("Reading TONE channel %d... ", idx);
            cnt = 0;
            while (!feof(fp)) {
                char b2[256];
                if (fgets(b2, sizeof(b2), fp)) {
                    b2[sizeof(b2)-1]='\0';
                    if (2 != sscanf_s(b2, "0x%X,0x%X", &VGMDAT[chan][cnt], &VGMVOL[chan][cnt])) {
                        // try decimal
                        if (2 != sscanf_s(b2, "%d,%d", &VGMDAT[chan][cnt], &VGMVOL[chan][cnt])) {
                            printf("Error parsing line %d\n", cnt+1);
                            return 1;
                        }
                    }
                    VGMDAT[chan][cnt] &= NOISE_MASK;    // limit input
                    VGMVOL[chan][cnt] &= 0xff;
                    ++cnt;
                    if (cnt >= MAXTICKS) {
                        printf("(maximum reached) ");
                        break;
                    }
                }
            }
            fclose(fp);
            printf("read %d lines\n", cnt);
            isNoise[chan] = false;
            registerChan(chan, false);
            ++chan;
        }
    }
    // then all the noises
    for (int idx=0; idx<99; idx++) {
        sprintf_s(buf, "%s_noi%02d.%s", namebuf, idx, ext);
        FILE *fp = fopen(buf, "rb");
        if (NULL != fp) {
            printf("Reading NOISE channel %d... ", idx);
            cnt = 0;
            while (!feof(fp)) {
                char b2[256];
                if (fgets(b2, sizeof(b2), fp)) {
                    b2[sizeof(b2)-1]='\0';
                    if (2 != sscanf_s(b2, "0x%X,0x%X", &VGMDAT[chan][cnt], &VGMVOL[chan][cnt])) {
                        // try decimal
                        if (2 != sscanf_s(b2, "%d,%d", &VGMDAT[chan][cnt], &VGMVOL[chan][cnt])) {
                            printf("Error parsing line %d\n", cnt+1);
                            return 1;
                        }
                    }
                    VGMDAT[chan][cnt] &= NOISE_MASK|NOISE_TRIGGER|NOISE_PERIODIC;    // limit input
                    VGMVOL[chan][cnt] &= 0xff;
                    ++cnt;
                    if (cnt >= MAXTICKS) {
                        printf("(maximum reached) ");
                        break;
                    }
                }
            }
            fclose(fp);
            printf("read %d lines\n", cnt);
            isNoise[chan] = true;
            registerChan(chan, true);
            ++chan;
        }
    }

    // setup the sound system
    if (!sound_init(22050)) return 1;

    printf(".. and playing...\n");

    // now play out the song, at delay ms per frame
    // cnt better be the same on each channel ;)
    for (int row = 0 ; row < cnt; ++row) {
        // to heck with correct timing...
        // works oddly well on Win10...
        Sleep(delay);

        // process the command line from each channel
        for (int idx=0; idx<chan; ++idx) {
            setfreq(idx, VGMDAT[idx][row], clipbass);
            setvol(idx, mapVolume(VGMVOL[idx][row]));
        }

        if (NULL != soundbuf) {
        	UpdateSoundBuf(soundbuf, sound_update, &soundDat);
        }
    }

    printf("** DONE **\n");
    return 0;
}


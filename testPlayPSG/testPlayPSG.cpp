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
#include <cmath>
#include <string.h>
#include <errno.h>
#include "sound.h"

extern LPDIRECTSOUNDBUFFER soundbuf;		// sound chip audio buffer
struct StreamData soundDat;

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool isNoise[MAXCHANNELS];

bool clipbass = false;                      // whether to clip bass
bool shownotes = false;                     // whether to scroll the notes
char NoteTable[4096][4];                    // we'll just lookup the whole range

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

void buildNoteTable() {
    // note scales are confusing. ;) But anyway, we'll start at
    // A0 in the tempered scale which is 27.5 hz, or 4068 counts.
    // That's the lowest this protocol supports (and is far lower
    // than the TI can manage.)
    static const char *names[12] = { "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-" };
    int scale = 0;  // scale increments at C
    int namepos = 9;    // A
    double currentNote = 27.5;  // A

    // so first init the table blank
    for (int idx=0; idx<sizeof(NoteTable)/sizeof(NoteTable[0]); ++idx) {
        strcpy(NoteTable[idx],"---");
    }

    // okay! Now we build, increasing frequency until it is under 20
    int lastPer = 4095;
    for (;;) {
        int period = int(111860.8/currentNote+0.5);
        if (period < 20) break;     // no longer tuned around here
        int split = (lastPer-period) / 2;
        for (int run = period-split; run<period+split; ++run) {
            sprintf(NoteTable[run], "%s%d", names[namepos], scale);
        }
        ++namepos;
        if (namepos > 11) {
            namepos = 0;
            ++scale;
        }
        currentNote = currentNote * pow(2, 1.0/12.0);
        lastPer = period;
    }
    // close enough...
}

// MUST return a 3-character string
const char *getNoteStr(int note, int vol) {
    if (vol == 0) return "---";
    return NoteTable[note&0xfff];
}

// pass in file pointer (will be reset), channel index, last row count, first input column (from 0), and true if noise
// set scalevol to true to scale PSG volumes back to 8-bit linear
bool loadData(FILE *fp, int &chan, int &cnt, int column, bool noise, bool scalevol) {
    // up to 8 columns allowed
    int in[8];
    
    // remember last
    int oldcnt = cnt;

    // back to start of file
    fseek(fp, 0, SEEK_SET);

    cnt = 0;
    while (!feof(fp)) {
        char b2[256];
        if (fgets(b2, sizeof(b2), fp)) {
            int cols;
            b2[sizeof(b2)-1]='\0';
            cols = sscanf_s(b2, "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X", &in[0], &in[1], &in[2], &in[3], &in[4], &in[5], &in[6], &in[7]);
            if (cols == 0) {
                // try decimal
                cols = sscanf_s(b2, "%d,%d,%d,%d,%d,%d,%d,%d", &in[0], &in[1], &in[2], &in[3], &in[4], &in[5], &in[6], &in[7]);
            }
            if (cols < column+2) {
                printf("Error parsing line %d\n", cnt+1);
                return false;
            }
            VGMDAT[chan][cnt] = in[column];
            if (scalevol) {
                VGMVOL[chan][cnt] = volumeTable[in[column+1]&0x0f];
            } else {
                VGMVOL[chan][cnt] = in[column+1];
            }

            if (noise) {
                VGMDAT[chan][cnt] &= NOISE_MASK|NOISE_TRIGGER|NOISE_PERIODIC;    // limit input
                VGMVOL[chan][cnt] &= 0xff;
                if (scalevol) {
                    // the noise needs to be converted back to a shift rate too
                    int mask = VGMDAT[chan][cnt] & NOISE_TRIGGER;
                    int periodic = VGMDAT[chan][cnt] & 0x4;
                    int type = VGMDAT[chan][cnt] & 3;
                    switch (type) {
                        case 0: type = 0x10; break;
                        case 1: type = 0x20; break;
                        case 2: type = 0x40; break;
                        case 3: if (chan > 0) {type = VGMDAT[chan-1][cnt]&0x3ff;} break;  // works if you aren't being weird
                    }
                    VGMDAT[chan][cnt] = mask | (periodic?NOISE_PERIODIC:0) | type;
                }
            } else {
                VGMDAT[chan][cnt] &= NOISE_MASK;    // limit input
                VGMVOL[chan][cnt] &= 0xff;
            }
            ++cnt;
            if (cnt >= MAXTICKS) {
                printf("(maximum tick count reached) ");
                break;
            }
        }
    }
    printf("read %d lines\n", cnt);
    isNoise[chan] = noise;
    registerChan(chan, noise);
    ++chan;

    if ((oldcnt > 0) && (oldcnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", oldcnt, cnt);
    }

    return true;
}

int main(int argc, char *argv[])
{
    char namebuf[128];
    char buf[1024];
    char ext[16];
    int delay = 16;

	if (argc < 2) {
		printf("testPlayPSG [-clipbass] [-shownotes] [<file prefix> | <track1> <track2> ...]\n");
		printf(" -clipbass - restrict bass notes to the range of the TI PSG\n");
		printf(" -shownotes - display notes as the frames are played\n");
		printf(" <file prefix> - PSG file prefix (usually the name of the original VGM).\n");
        printf(" <track1> etc - instead of a prefix, you may explicitly list the files to play\n");
        printf("Prefix will search for 60hz, 50hz, 30hz, and 25hz in that order.\n");
        printf("Non-prefix can be a list of track files, or a single PSG file.\n");
		return -1;
	}

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-clipbass")) {
			clipbass=true;
        } else if (0 == strcmp(argv[arg], "-shownotes")) {
			shownotes=true;
            buildNoteTable();
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    int cnt = 0;    // total rows in song
    int chan = 0;   // total channels in song

    // check for explicit list. The problem is the prefix file usually exists, so
    // we HAVE to try to parse first.
    while (arg < argc) {
        strncpy_s(namebuf, argv[arg], sizeof(namebuf));
        namebuf[sizeof(namebuf)-1]='\0';

        FILE *fp=fopen(namebuf, "rb");
        if (NULL == fp) {
            printf("Failed to open '%s', code %d\n", namebuf, errno);
            return 1;
        }

        // it might be a 60Hz or a might be a PSG, they have different loaders
        // look at the first line - just check for more than one comma ;)
        char buf[256];
        memset(buf, 0, sizeof(buf));
        fgets(buf, sizeof(buf)-1, fp);

        // find first comma and last comma
        char *fcomma = strchr(buf, ',');
        char *lcomma = strrchr(buf, ',');

        if ((fcomma==NULL)||(lcomma==NULL)) {
            if (chan != 0) {
                // if we were processing...
                printf("Failed to parse file at first line of file '%s'.\n", namebuf);
                return 1;
            }
            // break and check for prefix
            break;
        }

        if (fcomma == lcomma) {
            // then it's a simple 60hz file with one channel
            // noise or tone by filename
            bool noise = (strstr(namebuf, "_noi") != NULL);
            printf("Reading %s channel %d from %s... ", noise?"NOISE":"TONE", chan, namebuf);
            if (!loadData(fp, chan, cnt, 0, noise, false)) return 1;
        } else {
            // it's a PSG file, we need to load all four channels
            // This is not efficient, but it's simple
            for (int idx=0; idx<4; ++idx) {
                printf("Reading %s channel %d from %s... ", (idx==3)?"NOISE":"TONE", chan, namebuf);
                if (!loadData(fp, chan, cnt, idx*2, (idx==3), true)) return 1;
            }
        }
        ++arg;
    }

    if (chan == 0) {
        // maybe using a prefix, lots of guessing involved

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
        for (int idx=0; idx<99; idx++) {
            sprintf_s(buf, "%s_ton%02d.%s", namebuf, idx, ext);
            FILE *fp = fopen(buf, "rb");
            if (NULL != fp) {
                printf("Reading TONE channel %d from %s... ", idx, buf);
                if (!loadData(fp, chan, cnt, 0, false, false)) return 1;
                fclose(fp);
            }
        }
        // then all the noises
        for (int idx=0; idx<99; idx++) {
            sprintf_s(buf, "%s_noi%02d.%s", namebuf, idx, ext);
            FILE *fp = fopen(buf, "rb");
            if (NULL != fp) {
                printf("Reading NOISE channel %d from %s... ", idx, buf);
                if (!loadData(fp, chan, cnt, 0, true, false)) return 1;
                fclose(fp);
            }
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

        if (shownotes) {
            printf("%04d | ", row);
        }

        // process the command line from each channel
        for (int idx=0; idx<chan; ++idx) {
            setfreq(idx, VGMDAT[idx][row], clipbass);
            setvol(idx, mapVolume(VGMVOL[idx][row]));

            if (shownotes) {
                printf("%s %02X | ", getNoteStr(VGMDAT[idx][row],VGMVOL[idx][row]), VGMVOL[idx][row]);
            }

            static bool warn = false;
            if ((!warn)&&(!clipbass)) {
                if ((VGMDAT[idx][row] > 0x3ff) && (VGMVOL[idx][row] > 0)) {
                    warn=true;
                    printf("\nWarning: out of range bass for PSG (%03X)\n", VGMDAT[idx][row]);
                }
            }
        }
        if (shownotes) {
            printf("\n");
        }

        if (NULL != soundbuf) {
        	UpdateSoundBuf(soundbuf, sound_update, &soundDat);
        }
    }

    printf("** DONE **\n");
    return 0;
}


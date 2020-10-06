// bass2noise.cpp : Defines the entry point for the console application.
// Moves bass of a lower pitch than the SN supports to periodic noise.
// This has no analog on the SID and the AY both has better bass and
// does not support periodic noise, so there are no flags for those chips.
//
// This one does the entire song because multiple channels need to be
// dealt with. A new noise channel is always written, to allow you to
// use other tools to decide how to merge it, if necessary.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool isNoise[MAXCHANNELS];
int oldIdx[MAXCHANNELS];                    // so we write back with the same index
bool verbose = true;
bool nomute = false;                        // when true, don't mute un-movable basslines

#define TONEMASK 0xfff                      // note portion only, we have to be careful with noise flags
#define NOISEFLAGS 0xF0000
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

#define RENAME ".bass2noise.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Bass to SN Noise conversion Tool - v20201006\n\n");

    if (argc < 2) {
        printf("bass2noise [-q] [-nomute] <song>\n");
        printf("Converts tones lower in frequency than supported on the SN chip\n");
        printf("into periodic noise. A /new/ noise channel is written to support this.\n");
        printf("-q - quiet verbose output\n");
        printf("-nomute - in case of multiple basslines, the loudest one will be moved.\n");
        printf("          Quieter ones are /muted/ unless you pass this line.\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and updated.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else {
            printf("Unrecognized option '%s'\n", argv[arg]);
            return -1;
        }
        ++arg;
    }

    if (arg >= argc) {
        printf("Not enough arguments for filename.\n");
        return -1;
    }

    // we're going to use a similar hacky loader that testplayer does
    char szFilename[1024];
    strncpy(szFilename, argv[arg], 1024);
    szFilename[1023] = '\0';

    // we expect the file to end with "_ton00.60hz", except the ton might be noi, the
    // 00 might be any number from 00 to 99, and the 60hz might be any other number of hz.
    char *p = strrchr(szFilename, '_'); // this should be the start of our area of interest
    if (NULL == p) {
        printf("Please enter the full name of a single channel, not the prefix. We'll find the rest!\n");
        return -1;
    }
    int cnt,hz;
    if (2 != sscanf(p, "_%*c%*c%*c%d.%dhz", &cnt, &hz)) {
        printf("Failed to parse filename, can not find _ton00.60hz (or equivalent)\n");
        return -1;
    }
    ++p;
    *p = '\0';  // filename is now filename_
    
    // safety check - malicious input. just cause I feel like it.
    // cause we're going to use it as a format string we can't let the
    // user provide any format specifiers for us.
    if (NULL != strchr(szFilename, '%')) {
        printf("Illegal filename.\n");
        return -1;
    }
    strcat(szFilename, "%s%02d.%dhz"); // filename now filename_%s%02d.%dhz

    // try to load the song
    int chancnt = 0;
    int row = 0;
    int highestIdx = -1;    // will contain the index for the new noise channel
    int clips = 0;          // should always be 0, unless a file was hand editted
    for (int idx=0; idx<200; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (idx < 100) {
            sprintf(testFile, szFilename, "ton", idx, hz);
            isNoise[chancnt] = false;
            oldIdx[chancnt] = idx;
        } else {
            // Technically, there's no reason to process noise channels.
            // But we need to ensure we don't stomp on them, so we'll
            // go ahead and pull them in...
            sprintf(testFile, szFilename, "noi", idx-100, hz);
            isNoise[chancnt] = true;
            oldIdx[chancnt] = idx-100;
        }

        FILE *fp = fopen(testFile, "r");
        if (NULL == fp) continue;       // in fact, most will fail
        if (idx%100 > highestIdx) highestIdx = idx%100;

        if (verbose) printf("Opened channel %d: %s\n", idx%100, testFile);

        // read until the channel ends
        int oldrow = row;
        row = 0;
        bool cont = true;
        while (cont) {
            char buf[128];

            if (feof(fp)) {
                cont = false;
                break;
            }

            if (NULL == fgets(buf, 128, fp)) {
                cont = false;
                break;
            }
            if (2 != sscanf(buf, "0x%X,0x%X", &VGMDAT[chancnt][row], &VGMVOL[chancnt][row])) {
                if (2 != sscanf(buf, "%d,%d", &VGMDAT[chancnt][row], &VGMVOL[chancnt][row])) {
                    printf("Failed to parse line %d for channel %d\n", row+1, idx%100);
                    cont = false;
                    break;
                }
            }
            if ((VGMDAT[chancnt][row]&TONEMASK) > 0xfff) {
                ++clips;
                VGMDAT[chancnt][row] = 0xfff | (VGMDAT[chancnt][row]&NOISEFLAGS);
            }
            if (cont) {
                ++row;
                if (row >= MAXTICKS) {
                    printf("Maximum song length reached, truncating.\n");
                    break;
                }
            }
        }

        fclose(fp);

        if ((oldrow != 0) && (row != oldrow)) {
            printf("Inconsistent row length (%d vs %d) found. Please correct.\n", oldrow, row);
            return -1;
        }

        ++chancnt;
        if (chancnt >= MAXCHANNELS-1) {     // -1 because we will need room to add one
            printf("Too many channels!\n");
            return -1;
        }
    }
    if (chancnt < 1) {
        printf("No channels found!\n");
        return -1;
    }
    if (++highestIdx >= 100) {
        // this is lazy, but it's pretty unlikely...
        printf("Highest channel index is too high, please make room at 99 or lower.\n");
        return -1;
    }
    if (clips > 0) {
        printf("Warning: %d notes clipped (>0xfff)\n", clips);
    }

    // okay, now the whole song is loaded. chancnt has
    // the number of loaded channels, and isNoise[] tracks if
    // they are noise or not for the save code. row has the
    // number of rows (verified to match), and oldIdx has the
    // file index. The new noise channel is going to be at chancnt-1

    // this loop renames all the old files
    for (int idx = 0; idx<chancnt; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (isNoise[idx]) {
            sprintf(testFile, szFilename, "noi", oldIdx[idx], hz);
        } else {
            sprintf(testFile, szFilename, "ton", oldIdx[idx], hz);
        }

        char buf[1100];
        sprintf(buf, "%s" RENAME, testFile);
        if (rename(testFile, buf)) {
            // this is kind of bad - we might be partially through converting the song?
            printf("Error renaming '%s' to '%s', code %d\n", testFile, buf, errno);
            return 1;
        }
    }

    // create the new noise channel, all muted of course
    for (int idx=0; idx<row; ++idx) {
        VGMDAT[chancnt][idx] = 1;   // highest freq
        VGMVOL[chancnt][idx] = 0;   // mute
    }
    isNoise[chancnt] = true;
    oldIdx[chancnt] = highestIdx;
    ++chancnt;

    // now run through the song, moving bass to the new noise channel
    // in case of conflict, take the loudest, then either mute the
    // other one or leave it, per command line.
    int moved = 0;
    int muted = 0;      // or left behind if nomute is set
    for (int idx=0; idx<row; ++idx) {
        // find the loudest bass on this row
        int loudestChan = -1;
        int loudestVol = 0;
        int loudestCnt = 0;
        for (int ch = 0; ch<chancnt-1; ++ch) {
            if ((VGMDAT[ch][idx] > 0x3ff) && (VGMVOL[ch][idx] > loudestVol)) {
                loudestChan = ch;
                loudestVol = VGMVOL[ch][idx];
                ++loudestCnt;
            }
        }
        if (loudestChan > -1) {
            // we have one, so first we go ahead and move it
            // we are assuming a TI SN with a 15 bit shift register,
            // so we divide the frequency by 15 before writing it
            // Since the input is limited to 0xfff, this can NEVER exceed 0x3ff.
            VGMDAT[chancnt-1][idx] = int(VGMDAT[loudestChan][idx]/15.0+0.5) | NOISE_PERIODIC;
            VGMVOL[chancnt-1][idx] = VGMVOL[loudestChan][idx];

            // check if it's a new trigger
            if ((idx == 0) || ((VGMDAT[chancnt-1][idx]&TONEMASK) != (VGMDAT[chancnt-1][idx-1]&TONEMASK))) {
                VGMDAT[chancnt-1][idx] |= NOISE_TRIGGER;
            }
            
            // now zero the one we moved
            VGMDAT[loudestChan][idx] = 1;
            VGMVOL[loudestChan][idx] = 0;

            // and count it
            ++moved;
        }
        // count how many other channels there were...
        muted += loudestCnt-1;
        if ((!nomute) && (loudestCnt > 1)) {
            // there was conflict, so mute any remaining basses
            for (int ch = 0; ch<chancnt-1; ++ch) {
                if (VGMDAT[ch][idx] > 0x3ff) {
                    VGMDAT[ch][idx] = 1;
                    VGMVOL[ch][idx] = 0;
                }
            }
        }
    }

    if (verbose) printf("\n");
    if ((verbose)&&(clips>0)) printf("%d notes clipped\n", clips);
    if ((verbose)&&(moved>0)) printf("%d/%d notes moved (non-lossy)\n", moved, chancnt*row);
    if ((verbose)&&(muted>0)) {
        if (nomute) {
            printf("WARNING:: %d bass notes remain - run again or process with another tool.\n", muted);
        } else {
            printf("%d/%d notes muted (lossy).\n", muted, chancnt*row);
        }
    }
    if (verbose) printf("\n");

    for (int idx=0; idx<chancnt; ++idx) {
        // now we just have to spit the data back out to new files
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (isNoise[idx]) {
            sprintf(testFile, szFilename, "noi", oldIdx[idx], hz);
        } else {
            sprintf(testFile, szFilename, "ton", oldIdx[idx], hz);
        }

        FILE *fout = fopen(testFile, "w");
        if (NULL == fout) {
            printf("Failed to open output file '%s', err %d\n", testFile, errno);
            return 1;
        }
        if (idx == oldIdx[chancnt-1]) {
            printf("Writing NEW: %s\n", testFile);
        } else {
            printf("Writing: %s\n", testFile);
        }
        for (int idx2=0; idx2<row; ++idx2) {
            fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[idx][idx2], VGMVOL[idx][idx2]);
        }
        fclose(fout);
    }
    
    printf("** DONE **\n");
    
    return 0;
}


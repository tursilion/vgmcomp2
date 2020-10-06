// findmuted.cpp : Defines the entry point for the console application.
// Analyzes an entire song, and reports if any channels are completely mute
// (so they can be deleted)

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

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0)) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 Mute Channel Finding Tool - v20201006\n\n");

    if (argc < 2) {
        printf("findmuted [-q] <song>\n");
        printf("Reports if there are any muted channels in a song that can be deleted.\n");
        printf("-q - quiet verbose output\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and checked.\n");
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

    // try to load the song - we check it as we load it
    int chancnt = 0;
    int row = 0;
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

        if (verbose) printf("%s: ", testFile);

        // read until the channel ends
        int oldrow = row;
        row = 0;
        bool cont = true;
        int dataLines = 0;
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
            if (cont) {
                if (!muted(chancnt, row)) ++dataLines;
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

        if (dataLines > 0) {
            printf("%d notes / %d total (%2d %%)\n", dataLines, row, dataLines*100/row);
        } else {
            printf("No data - can be deleted.\n");
        }

        ++chancnt;
        if (chancnt >= MAXCHANNELS) {
            printf("Too many channels!\n");
            return -1;
        }
    }
    if (chancnt < 1) {
        printf("No channels found!\n");
        return -1;
    }
   
    printf("** DONE **\n");
    
    return 0;
}


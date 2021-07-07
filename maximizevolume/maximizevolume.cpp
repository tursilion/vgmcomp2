// maximizevolume.cpp : Defines the entry point for the console application.
// Analyzes an entire song, and scales the volume of all channels so that the peak is as requested

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
int maxPercent = 255;       // cough... 8 bit percent ;)

#define RENAME ".maximizevolume.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Song Volume Maximize Tool - v20210707\n\n");

    if (argc < 2) {
        printf("maximizevolume [-q] [-max <n>] <song>\n");
        printf("Maximizes the volume of a song.\n");
        printf("-q - quiet verbose output\n");
        printf("-max <n> - sets the maximum volume to <n> percent (default 100)\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and checked.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else if (strcmp(argv[arg], "-max") == 0) {
            if (arg+1>=argc) {
                printf("Not enough arguments for max\n");
                return 1;
            }
            ++arg;
            maxPercent = atoi(argv[arg]);
            if ((maxPercent < 1) || (maxPercent > 255)) {
                printf("Invalid max percentage (1-255)\n");
                return 1;
            }
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
            // tones
            sprintf(testFile, szFilename, "ton", idx, hz);
            isNoise[chancnt] = false;
            oldIdx[chancnt] = idx;
        } else {
            // noise
            sprintf(testFile, szFilename, "noi", idx-100, hz);
            isNoise[chancnt] = true;
            oldIdx[chancnt] = idx-100;
        }

        FILE *fp = fopen(testFile, "r");
        if (NULL == fp) continue;       // in fact, most will fail

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
        if (chancnt >= MAXCHANNELS) {
            printf("Too many channels!\n");
            return -1;
        }
    }
    if (chancnt < 1) {
        printf("No channels found!\n");
        return -1;
    }

    printf("Imported %d rows...\n", row);

    // calculate the current maximum volume - check this before we rename everything
    int maxVol = 0;
    for (int idx=0; idx<chancnt; ++idx) {
        for (int i2=0; i2<row; ++i2) {
            if (VGMVOL[idx][i2] > maxVol) maxVol = VGMVOL[idx][i2];
        }
    }

    printf("Current maximum volume: %d\n", maxVol);
    printf("Desired maximum volume: %d\n", maxPercent);
    if (maxVol == maxPercent) {
        printf("Nothing to do!\n");
        return 0;
    }
    double scale = (double)maxPercent / (double)maxVol;
    printf("Scale percentage: %f\n", scale);

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
    
    // fix all the volumes
    int clipped = 0;
    for (int idx=0; idx<chancnt; ++idx) {
        for (int i2=0; i2<row; ++i2) {
            VGMVOL[idx][i2] = (int)(VGMVOL[idx][i2]*scale+0.5);
            if (VGMVOL[idx][i2] < 0) { VGMVOL[idx][i2] = 0; ++clipped; }
            if (VGMVOL[idx][i2] > 255) { VGMVOL[idx][i2] = 255; ++clipped; }
        }
    }

    if (clipped > 0) printf("Warning: %d notes clipped.\n", clipped);
    
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
        printf("Writing: %s\n", testFile);
        for (int idx2=0; idx2<row; ++idx2) {
            fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[idx][idx2], VGMVOL[idx][idx2]);
        }
        fclose(fout);
    }

    printf("** DONE **\n");

    return 0;
}


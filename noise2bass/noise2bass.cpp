// noise2bass.cpp : Defines the entry point for the console application.
// Moves periodic noise out of the noise channel and onto a new tone channel.
// This only has meaning when the input was an SN VGM file. Every noise channel
// will thus generate a new tone channel, unless it has no periodic noise.
//
// This one does the entire song because multiple channels need to be
// dealt with. 

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
bool mute = false;

#define TONEMASK 0xfff                      // note portion only, we have to be careful with noise flags
#define NOISEFLAGS 0xF0000
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

#define RENAME ".noise2bass.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 SN Noise to bass conversion Tool - v20260723\n\n");

    if (argc < 2) {
        printf("noise2bass [-q] [-mute] <song>\n");
        printf("Converts periodic noise back to tone channels.\n");
        printf("A /new/ tone channel is written to support this for each noise channel.\n");
        printf("(Unless there is no periodic noise found on that noise channel.\n");
        printf("This is intended for converting SN tunes to other chips.\n");
        printf("-q - quiet verbose output\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and updated.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else if (strcmp(argv[arg], "-mute") == 0) {
            mute = true;
            printf("Will mute out of range noise instead of clipping.\n");
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
    int highestIdx = -1;    // will contain the index for the new tone channel(s)
    int clips = 0;
    for (int idx=0; idx<200; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (idx < 100) {
            // Technically, there's no reason to process tone channels.
            // But we need to ensure we don't stomp on them, so we'll
            // go ahead and pull them in...
            sprintf(testFile, szFilename, "ton", idx, hz);
            isNoise[chancnt] = false;
            oldIdx[chancnt] = idx;
        } else {
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

    // for the rare case where we have multiple noise channels, we'll loop
    int lastNoiseChannel = -1;
    int firstNew = chancnt;
    for (int noise = 0; noise < chancnt; ++noise) {
        if (!isNoise[noise]) continue;

        if (verbose) printf("Processing noise on channel %d to tone channel %d\n", noise, chancnt);

        // create the new tone channel, all muted of course
        for (int idx=0; idx<row; ++idx) {
            VGMDAT[chancnt][idx] = 1;   // highest freq
            VGMVOL[chancnt][idx] = 0;   // mute
        }
        isNoise[chancnt] = false;
        oldIdx[chancnt] = highestIdx;
        ++chancnt;

        // now run through the song, moving periodic noise to the new bass channel
        // No chance of conflict since it's 1:1 (the user can later resolve conflicts
        // merging it to tone channel 2, which usually will have room).
        int moved = 0;
        clips = 0;
        for (int idx=0; idx<row; ++idx) {
            // we know exactly which channel we are dealing with 'noise'
            if (VGMDAT[noise][idx] & NOISE_PERIODIC) {
                // we have one, so first we go ahead and move it
                // we are assuming a TI SN with a 15 bit shift register,
                // so we multiply the frequency by 15 before writing it.
                // This /CAN/ exceed our maximum of 0xfff, so we will clip by default with
                // the option to mute instead at the command line (cause at that point
                // it's not really a tone anymore...)
                int val = (VGMDAT[noise][idx]&(~NOISEFLAGS)) * 15;
                if (val > 0xfff) {
                    ++clips;
                    if (!mute) {
                        // then move it anyway, but clipped
                        val = 0xfff;
                    } else {
                        val = 1;    // highest frequency
                    }
                }
                VGMDAT[chancnt-1][idx] = val;
                if (val <= 1) {
                    VGMVOL[chancnt-1][idx] = 0;
                } else {
                    VGMVOL[chancnt-1][idx] = VGMVOL[noise][idx];
                }

                // now zero the one we moved
                VGMDAT[noise][idx] = 1;
                VGMVOL[noise][idx] = 0;

                // and count it
                ++moved;
            }
        }

        if (verbose) printf("\n");
        if ((verbose)&&(clips>0)) printf("%d notes clipped\n", clips);
        if ((verbose)&&(moved>0)) printf("%d/%d notes moved (non-lossy)\n", moved, chancnt*row);
        if ((verbose)&&(clips>0)) {
            printf("%d/%d notes clipped or muted (lossy).\n", clips, chancnt*row);
        }
        if (verbose) printf("\n");
    }

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
        if (idx >= firstNew) {
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


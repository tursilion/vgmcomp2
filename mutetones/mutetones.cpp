// mutetones.cpp : Defines the entry point for the console application.
// Mutes any out-of-range frequencies to the min or max for a particular chip.

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 1
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];
bool isSN = false;
bool isAY = false;
bool isSID = false;
int minNote = 0;
int maxNote = 0;

#define RENAME ".mutetone.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Tone Muting Tool - v20200924\n\n");

    if (argc < 2) {
        printf("mutetones (-SN|-AY|-SID) <channel input>\n");
        printf("Based on the specified output chip, mutes any out-of-range\n");
        printf("tones to the minimum or maximum permitted.\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        if (0 == strcmp(argv[arg], "-SN")) {
            if ((isAY||isSID)) {
                printf("Specify only one chip type\n");
                return 1;
            }
            isSN = true;
            minNote = 1;
            maxNote = 0x3ff;
        } else if (0 == strcmp(argv[arg], "-AY")) {
            if ((isSN||isSID)) {
                printf("Specify only one chip type\n");
                return 1;
            }
            isAY = true;
            minNote = 1;
            maxNote = 0xfff;    // no real restrictions here
        } else if (0 == strcmp(argv[arg], "-SID")) {
            if ((isSN||isAY)) {
                printf("Specify only one chip type\n");
                return 1;
            }
            isSID = true;
            // these ones are a bit trickier, since the clock is so different.
            // Our target is the TI SID Blaster, which doesn't use the same clock
            // as the C64, either. The max SID register is 0xFFFF after convert.
            // the TI SID blaster is using an exactly 1MHZ clock, so it's not
            // the same as the real C64 at .985248 (PAL) or 1.022727 (NTSC).
            // Since we have the math, we might as well use it. ;)
            // SN HZ = 111860.8/code
            // CONSTANT = 256^3 / CLOCK
            // SID code = hz * CONSTANT
            // newcode = (111860.8/code) * (16777216 / CLOCK)
            minNote = 29;   // works out to about 0xFCCA, 28 is 0x105D1
            maxNote = 0xfff;// is only 0x1CA, but it's this toolchain max
        }
        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
    }
    if ((!isSN)&&(!isAY)&&(!isSID)) {
        printf("Must specify -SN, -AY or -SID for target range.\n");
        return 1;
    }

    if (arg>=argc) {
        printf("Not enough arguments for filename.\n");
        return 1;
    }

    // open input file(s)
    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        if (arg >= argc) {
            printf("Out of filename arguments.\n");
            return 1;
        }
        szFilename[idx] = argv[arg];
        fp[idx] = fopen(argv[arg], "r");
        if (NULL == fp[idx]) {
            printf("Failed to open file '%s' for channel %d, code %d\n", argv[arg], idx, errno);
            return 1;
        }
        printf("Opened channel %d: %s\n", idx, argv[arg]);
        ++arg;
    }

    // read until one of the channels ends
    bool cont = true;
    int row = 0;
    while (cont) {
        for (int idx=0; idx<MAXCHANNELS; ++idx) {
            char buf[128];

            if (feof(fp[idx])) {
                cont = false;
                break;
            }

            if (NULL == fgets(buf, 128, fp[idx])) {
                cont = false;
                break;
            }
            if (2 != sscanf(buf, "0x%X,0x%X", &VGMDAT[idx][row], &VGMVOL[idx][row])) {
                if (2 != sscanf(buf, "%d,%d", &VGMDAT[idx][row], &VGMVOL[idx][row])) {
                    printf("Failed to parse line %d for channel %d\n", row+1, idx);
                    cont = false;
                    break;
                }
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

    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        if (NULL != fp[idx]) {
            fclose(fp[idx]);
            fp[idx] = NULL;
            // since we were successful, also rename the source files
            char buf[1024];
            sprintf(buf, "%s" RENAME, szFilename[idx]);
            if (rename(szFilename[idx], buf)) {
                printf("Error renaming '%s' to '%s', code %d\n", szFilename[idx], buf, errno);
                return 1;
            }
        }
    }

    printf("Imported %d rows\n", row);

    // now just clip on minNote and maxNote
    int muted = 0;
    for (int idx = 0; idx<row; ++idx) {
        if (VGMDAT[0][idx] < minNote) {
            VGMDAT[0][idx] = minNote;
            VGMVOL[0][idx] = 0;
            ++muted;
        } else if (VGMDAT[0][idx] > maxNote) {
            VGMDAT[0][idx] = minNote;   // always minNote to reduce unnecessary notes in the output
            VGMVOL[0][idx] = 0;
            ++muted;
        }
    }
    printf("%d notes muted (lossy)\n", muted);

    // now we just have to spit the data back out to a new file
    FILE *fout = fopen(szFilename[0], "w");
    if (NULL == fout) {
        printf("Failed to open output file '%s', err %d\n", szFilename[0], errno);
        return 1;
    }
    printf("Writing: %s\n", szFilename[0]);

    for (int idx=0; idx<row; ++idx) {
        fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[0][idx], VGMVOL[0][idx]);
    }
    fclose(fout);

    printf("** DONE **\n");
    
    return 0;
}


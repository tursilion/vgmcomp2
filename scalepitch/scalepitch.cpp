// scalepitch.cpp : Defines the entry point for the console application.
// scales all notes in a channel by a user-specified amount. Octaves
// are usually double or half. This can bring out of range notes into
// range for a chip without losing musical integrity

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

#define RENAME ".pitch.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Pitch Scaling Tool - v20200726\n\n");

    if (argc < 2) {
        printf("scalepitch <scale> <channel input>\n");
        printf("Scales the frequency of the input channel. Whole octaves are\n");
        printf("usually double (2.0, which on most chips is lower), or half\n");
        printf("(0.5, which on most chips is higher).\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
    }

    if (arg>=argc) {
        printf("Not enough arguments for scale.\n");
        return 1;
    }

    // get scale factor
    float scale = 0.0;
    if (1 != sscanf(argv[arg], "%f", &scale)) {
        printf("Failed to parse scale factor.\n");
        return 1;
    }
    if (scale == 0) {
        printf("Scale of 0 is illegal\n");
        return 1;
    }
    ++arg;

    if (arg>=argc) {
        printf("Not enough arguments for filename.\n");
        return 1;
    }

    // open input file(s)
    for (int idx=0; idx<MAXCHANNELS; ++idx) {
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
            if (row >= MAXTICKS-4) {
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

    printf("Imported %d rows, scaling by %f\n", row, scale);

    // we just apply the scale, however, we have a hard maximum count of 4095
    // through much of the toolchain, so we'll still clip on that if we have to
    // also warn on any counts that hit 0
    int clipped = 0;
    int zeroed = 0;

    for (int idx = 0; idx<row; ++idx) {
        VGMDAT[0][idx] = int(VGMDAT[0][idx] * scale + 0.5);
        if (VGMDAT[0][idx] == 0) {
            ++zeroed;
            // disallow 0
            VGMDAT[0][idx] = 1;
        } else if (VGMDAT[0][idx] > 0xfff) {
            ++clipped;
            VGMDAT[0][idx] = 0x3ff;
        }
    }

    printf("%d notes clipped (lossy)\n", clipped);
    printf("%d notes zeroed (lossy)\n", zeroed);

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


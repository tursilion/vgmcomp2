// smoothvolume.cpp : Defines the entry point for the console application.
// Performs a smoothing function on the volume channel of a file
// useful for noisy converters like mod2psg

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

enum {
    MODE_AVERAGE,
    MODE_HERMITE,
    MODE_CUBIC
};
int mode = MODE_AVERAGE;

#define RENAME ".smooth.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Volume Smoothing Tool - v20200919\n\n");

    if (argc < 2) {
        printf("smoothvolume [-hermite|-cubic] <channel input>\n");
        printf("Performs a smoothing function on the volume component.\n");
        printf("Original file is renamed to " RENAME "\n");
        printf("-hermite - Use a Hermite interpolation instead of average\n");
        printf("-cubic - use a cubic interpolation instead of average\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        if (0 == strcmp(argv[arg], "-hermite")) {
            mode = MODE_HERMITE;
            printf("Using Hermite interpolation\n");
        } else if (0 == strcmp(argv[arg], "-cubic")) {
            mode = MODE_CUBIC;
            printf("Using Cubic interpolation\n");
        }
        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
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

    printf("Imported %d rows\n", row);

    // just process the volume through the smoothing function

    // note this skips the first 2 and last 2 samples, so make sure
    // it starts and ends at zero. Muting rather than changing length
    // because accidentally changing only one channel is bad
    if ((VGMVOL[0][0] != 0)||(VGMVOL[0][1] != 0)) {
        printf("Warning: Channel does not start at 0 volume, muting start\n");
        VGMVOL[0][0]=0;
        VGMVOL[0][1]=0;
    }
    if ((VGMVOL[0][row-1]!=0)||(VGMVOL[0][row-2]!=0)) {
        printf("Warning: Channel does not end at 0 volume, muting end\n");
        VGMVOL[0][row-1]=0;
        VGMVOL[0][row-2]=0;
    }

    int changed = 0;
    int clipped = 0;

    for (int idx = 2; idx<row-2; ++idx) {
        int oldVol = VGMVOL[0][idx];

        switch (mode) {
        case MODE_HERMITE:
            {
                // apparently, a Hermite interpolation curve idx == x2
                double c0 = VGMVOL[0][idx-1];
                double c1 = 0.5 * (VGMVOL[0][idx] - VGMVOL[0][idx-2]);
                double c2 = VGMVOL[0][idx-2] - (2.5 * VGMVOL[0][idx-1]) + (2 * VGMVOL[0][idx]) - (0.5 * VGMVOL[0][idx+1]);
                double c3 = (0.5 * (VGMVOL[0][idx+1] - VGMVOL[0][idx-2])) + (1.5 * (VGMVOL[0][idx-1] - VGMVOL[0][idx]));
                // take from 0.5 between x1 and x2
                VGMVOL[0][idx] = int((((((c3*.5)+c2)*.5)+c1)*.5)+c0 + 0.5);
            }
            break;

        case MODE_CUBIC:
            {
                // cubic interpolation idx == x2
                double a0 = VGMVOL[0][idx+1] - VGMVOL[0][idx] - VGMVOL[0][idx-2] + VGMVOL[0][idx];
                double a1 = VGMVOL[0][idx-2] - VGMVOL[0][idx-1] - a0;
                double a2 = VGMVOL[0][idx] - VGMVOL[0][idx-2];
                double a3 = VGMVOL[0][idx-1];
                double t = 0.5;
                VGMVOL[0][idx] = int(0.5 + ((a0 * (t*t*t)) + (a1 * (t*t)) + (a2*t) + (a3)));
            } 
            break;

        case MODE_AVERAGE:
        default:
            {
                // average of three points
                double avg = (VGMVOL[0][idx-1] + VGMVOL[0][idx] + VGMVOL[0][idx+1]) / 3.0;
                VGMVOL[0][idx] = int(avg+0.5);
            }
            break;
        }

        if (VGMVOL[0][idx] > 255) {
            ++clipped;
            VGMVOL[0][idx] = 255;
        }
        if (VGMVOL[0][idx] < 0) {
            ++clipped;
            VGMVOL[0][idx] = 0;
        }
        if (VGMVOL[0][idx] != oldVol) ++changed;
    }

    printf("%d volumes updated\n", changed);
    printf("%d volumes clipped\n", clipped);

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


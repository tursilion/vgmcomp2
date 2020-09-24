// scalevolume.cpp : Defines the entry point for the console application.
// scales all volume in a channel by a user-specified amount. Clips at top or bottom.

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

#define RENAME ".volume.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Volume Scaling Tool - v20200924\n\n");

    if (argc < 2) {
        printf("scalevolume (-maximize|<scale>) <channel input>\n");
        printf("Scales the volume of the input channel. Volumes are\n");
        printf("stored linearly from 0 (silent) to 255 (maximum)\n");
        printf("Be careful with maximize, if all channels are not scaled\n");
        printf("the same you may end up sounding worse!\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    bool maximize = false;
    while (argv[arg][0] == '-') {
        if (0 == strcmp(argv[arg], "-maximize")) {
            maximize = true;
        }
        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
    }

    float scale = 0.0;
    if (!maximize) {
        if (arg>=argc) {
            printf("Not enough arguments for scale.\n");
            return 1;
        }

        // get scale factor
        if (1 != sscanf(argv[arg], "%f", &scale)) {
            printf("Failed to parse scale factor.\n");
            return 1;
        }
        if (scale == 0) {
            printf("Scale of 0 is illegal\n");
            return 1;
        }
        ++arg;
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
    int maxVolRead = 0;
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
            if (VGMVOL[idx][row] > maxVolRead) {
                maxVolRead = VGMVOL[idx][row];
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

    // if we are maximizing, then use the maximum read volume to calculate scale
    if (maximize) {
        printf("Maximum volume read %d/255\n", maxVolRead);
        scale = (float)(255.0 / maxVolRead);
    }

    printf("Imported %d rows, scaling volume by %f\n", row, scale);

    // we just apply the scale, however, clip on maximum of 255.
    // also warn on any counts that hit 0
    int clipped = 0;
    int zeroed = 0;

    for (int idx = 0; idx<row; ++idx) {
        bool wasZero = (VGMVOL[0][idx] == 0);   // zero is legal, after all!
        VGMVOL[0][idx] = int(VGMVOL[0][idx] * scale + 0.5);
        if ((!wasZero) && (VGMVOL[0][idx] == 0)) {
            ++zeroed;
        } else if (VGMVOL[0][idx] > 255) {
            ++clipped;
            VGMVOL[0][idx] = 255;
        }
    }

    printf("%d volumes clipped (lossy)\n", clipped);
    printf("%d volumes zeroed (lossy)\n", zeroed);

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


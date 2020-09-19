// reducevolumecnt.cpp : Defines the entry point for the console application.
// reduces the resolution of the volume on the specified channel from 256
// levels to the level specified by the user.
// I'm not completely certain how helpful this is, given that the
// linear volume may not convert nicely to logarithmic once reduced...?
// I suppose time will tell!

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 1
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];

#define RENAME ".reducevol.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Volume Resolution Reduction Tool - v20200919\n\n");

    if (argc < 2) {
        printf("reducevolumecnt <channel input> <new cnt>\n");
        printf("Reduces the number of distinct volume levels from 256 to 'new cnt'\n");
        printf("Note that values greater than or equal to 16 are unlikely to affect the final file.\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        // no args

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

    // do the close first, we still have one more argument to check
    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        if (NULL != fp[idx]) {
            fclose(fp[idx]);
            // don't clear it yet - we need it below
        }
    }

    // check this before reporting on the load or renaming anything
    if (arg >= argc) {
        printf("Missing count argument.\n");
        return 1;
    }
    // check it's meaningful
    int newSize = atoi(argv[arg]);
    if ((newSize < 2) || (newSize > 255)) {
        // honestly, 255 is meaningless too, but whatever
        printf("New size of %d is meaningless, must be 2-255\n", newSize);
        return 1;
    }
    if (newSize >= 16) {
        printf("Warning: New size greater than 16 will probably not impact final output.\n");
    }

    // do the renames
    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        if (NULL != fp[idx]) {
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

    // calculate a divider for the volume - we'll use floats
    // I guess we might end up with off-by-ones in some cases,
    // but it'll work well enough most of the time.
    double volDivider = 255.0 / newSize;
    // to avoid the integer rounding just making everything really quiet,
    // also calculate a new multiplier
    double volMultiplier = 255.0 / int(255.0 / volDivider);
    int clipped = 0;
    int newVols = 0;

    for (int idx = 0; idx<row; ++idx) {
        int newVol = int(int(VGMVOL[0][idx] / volDivider) * volMultiplier + 0.5);
        VGMVOL[0][idx] = newVol;
        if (VGMVOL[0][idx] > 255) {
            ++clipped;
            VGMVOL[0][idx] = 255;
        }
        if (VGMVOL[0][idx] < 0) {
            ++clipped;
            VGMVOL[0][idx] = 0;
        }
        int t=0;
        while ((t<idx)&&(VGMVOL[0][t]!=VGMVOL[0][idx])) ++t;
        if (t>=idx) ++newVols;
    }

    printf("%d volume levels in output\n", newVols);
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


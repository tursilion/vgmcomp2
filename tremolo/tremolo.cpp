// tremolo.cpp : Defines the entry point for the console application.
// applies a sine wave to the volume with configurable ramp in on new note

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 1
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];

#define RENAME ".tremolo.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Volume Tremolo Tool - v20260412\n\n");

    if (argc < 2) {
        printf("tremolo <ramp in time in frames> <sine amplitude in percent (0-1)> <speed (float)> <channel input>\n");
        printf("Applies a tremolo to the existing volume. The strength of the tremolo\n");
        printf("ramps in from 0 to 'sine amplitude' percent over 'ramp in time' frames.\n");
        printf("The existing volume is the CENTER volume of the sine wave, so you may need to adjust before using.\n");
        printf("The ramp resets every time the frequency or volume changes. Use 0 for no ramp.\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    int rampintime = 0;
    if (arg >= argc) {
        printf("Not enough arguments for ramp\n");
        return 1;
    }
    rampintime = atoi(argv[arg]);
    ++arg;

    float amplitude = 0.0;
    if (arg>=argc) {
        printf("Not enough arguments for amplitude.\n");
        return 1;
    }
    if (1 != sscanf(argv[arg], "%f", &amplitude)) {
        printf("Failed to parse amplitude factor.\n");
        return 1;
    }
    if (amplitude == 0) {
        printf("Amplitude of 0 is illegal\n");
        return 1;
    }
    ++arg;

    float speed = 0.0;
    if (arg>=argc) {
        printf("Not enough arguments for speed.\n");
        return 1;
    }
    if (1 != sscanf(argv[arg], "%f", &speed)) {
        printf("Failed to parse speed factor.\n");
        return 1;
    }
    if (amplitude == 0) {
        printf("Speed of 0 is illegal\n");
        return 1;
    }
    ++arg;

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

    printf("Imported %d rows, tremolo strength %f, ramp %d frames, speed %f\n", row, amplitude, rampintime, speed);

    int oldtone = -1;
    int oldvol = -1;
    int framecnt = 0;
    int clipped = 0;
    int zeroed = 0;
    for (int idx = 0; idx<row; ++idx) {
        // first check if we need to reset the ramp
        if ((VGMDAT[0][idx] != oldtone) || (VGMVOL[0][idx] != oldvol)) {
            oldtone = VGMDAT[0][idx];
            oldvol = VGMVOL[0][idx];
            framecnt = 0;
        }

        // calculate the tremolo for this level
        double scale = amplitude;
        if (rampintime > 0) {
            scale *= (framecnt / rampintime);
        }
        scale *= sin(idx*speed);
        VGMVOL[0][idx] += (int)(VGMVOL[0][idx]*scale+0.5);
        if (VGMVOL[0][idx] > 255) {
            ++clipped;
            VGMVOL[0][idx] = 255;
        }
        if (VGMVOL[0][idx] < 0) {
            ++zeroed;
            VGMVOL[0][idx]=0;
        }

        if (framecnt < rampintime) {
            ++framecnt;
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


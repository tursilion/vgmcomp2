// fixsnnoise.cpp : Defines the entry point for the console application.
// Remaps noise frequencies to the SN fixed rates

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

#define NOISE_FLAGS    0x0F000
#define NOISE_MASK     0x00FFF

#define RENAME ".fixsnnoise.old"

// return absolute value
inline int ABS(int x) {
    if (x<0) return -x;
    else return x;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 SN Noise Fix Tool - v20201006\n\n");

    if (argc < 2) {
        printf("fixsnnoise <noise channel input>\n");
        printf("Remaps noise frequencies to fixed SN rates.\n");
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

        // verify it's noise
        if (NULL == strstr(szFilename[idx], "_noi")) {
            printf(" - Must be a NOISE channel.\n");
            return -1;
        }

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

    // now walk through it, and remap all the valid frequencies
    int mapped = 0;
    for (int idx = 0; idx<row; ++idx) {
        if (VGMVOL[0][idx] > 0) {
            // save the noise flags
            int noiseflags = VGMDAT[0][idx]&NOISE_FLAGS;
            VGMDAT[0][idx] &= NOISE_MASK;
            // map to the closest shift
            int diff16 = ABS(16 - (VGMDAT[0][idx]&NOISE_MASK));
            int diff32 = ABS(32 - (VGMDAT[0][idx]&NOISE_MASK));
            int diff64 = ABS(64 - (VGMDAT[0][idx]&NOISE_MASK));
            if ((diff16 <= diff32) && (diff16 <= diff64)) {
                VGMDAT[0][idx] = 16;    // use 16
                if (diff16 != 0) ++mapped;
            } else if ((diff32 <= diff16) && (diff32 <= diff64)) {
                VGMDAT[0][idx] = 32;    // use 32
                if (diff32 != 0) ++mapped;
            } else {
                VGMDAT[0][idx] = 64;    // use 64
                if (diff64 != 0) ++mapped;
            }
            VGMDAT[0][idx] |= noiseflags;
        }
    }
    printf("%d noises remapped (lossy)\n", mapped);

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


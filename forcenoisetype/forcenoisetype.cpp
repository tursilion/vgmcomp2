// forcenoisetype.cpp : Defines the entry point for the console application.
// Changes all noises in a channel to either white noise or periodic noise

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
bool isPeriodic = false;
bool isWhite = false;

#define NOISE_PERIODIC 0x20000
#define NOISE_MASK     0x00FFF

#define RENAME ".forcenoisetype.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 SN Noise Type Force Tool - v20201008\n\n");

    if (argc < 2) {
        printf("forcenoisetype <-white|-periodic> <noise channel input>\n");
        printf("Forces all noise in a channel to either white noise or periodic noise.\n");
        printf("Original file is renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while (argv[arg][0] == '-') {
        if (0 == strcmp(argv[arg], "-white")) {
            isWhite = true;
            if (isPeriodic) {
                printf("Can't specify both white and periodic noise!\n");
                return 1;
            }
        } else if (0 == strcmp(argv[arg], "-periodic")) {
            isPeriodic = true;
            if (isWhite) {
                printf("Can't specify both white and periodic noise!!\n");
                return 1;
            }
        }
        ++arg;
        if (arg >= argc) {
            printf("out of arguments\n");
            return 1;
        }
    }
    if ((!isWhite)&&(!isPeriodic)) {
        printf("Must specify either -white or -periodic for noise type!\n");
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

    // now walk through it, and set the noise flag as appropriate
    int changed = 0;
    for (int idx = 0; idx<row; ++idx) {
        int old = VGMDAT[0][idx];
        if (isWhite) {
            VGMDAT[0][idx] &= ~NOISE_PERIODIC;
        } else if (isPeriodic) {
            VGMDAT[0][idx] |= NOISE_PERIODIC;
        }
        if (VGMDAT[0][idx] != old) {
            ++changed;
        }
    }
    printf("%d noises updated (lossy)\n", changed);

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


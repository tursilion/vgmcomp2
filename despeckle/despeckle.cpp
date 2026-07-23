// despeckle.cpp : Defines the entry point for the console application.
// Removes any single-tick notes from a channel and paints over any single-tick mutes

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

#define RENAME ".despeckle.old"

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0)) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 Despeckle Tool - v20260723\n\n");

    if (argc < 2) {
        printf("despeckle <channel input>\n");
        printf("Mutes any single-tick sound in the passed-in channel.\n");
        printf("Single tick mutes with the same data on either side are also filled in.\n");
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

    // now walk through it, and if the previous and next volumes are muted, mute this one
    int cleared = 0;
    int filled = 0;
    for (int idx = 0; idx<row; ++idx) {
        int prevMute = false, nextMute = false, thisMute = false;

        if ((idx == 0) || (muted(0, idx-1))) prevMute = true;
        if ((idx == row-1) || (muted(0, idx+1))) nextMute = true;
        if (muted(0, idx)) thisMute = true;

        // check for single tick note
        if ((prevMute)&&(nextMute)&&(!thisMute)) {
            ++cleared;
            VGMDAT[0][idx] = 1;
            VGMVOL[0][idx] = 0;
        }

        // check for single tick mute (values on either side must be the same)
        if ((!prevMute) && (!nextMute) && (thisMute)) {
            // only if the notes are the same on both sides
            if ((idx != 0) && (idx != row-1)) {
                if ((VGMDAT[0][idx-1] == VGMDAT[0][idx+1]) && (VGMVOL[0][idx-1] == VGMVOL[0][idx+1])) {
                    ++filled;
                    VGMDAT[0][idx] = VGMDAT[0][idx-1];
                    VGMVOL[0][idx] = VGMVOL[0][idx-1];
                }
            }
        }
    }
    printf("%d single-tick notes muted (lossy)\n", cleared);
    printf("%d single-tick muted filled (lossy)\n", filled);

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


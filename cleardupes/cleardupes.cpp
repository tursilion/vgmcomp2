// cleardupes.cpp : Defines the entry point for the console application.
// clear duplicate notes from two channels

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 2
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];

#define RENAME ".dupes.old"

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
// The volume table at this point has been adjusted to the TI volume values
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0xf)) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 Duplicates Tool - v20200919\n\n");

    if (argc < 3) {
        printf("cleardupes <chan1> <chan2>\n");
        printf("Mutes channel 2 any time both channels have the same note.\n");
        printf("If chan2 was louder, its volume is transfered to chan1.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
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

    // handle the duplicate check
    int chan2mutes = 0;
    int chan1vols = 0;

    for (int idx = 0; idx<row; ++idx) {
        if (VGMDAT[0][idx] == VGMDAT[1][idx]) {
            // notes are the same, check volume only
            if (VGMVOL[1][idx] > VGMVOL[0][idx]) {
                VGMVOL[0][idx] = VGMVOL[1][idx];
                ++chan1vols;
            }
            VGMVOL[1][idx]=0;
            ++chan2mutes;
        }
    }

    printf("%d channel 1 volume updated\n", chan1vols);
    printf("%d channel 2 mutes\n", chan2mutes);

    // now we just have to spit the data back out to new files
    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        FILE *fout = fopen(szFilename[idx], "w");
        if (NULL == fout) {
            printf("Failed to open output file '%s', err %d\n", szFilename[idx], errno);
            return 1;
        }
        printf("Writing: %s\n", szFilename[idx]);

        for (int r=0; r<row; ++r) {
            fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[idx][r], VGMVOL[idx][r]);
        }
        fclose(fout);
    }

    printf("** DONE **\n");
    
    return 0;
}


// mergenoise.cpp : Defines the entry point for the console application.
// merge two noise channels into one by taking the loudest
// could merge tone channels too, but that's less useful

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

#define RENAME ".merge.old"

// for noise, mute can't be determined via frequency, only by volume
int main(int argc, char *argv[])
{
	printf("VGMComp2 Noise Merge Tool - v20200919\n\n");

    if (argc < 3) {
        printf("mergenoise <chan1> <chan2>\n");
        printf("Merges chan2 into chan 1 by taking the louder of the two channels.\n");
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

    // merge the second channel into the first one, then output that
    // to the output file. Just a straight alternate of the tones.
    // Even lines, chan 1, odd lines, chan 2, but only if one isn't muted
    // So.. 
    // 1) if channel 2 is mute, do nothing
    // 2) if channel 1 is mute, merge channel 2
    // 3) merge channel 2 only if it's louder than channel 1
    int movedNotes = 0;
    int overriddenNotes = 0;

    for (int idx = 0; idx<row; ++idx) {
        if (VGMVOL[1][idx] > 0) {
            if ((VGMVOL[0][idx] == 0)||(VGMVOL[1][idx] > VGMVOL[0][idx])) {
                // see which one it was
                if (VGMVOL[0][idx] == 0) {
                    ++movedNotes;
                } else {
                    ++overriddenNotes;
                }
                // copy channel 2 to channel 1
                VGMVOL[0][idx] = VGMVOL[1][idx];
                VGMDAT[0][idx] = VGMDAT[1][idx];
            }
        }
    }

    printf("%d tones moved      (non-lossy)\n", movedNotes);
    printf("%d tones overridden (lossy)\n", overriddenNotes);

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


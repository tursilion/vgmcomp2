// arptones.cpp : Defines the entry point for the console application.
// arpeggio two tone channels into one
// could arpeggio noise channels too, but that's less useful

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

#define RENAME ".arp.old"

// codes for noise processing (if not periodic (types 0-3), it's white noise (types 4-7))
// only NOISE_TRIGGER makes it to the output file
#define NOISE_TRIGGER  0x10000

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
	printf("VGMComp2 Arpeggio Tool - v20201005\n\n");

    if (argc < 3) {
        printf("arptones <chan1> <chan2>\n");
        printf("Performs an arpeggio where appropriate of two channels.\n");
        printf("chan2 is merged into chan1, alternating when conflicting.\n");
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
    // 3) merge channel 2 only if it's an odd line
    int movedNotes = 0;
    int arpedNotes = 0;

    for (int idx = 0; idx<row; ++idx) {
        if (!muted(1,idx)) {
            if ((muted(0,idx))||(idx&1)) {
                // see which one it was
                if (muted(0,idx)) {
                    ++movedNotes;
                } else {
                    ++arpedNotes;
                }
                // copy channel 2 to channel 1
                VGMVOL[0][idx] = VGMVOL[1][idx];
                VGMDAT[0][idx] = VGMDAT[1][idx];
            }
        }
    }

    printf("%d tones moved   (non-lossy)\n", movedNotes);
    printf("%d tones arped   (lossy)\n", arpedNotes);

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


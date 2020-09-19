// arp3tones.cpp : Defines the entry point for the console application.
// arpeggio three tone channels into one
// could arpeggio noise channels too, but that's less useful

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 3
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];

#define RENAME ".arp3.old"

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
	printf("VGMComp2 Arpeggio3 Tool - v20200919\n\n");

    if (argc < 4) {
        printf("arptones <chan1> <chan2> <chan3>\n");
        printf("Performs an arpeggio where appropriate of three channels.\n");
        printf("chan3 and chan2 are merged into chan1, alternating when conflicting.\n");
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

    // merge the second and third channel into the first one, then output that
    // to the output file. Just a straight alternate of the tones.
    // Chan1, Chan2, Chan3, if active.
    int movedNotes = 0;
    int arpedNotes = 0;

    for (int idx = 0; idx<row; ++idx) {
        int cnt = 0;
        if (!muted(0,idx)) ++cnt;
        if (!muted(1,idx)) ++cnt;
        if (!muted(2,idx)) ++cnt;
        // if it's more than 1, then this is an arp, not a move

        switch (idx%3) {
        case 0:
            // preferred is 0 if there is a conflict
            if (muted(0,idx)) {
                // prefer 1 over 2
                if (!muted(1,idx)) {
                    // copy 1
                    VGMVOL[0][idx] = VGMVOL[1][idx];
                    VGMDAT[0][idx] = VGMDAT[1][idx];
                    if (cnt == 1) ++movedNotes;
                } else if (!muted(2,idx)) {
                    // copy 2
                    VGMVOL[0][idx] = VGMVOL[2][idx];
                    VGMDAT[0][idx] = VGMDAT[2][idx];
                    if (cnt == 1) ++movedNotes;
                }
            }
            break;
        case 1:
            // preferred is 1 if there is a conflict
            if (muted(1,idx)) {
                // prefer 2 over 0
                if (!muted(2,idx)) {
                    // copy 2
                    VGMVOL[0][idx] = VGMVOL[2][idx];
                    VGMDAT[0][idx] = VGMDAT[2][idx];
                    if (cnt == 1) ++movedNotes;
                }
                // nothing to do to copy 0
            } else {
                // copy 1
                VGMVOL[0][idx] = VGMVOL[1][idx];
                VGMDAT[0][idx] = VGMDAT[1][idx];
                if (cnt == 1) ++movedNotes;
            }
            break;
        case 2:
            // preferred is 2 if there is a conflict
            if (muted(2,idx)) {
                // prefer 0 over 1
                // nothing to do to copy 0
                if ((muted(0,idx))&&(!muted(1,idx))) {
                    // copy 1
                    VGMVOL[0][idx] = VGMVOL[1][idx];
                    VGMDAT[0][idx] = VGMDAT[1][idx];
                    if (cnt == 1) ++movedNotes;
                }
            } else {
                // copy 2
                VGMVOL[0][idx] = VGMVOL[2][idx];
                VGMDAT[0][idx] = VGMDAT[2][idx];
                if (cnt == 1) ++movedNotes;
            }
            break;
        }
        if (cnt > 1) ++arpedNotes;
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

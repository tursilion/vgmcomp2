// retunenoise.cpp : Defines the entry point for the console application.
// merge a channel into another, but only when that channel is silent

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
bool forcePeriodic = false;
bool forceWhite = false;

#define RENAME ".retunenoise.old"

// mask for data
#define NOISE_FLAGS    0xF0000
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

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
	printf("VGMComp2 RetuneNoise Tool - v20240414\n\n");

    if (argc < 3) {
        printf("retunenoise <noise_chan> <tone_chan> [P|W]\n");
        printf("Merges tone_chan into noise_chan when not muted.\n");
        printf("Noise volume is preserved, as are flags.\n");
        printf("Optionally, add a 'P' or 'W' to force periodic or\n");
        printf("white noise when overwriting. Usually will want P\n");
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
    if (arg < argc) {
        if (argv[arg][0]=='P') {
            forcePeriodic = true;
            printf("Will force periodic noise\n");
        }
        else if (argv[arg][0]=='W') {
            forceWhite = true;
            printf("Will force white noise\n");
        }
        else {
            printf("Unknown argument '%s'\n", argv[arg]);
            return 1;
        }
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
            // since we were successful, also rename the source file we plan to overwrite
            if (idx == 0) {
                char buf[1024];
                sprintf(buf, "%s" RENAME, szFilename[idx]);
                if (rename(szFilename[idx], buf)) {
                    printf("Error renaming '%s' to '%s', code %d\n", szFilename[idx], buf, errno);
                    return 1;
                }
            }
        }
    }

    printf("Imported %d rows\n", row);

    // merge the second channel into the first one, then output that
    // to the output file. Second channel /always/ gets priority. Do not move volume,
    // and do not overwrite noise flags.
    int movedNotes = 0;
    int lostNotes = 0;
    int typeChanged = 0;

    for (int idx = 0; idx<row; ++idx) {
        if (!muted(1,idx)) {
            if ((!muted(0,idx)) && ((VGMDAT[0][idx]&(~NOISE_FLAGS)) != (VGMDAT[1][idx] & (~NOISE_FLAGS)))) {
                // we will overwrite an existing note
                ++lostNotes;
            }
            // go ahead and move it
            ++movedNotes;
            // copy channel 2 to channel 1
            VGMDAT[0][idx] = (VGMDAT[0][idx]&NOISE_FLAGS) | (VGMDAT[1][idx] & (~NOISE_FLAGS));
            if (forcePeriodic) {
                if ((VGMDAT[0][idx] & NOISE_PERIODIC) == 0) {
                    VGMDAT[0][idx] |= NOISE_PERIODIC;
                    ++typeChanged;
                }
            } else if (forceWhite) {
                if (VGMDAT[0][idx] & NOISE_PERIODIC) {
                    VGMDAT[0][idx] &= ~NOISE_PERIODIC;
                    ++typeChanged;
                }
            }
        }
    }

    printf("%d tones moved        (non-lossy)\n", movedNotes);
    printf("%d noise tones lost   (lossy)\n", lostNotes);
    printf("%d noise type changed (lossy)\n", typeChanged);

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


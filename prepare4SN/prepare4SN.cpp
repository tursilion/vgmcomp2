// prepare4SN.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 4
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];

// codes for noise processing (if not periodic (types 0-3), it's white noise (types 4-7))
// only NOISE_TRIGGER makes it to the output file
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
};

// return absolute value
inline int ABS(int x) {
    if (x<0) return -x;
    else return x;
}

// input: 8 bit unsigned audio (0-mute, 255-max)
// output: 15 (silent) to 0 (max)
int mapVolume(int nTmp) {
	int nBest = -1;
	int nDistance = INT_MAX;
	int idx;

	// testing says this is marginally better than just (nTmp>>4)
	for (idx=0; idx < 16; idx++) {
		if (ABS(volumeTable[idx]-nTmp) < nDistance) {
			nBest = idx;
			nDistance = ABS(volumeTable[idx]-nTmp);
		}
	}

    // don't return mute unless the input was mute
    if ((nBest == 15) && (nTmp != 0)) --nBest;

	// return the index of the best match
	return nBest;
}

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
	printf("VGMComp2 PSG Prep Tool - v20201006\n\n");

    if (argc < 6) {
        printf("prepare4SN <tone1> <tone2> <tone3> <noise> <output>\n");
        printf("  You must specify 4 channels to prepare - three tone and one noise.\n");
        printf("  It is up to you to ensure that you are placing the correct data.\n");
        printf("  The frequency is not verified - again, up to you to verify they are the same.\n");
        printf("  You may leave a channel blank by passing '-' instead of a filename.\n");
        printf("  Volumes will be mapped to the PSG range, and the noise channel\n");
        printf("  will be mapped against tone3 where possible. (Pre-process using\n");
        printf("  other tools to force it one way or another.)\n");
        printf("  The output file is ready for compression with vgmcomp2.\n");
        return 1;
    }

    // open up the files (if defined)
    // continue only if at least one opens!
    bool cont = false;

    for (int idx=0; idx<MAXCHANNELS; ++idx) {
        if (idx+1 >= argc) {
            printf("Out of filename arguments.\n");
            return 1;
        }
        if ((argv[idx+1][0]=='-')&&(argv[idx+1][1]=='\0')) {
            printf("Channel %d (%s) free\n", idx, idx == 3 ? "noise":"tone");
            fp[idx] = NULL;
        } else {
            fp[idx] = fopen(argv[idx+1], "r");
            if (NULL == fp[idx]) {
                printf("Failed to open file '%s' for channel %d, code %d\n", argv[idx+1], idx, errno);
                return 1;
            }
            printf("Opened %s channel %d: %s\n", idx == 3 ? "noise":"tone", idx, argv[idx+1]);
            cont = true;
        }
    }

    // read until one of the channels ends
    int row = 0;
    while (cont) {
        for (int idx=0; idx<MAXCHANNELS; ++idx) {
            if (NULL == fp[idx]) {
                VGMDAT[idx][row]=1;
                VGMVOL[idx][row]=0;
            } else {
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
        }
    }

    printf("Imported %d rows\n", row);

    // pass through each imported row. We have three tasks to do:
    // 1) resample the volume to the appropriate logarithmic value
    // 2) Clip any bass notes with a greater shift than 0x3ff
    // 3) handle the noise channel - setting either a fixed shift rate or adapting with voice 3, if free
    int customNoises = 0;
    int noisesMapped = 0;
    int tonesMoved = 0;
    int tonesClipped = 0;

    // to improve compression, pre-process the channel, and any volume muted channels,
    // make sure they are the same frequency as the channel above them
    int mutemaps = 0;
    for (int idx=1; idx<row; ++idx) {   // start at 1
        for (int ch=0; ch<4; ++ch) {
            if (VGMVOL[ch][idx] == 0) {   // NOT converted yet
                if (VGMDAT[ch][idx-1] != VGMDAT[ch][idx]) {
                    ++mutemaps;
                    VGMDAT[ch][idx] = VGMDAT[ch][idx-1];
                }
            }
        }
    }

    for (int idx = 0; idx<row; ++idx) {
        // the volume is easy
        for (int ch = 0; ch < 4; ++ch) {
            VGMVOL[ch][idx] = mapVolume(VGMVOL[ch][idx]);
        }

        // the clipping is simple enough too - we also clip the noise channel
        for (int ch = 0; ch < 4; ++ch) {
            if ((VGMDAT[ch][idx]&NOISE_MASK) > 0x3ff) {
                VGMDAT[ch][idx] = (VGMDAT[ch][idx] & (~NOISE_MASK)) | 0x3ff;
                ++tonesClipped;
            }
        }

        // now we try to map the noise. Three phases:
        // 1) Shift rate matches fixed value - just map directly
        // 2) Channel 3 is free (or can be moved) - apply to channel 3
        // 3) Map to nearest fixed shift rate
        // Remember to watch for the periodic flag and keep the trigger flag
        int noiseShift = VGMDAT[3][idx] & NOISE_MASK;
        bool bPeriodic = (VGMDAT[3][idx] & NOISE_PERIODIC) ? true : false;
        bool bTrigger = (VGMDAT[3][idx] & NOISE_TRIGGER) ? true : false;
        int out = -1;

        if (VGMVOL[3][idx] == 0xf) {
            // we're muted, nothing to be mapped - save last note
            if (idx == 0) {
                out = 1;    // match the tones for default output (this is shift rate 32)
            } else {
                out = VGMDAT[3][idx-1]&0x03;    // take just the noise shift, we add trigger and periodic below
            }
        } else {
            // check for a fixed shift rate
            if (noiseShift == 16) {
                out = 0;
            } else if (noiseShift == 32) {
                out = 1;
            } else if (noiseShift == 64) {
                out = 2;
            }

            if (-1 == out) {
                // it wasn't fixed, so can we just copy it over into channel 2?
                if ((muted(2, idx))||(muted(1, idx))||(muted(0, idx))) {
                    // at least one of them are muted
                    if (!muted(2,idx)) {
                        // we need to move channel 2
                        if (muted(1,idx)) {
                            VGMDAT[1][idx] = VGMDAT[2][idx];    // move 2->1
                            VGMVOL[1][idx] = VGMVOL[2][idx];
                            ++tonesMoved;
                        } else if (muted(0,idx)) {
                            // this must be true, otherwise something weird happened
                            VGMDAT[0][idx] = VGMDAT[2][idx];    // move 2->0
                            VGMVOL[0][idx] = VGMVOL[2][idx];
                            ++tonesMoved;
                        } else {
                            printf("Internal consistency error at row %d\n", idx);
                            return 1;
                        }
                    } else {
                        ++customNoises;
                    }
                    // channel 2 is ready to use, overwrite it
                    VGMDAT[2][idx] = VGMDAT[3][idx]&NOISE_MASK;    // take the custom shift rate
                    VGMVOL[2][idx] = 0xf;               // keep it muted though
                    out = 3;                            // custom shift mode
                }
            }

            if (-1 == out) {
                // we still don't have one, so map to the closest shift
                int diff16 = ABS(16 - (VGMDAT[3][idx]&NOISE_MASK));
                int diff32 = ABS(32 - (VGMDAT[3][idx]&NOISE_MASK));
                int diff64 = ABS(64 - (VGMDAT[3][idx]&NOISE_MASK));
                if ((diff16 <= diff32) && (diff16 <= diff64)) {
                    out = 0;    // use 16
                } else if ((diff32 <= diff16) && (diff32 <= diff64)) {
                    out = 1;    // use 32
                } else {
                    out = 2;    // use 64
                }
                ++noisesMapped;
            }

            if (-1 == out) {
                // not possible...
                printf("Second internal consistency error at row %d\n", idx);
                return 1;
            }
        }

        if (!bPeriodic) out += 4;               // make it white noise
        if (bTrigger) out |= NOISE_TRIGGER;     // save the trigger flag
        VGMDAT[3][idx] = out;                   // record the translated noise
    }

    printf("%d custom noises (non-lossy)\n", customNoises);
    printf("%d tones moved   (non-lossy)\n", tonesMoved);
    printf("%d mutes mapped  (non-lossy)\n", mutemaps);
    printf("%d tones clipped (lossy)\n", tonesClipped);
    printf("%d noises mapped (lossy)\n", noisesMapped);

    // now we just have to spit the data back out.
    // it's a single line output
    FILE *fout = fopen(argv[5], "w");
    if (NULL == fout) {
        printf("Failed to open output file '%s', err %d\n", argv[5], errno);
        return 1;
    }

    for (int idx=0; idx<row; ++idx) {
        fprintf(fout, "0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X\n",
                VGMDAT[0][idx], VGMVOL[0][idx], VGMDAT[1][idx], VGMVOL[1][idx],
                VGMDAT[2][idx], VGMVOL[2][idx], VGMDAT[3][idx], VGMVOL[3][idx]);
    }
    fclose(fout);

    printf("** DONE **\n");
    
    return 0;
}


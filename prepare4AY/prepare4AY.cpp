// prepare4AY.cpp : Defines the entry point for the console application.
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

// codes for noise processing
#define NOISE_MASK     0x00FFF

// lookup table to map PSG volume to linear 8-bit. Matt H generated this
// table from his accurate AY emulation
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
#if 1
    // AY table by Matt - pure logarithmic
    255,181,128,90,64,45,32,
    23,
    16,11,8,6,4,3,2,0
#elif 0
    // curve by V_Soft and Lion17 (converted to 8 bit)
    254, 220, 185, 147, 119, 96, 70,
    40,
    36,19,13,8,5,3,2,0
#else
    // SN table
    254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
#endif
};

// return absolute value
inline int ABS(int x) {
    if (x<0) return -x;
    else return x;
}

// input: 8 bit unsigned audio (centers on 128)
// output: 0 (silent) to 15 (max)
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

	// return the index of the best match - note that AY is inverted
    // from the SN PSG - 0 is mute and 15 is max
	return 15-nBest;
}

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
// The volume table at this point has been adjusted to the AY volume values
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0)) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 AY Prep Tool - v20200919\n\n");

    if (argc < 6) {
        printf("prepare4AY <tone1> <tone2> <tone3> <noise> <output>\n");
        printf("  You must specify 4 channels to prepare - three tone and one noise.\n");
        printf("  It is up to you to ensure that you are placing the correct data.\n");
        printf("  The frequency is not verified - again, up to you to verify they are the same.\n");
        printf("  You may leave a channel blank by passing '-' instead of a filename.\n");
        printf("  Volumes will be mapped to the AY range, and the noise volume\n");
        printf("  will be mapped against whichever tone is closest. (Pre-process using\n");
        printf("  other tools to force it one way or another.)\n");
        printf("  The output file is ready for compression with vgmcomp2.\n");
        return 1;
    }

    // open up the files (if defined)
    // continue only if at least one opens!
    bool cont = false;

    for (int idx=0; idx<4; ++idx) {
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
        for (int idx=0; idx<4; ++idx) {
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

    for (int idx=0; idx<4; ++idx) {
        if (NULL != fp[idx]) {
            fclose(fp[idx]);
            fp[idx] = NULL;
        }
    }

    printf("Imported %d rows\n", row);

    // pass through each imported row. We have four tasks to do:
    // 1) resample the volume to the appropriate logarithmic value
    // 2) Clip any bass notes with a greater shift than 0xfff
    // 3) Clip the noise shift if greater than 0x1f
    // 4) See if there is a muted tone channel we can put noise on
    //    If not, map the noise volume to the closest tone channel
    int noisesMapped = 0;
    int noisesClipped = 0;
    int tonesMoved = 0;
    int tonesClipped = 0;

    for (int idx = 0; idx<row; ++idx) {
        // the volume is easy
        for (int ch = 0; ch < 4; ++ch) {
            VGMVOL[ch][idx] = mapVolume(VGMVOL[ch][idx]);
        }
        // volume is now 0 (mute) to 15 (loudest)

        // the clipping is simple enough too
        for (int ch = 0; ch < 4; ++ch) {
            if ((VGMDAT[ch][idx]&NOISE_MASK) > 0xfff) {
                VGMDAT[ch][idx] = (VGMDAT[ch][idx] & (~NOISE_MASK)) | 0xfff;
                ++tonesClipped;
            }
        }

        // handle the noise frequency clip
        if ((VGMDAT[3][idx]&NOISE_MASK) > 0x1f) {
            VGMDAT[3][idx] = (VGMDAT[3][idx] & (~NOISE_MASK)) | 0x1f;
            ++noisesClipped;
        }

        // now we try to map the noise volume. Three phases:
        // 1) Volume matches existing volume - just map it there
        // 2) Channel 3 is free (or can be moved) - apply to channel 3
        // 3) Map to nearest volume
        int noiseVol = VGMVOL[3][idx] & 0xf;
        int out = -1;   // to contain the mapper byte (0000 CBAN - C,B,A are noise disable, N is the note disable for C, normally 0)

        if (noiseVol == 0) {
            // we're muted, nothing to be mapped - turn off all noise
            // we also turn the tone back on, but it's in charge of that
            // any note we moved should be back if it didn't change
            out = 0x0e;     // all noise muted, all tone active (0x38 >> 2)
        } else {
            // check for a matching volume
            if (VGMVOL[0][idx] == noiseVol) {
                out = 0x0c;     // play on channel A (30 >> 2)
            } else if (VGMVOL[1][idx] == noiseVol) {
                out = 0x0a;     // play on channel B (28 >> 2)
            } else if (VGMVOL[2][idx] == noiseVol) {
                out = 0x06;     // play on channel C (18 >> 2)
            }

            if (-1 == out) {
                // it wasn't matching, so can we just copy it over into channel 2?
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
                    }
                    // channel 2 is ready to use, grab it's volume and mute it's tone
                    VGMVOL[2][idx] = noiseVol;    // give it our volume
                    out = 0x07;                   // noise on C, tone C muted (1c>>2)
                }
            }

            if (-1 == out) {
                // we still don't have one, so map to the closest volume
                // we should include mute in this test!
                int diffC = ABS(noiseVol - (VGMVOL[2][idx]&0xf));
                int diffB = ABS(noiseVol - (VGMVOL[1][idx]&0xf));
                int diffA = ABS(noiseVol - (VGMVOL[0][idx]&0xf));
                int diff0 = ABS(noiseVol - 0);
                if ((diffC <= diffB) && (diffC <= diffA) && (diffC <= diff0)) {
                    out = 0x06;     // play on channel C
                } else if ((diffB <= diffC) && (diffB <= diffA) && (diffB <= diff0)) {
                    out = 0x0a;     // play on channel B
                } else if ((diffA <= diffC) && (diffA <= diffB) && (diffA <= diff0)) {
                    out = 0x0c;     // play on channel A
                } else {
                    out = 0x0e;     // just mute it
                }
                ++noisesMapped;
            }

            if (-1 == out) {
                // not possible...
                printf("Second internal consistency error at row %d\n", idx);
                return 1;
            }
        }

        VGMVOL[3][idx] = out;   // record the translated volume
    }

    printf("%d tones moved   (non-lossy)\n", tonesMoved);
    printf("%d tones clipped (lossy)\n", tonesClipped);
    printf("%d noises clipped (lossy)\n", noisesClipped);
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


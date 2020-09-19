// prepare4SID.cpp : Defines the entry point for the console application.
//

// TODO: we might need to differently scale noise rates - have to
// come back to this to determine the correct scale. 

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 3
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];

bool isNoise[3];                            // keep track whether a channel is noise

// lookup table to map SID volume to linear 8-bit. (SID is also linear, 0=mute)
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
	255,238,221,204,187,170,153,
	136,
	119,102,85,68,51,34,17,0
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

	// return the index of the best match - note that SID is inverted
    // from the SN PSG - 0 is mute and 15 is max
	return 15-nBest;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 SID Prep Tool - v20200919\n\n");

    if (argc < 5) {
        printf("prepare4SID <tone1|noise1> <tone2|noise2> <tone3|noise3> <output>\n");
        printf("  You must specify 3 channels to prepare - any combination of tone and/or noise.\n");
        printf("  It is up to you to ensure that you are placing the correct data.\n");
        printf("  You may leave a channel blank by passing '-' instead of a filename.\n");
        printf("  The output file is ready for compression with vgmcomp2.\n");
        return 1;
    }

    int arg = 1;
    memset(isNoise, 0, sizeof(isNoise));

    // open up the files (if defined)
    // continue only if at least one opens!
    bool cont = false;

    // only /three/ channels on this chip!
    for (int idx=0; idx<3; ++idx) {
        if (idx+arg >= argc) {
            printf("Out of filename arguments.\n");
            return 1;
        }
        if ((argv[idx+arg][0]=='-')&&(argv[idx+arg][1]=='\0')) {
            printf("Channel %d free\n", idx);
            fp[idx] = NULL;
        } else {
            fp[idx] = fopen(argv[idx+arg], "r");
            if (NULL == fp[idx]) {
                printf("Failed to open file '%s' for channel %d, code %d\n", argv[idx+arg], idx, errno);
                return 1;
            }
            if (NULL != strstr(argv[idx+arg], "_noi")) {
                isNoise[idx] = true;
            }
            printf("Opened SID channel %d (%s): %s\n", idx, isNoise[idx] ? "noise":"tone ", argv[idx+arg]);
            cont = true;
        }
    }

    // read until one of the channels ends
    int row = 0;
    while (cont) {
        for (int idx=0; idx<3; ++idx) {
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

    for (int idx=0; idx<3; ++idx) {
        if (NULL != fp[idx]) {
            fclose(fp[idx]);
            fp[idx] = NULL;
        }
    }

    printf("Imported %d rows\n", row);

    // pass through each imported row. The SID supports 16-bit shift
    // rates, so we low frequency is not a problem (although,
    // I think the importers will not generate anything lower frequency
    // than 0xFFF as written today... that's adequate for the supported
    // chips so far. But we'll assume a future converter might).
    // It does have a problem with higher frequencies, though, maxing
    // out around 3905Hz, which is quite audible.
    // However, the shift rates are probably wrong for everything, so:
    // 1) resample the volume to the appropriate linear value
    // 2) resample the shift rates to the SID shift rates
    // 3) mute any frequencies that end up out of range (have to do this by volume)
    int muted = 0;
    for (int idx = 0; idx<row; ++idx) {
        // just do both resamples right here...
        for (int ch = 0; ch < 3; ++ch) {
            VGMVOL[ch][idx] = mapVolume(VGMVOL[ch][idx]);

            // the TI SID blaster is using an exactly 1MHZ clock, so it's not
            // the same as the real C64 at .985248 (PAL) or 1.022727 (NTSC).
            // Since we have the math, we might as well use it. ;) The old
            // 0.0596 was an approximation that actually matched 1MHz.
            // SN HZ = 111860.8/code
            // CONSTANT = 256^3 / CLOCK
            // SID code = hz * CONSTANT
            // newcode = (111860.8/code) * (16777216 / CLOCK)
            VGMDAT[ch][idx] = int((111860.8/VGMDAT[ch][idx]) * (16777216.0 / 1000000) + 0.5);
            // In fairness, the range makes it down to about 29 SN ticks, which
            // is only 7 notes off the end of the E/A table
            if (VGMDAT[ch][idx] > 0xffff) {
                VGMDAT[ch][idx] = 0xffff;
                if (VGMVOL[ch][idx] > 0) {
                    VGMVOL[ch][idx] = 0;
                    ++muted;
                }
            }
        }
    }
    printf("%d notes muted (lossy)\n", muted);

    // now we just have to spit the data back out.
    // it's a single line output
    FILE *fout = fopen(argv[arg+3], "w");
    if (NULL == fout) {
        printf("Failed to open output file '%s', err %d\n", argv[arg+3], errno);
        return 1;
    }

    // There's no fourth channel, but the rest is all set.
    for (int idx=0; idx<row; ++idx) {
        fprintf(fout, "0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X\n",
                VGMDAT[0][idx], VGMVOL[0][idx], VGMDAT[1][idx], VGMVOL[1][idx],
                VGMDAT[2][idx], VGMVOL[2][idx], 1, 0);
    }
    fclose(fout);

    printf("** DONE **\n");
    
    return 0;
}


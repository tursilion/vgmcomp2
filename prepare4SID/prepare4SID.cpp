// prepare4SID.cpp : Defines the entry point for the console application.
//

// TODO: we might need to differently scale noise rates - have to
// come back to this to determine the correct scale. That would also
// requiring keeping track of which channels ARE noise after all ;)

#include "stdafx.h"
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 3
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
FILE *fp[MAXCHANNELS];

// codes for noise processing
#define NOISE_MASK     0x00FFF

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

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
// The volume table at this point has been adjusted to the SID volume values
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0)) {
        return true;
    }
    return false;
}

int main(int argc, char *argv[])
{
	printf("VGMComp2 SID Prep Tool - v20200620\n\n");

    if (argc < 5) {
        printf("prepare4SID <tone1|noise1> <tone2|noise2> <tone3|noise3> <output>\n");
        printf("  You must specify 3 channels to prepare - any combination of tone and/or noise.\n");
        printf("  It is up to you to ensure that you are placing the correct data.\n");
        printf("  The frequency is not verified - again, up to you to verify they are the same.\n");
        printf("  You may leave a channel blank by passing '-' instead of a filename.\n");
        printf("  Volumes will be mapped to the SID range, however the player must be\n");
        printf("  configured correctly for tone/noise as that information is not stored.\n");
        printf("  The output file is ready for compression with vgmcomp2.\n");
        return 1;
    }

    // TODO: you could probably store the noise/tone config in the first row of data
    // and just allow it to be part of the datastream without costing must, but for
    // now we're just going to force the player to KNOW what each channel is.

    // open up the files (if defined)
    // continue only if at least one opens!
    bool cont = false;

    // only /three/ channels on this chip!
    for (int idx=0; idx<3; ++idx) {
        // we don't need to track whether it's noise or tone, so never mind the extension....
        if (argv[idx+1][0]=='-') {
            printf("Channel %d free\n", idx);
            fp[idx] = NULL;
        } else {
            fp[idx] = fopen(argv[idx+1], "r");
            if (NULL == fp[idx]) {
                printf("Failed to open file '%s' for channel %d, code %d\n", argv[idx+1], idx, errno);
                return 1;
            }
            printf("Opened SID channel %d: %s\n", idx, argv[idx+1]);
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
        if (cont) ++row;
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
    // 3) mute any frequencies that end up out of range
    int muted = 0;
    for (int idx = 0; idx<row; ++idx) {
        // just do both resamples right here...
        for (int ch = 0; ch < 3; ++ch) {
            VGMVOL[ch][idx] = mapVolume(VGMVOL[ch][idx]);

            // the current shift rates are based on the SN standard shift, we need
            // to adapt them to the SID shifts. This is the ratio.
            // SN HZ = 111860.8/code
            // SID code = hz/0.0596
            // convert newcode = (111860.8/code)/0.0596
            VGMDAT[ch][idx] = int(((111860.8/VGMDAT[ch][idx]) / 0.0596) + 0.5);
            // In fairness, the range makes it down to about 29 SN ticks, which
            // is only 7 notes off the end of the E/A table
            if (VGMDAT[ch][idx] > 0xffff) {
                VGMDAT[ch][idx] = 0xffff;
                VGMVOL[ch][idx] = 0;
                ++muted;
            }
        }
    }
    printf("%d notes muted (lossy)\n", muted);

    // now we just have to spit the data back out.
    // it's a single line output
    FILE *fout = fopen(argv[4], "w");
    if (NULL == fout) {
        printf("Failed to open output file '%s', err %d\n", argv[5], errno);
        return 1;
    }

    // the compressor requires a fourth channel, so we'll still provide it
    // the format will zero it out so that it only costs a few bytes and
    // the player will ignore it.
    for (int idx=0; idx<row; ++idx) {
        fprintf(fout, "0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X,0x%05X,0x%X\n",
                VGMDAT[0][idx], VGMVOL[0][idx], VGMDAT[1][idx], VGMVOL[1][idx],
                VGMDAT[2][idx], VGMVOL[2][idx], 1,0);
    }
    fclose(fout);

    printf("** DONE **\n");
    
    return 0;
}


// changespeed.cpp : Defines the entry point for the console application.
// changes the playback speed of a tune by resampling
//
// This is MOSTLY meant for going from 60hz to 30hz, but it can also
// do 60hz to 50hz or other useful rates.
//
// This is a lossy conversion - the original timing is approximated.
// Only 2-note arpeggios and noise triggers are attempted to be preserved.
// This one does the entire song because you never want to change
// only one channel.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 2                       // we only need to process one channel at a time, but we use the second
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool verbose = true;
bool checkNoiseTrigger = true;
bool checkArpeggio = true;
bool saveVolume = true;

#define RENAME ".speed.old"

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
	printf("VGMComp2 Change Speed Tool - v20201005\n\n");

    if (argc < 3) {
        printf("changespeed [-q] [-notrigger] [-noarp] [-novol] <song> <output HZ>\n");
        printf("Changes the speed of an entire song in a lossy manner,\n");
        printf("(meaning data is discarded, so don't repeat it on the same song)\n");
        printf("-q - quiet verbose output\n");
        printf("-notrigger - don't check removed rows for noise trigger\n");
        printf("-noarp - don't check removed rows for arpeggio\n");
        printf("-novol - don't check removed rows for maximum volume\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and updated.\n");
        printf("output HZ - desired playback rate in HZ\n");
        printf("  Note that testplayer today only supports 25,50,30 and 60hz\n");
        printf("  Also note that the playback rate on the target is entirely up to you.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else if (strcmp(argv[arg], "-notrigger") == 0) {
            checkNoiseTrigger = false;
        } else if (strcmp(argv[arg], "-noarp") == 0) {
            checkArpeggio = false;
        } else if (strcmp(argv[arg], "-novol") == 0) {
            saveVolume = false;
        } else {
            printf("Unrecognized option '%s'\n", argv[arg]);
            return -1;
        }
        ++arg;
    }

    if (arg+2 > argc) {
        printf("Not enough arguments.\n");
        return -1;
    }

    // we're going to use a similar hacky loader that testplayer does, except one at a time
    char szFilename[1024];
    strncpy(szFilename, argv[arg], 1024);
    szFilename[1023] = '\0';

    ++arg;
    int outputHz = atoi(argv[arg]);
    if (outputHz == 0) {
        printf("Failed to parse output HZ argument.\n");
        return -1;
    }

    // we expect the file to end with "_ton00.60hz", except the ton might be noi, the
    // 00 might be any number from 00 to 99, and the 60hz might be any other number of hz.
    // Fun, eh?
    char *p = strrchr(szFilename, '_'); // this should be the start of our area of interest
    int cnt,hz;
    if (2 != sscanf(p, "_%*c%*c%*c%d.%dhz", &cnt, &hz)) {
        printf("Failed to parse filename, can not find _ton00.60hz (or equivalent)\n");
        return -1;
    }
    ++p;
    *p = '\0';  // filename is now filename_
    
    // safety check - malicious input. just cause I feel like it.
    // cause we're going to use it as a format string we can't let the
    // user provide any format specifiers for us.
    if (NULL != strchr(szFilename, '%')) {
        printf("Illegal filename.\n");
        return -1;
    }
    strcat(szFilename, "%s%02d.%dhz"); // filename now filename_%s%02d.%dhz

    if (hz == outputHz) {
        printf("File is already at target frequency.\n");
        return 0;
    }

    double ratio = (double)hz/ outputHz;
    if (verbose) {
        printf("Source file at %dHz\n", hz);
        printf("Desired rate   %dHz\n", outputHz);
        printf("Ratio: %lf\n", ratio);
    }

    int converted = 0;

    for (int idx=0; idx<200; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (idx < 100) {
            sprintf(testFile, szFilename, "ton", idx, hz);
        } else {
            sprintf(testFile, szFilename, "noi", idx-100, hz);
        }

        FILE *fp = fopen(testFile, "r");
        if (NULL == fp) continue;       // in fact, most will fail

        if (verbose) printf("Opened channel %d: %s\n", idx%100, testFile);

        // read until the channel ends
        bool cont = true;
        int row = 0;
        while (cont) {
            char buf[128];

            if (feof(fp)) {
                cont = false;
                break;
            }

            if (NULL == fgets(buf, 128, fp)) {
                cont = false;
                break;
            }
            if (2 != sscanf(buf, "0x%X,0x%X", &VGMDAT[0][row], &VGMVOL[0][row])) {
                if (2 != sscanf(buf, "%d,%d", &VGMDAT[0][row], &VGMVOL[0][row])) {
                    printf("Failed to parse line %d for channel %d\n", row+1, idx%100);
                    cont = false;
                    break;
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

        fclose(fp);

        // since we were successful, also rename the source file
        char buf[1100];
        sprintf(buf, "%s" RENAME, testFile);
        if (rename(testFile, buf)) {
            // this is kind of bad - we might be partially through converting the song?
            printf("Error renaming '%s' to '%s', code %d\n", testFile, buf, errno);
            return 1;
        }

        if (verbose) printf("Imported %d rows\n", row);

        // now just resample the result into channel 1, cause that's easier
        int outrow = 0;
        double inrow = 0;
        int lastinrow = 0;
        int triggersadded = 0;
        int arpeggiosmoved = 0;
        int volumessaved = 0;
        while ((outrow < MAXTICKS-4) && (int(inrow+.5) < row)) {
            // check for arpeggio or noise trigger by scanning skipped rows
            int nowinrow = int(inrow+.5);
            if (nowinrow - lastinrow > 1) {
                // row was skipped, so check if it has any important information
                if (idx >= 100) {
                    if (checkNoiseTrigger) {
                        // noise, so only check for lost trigger
                        if ((VGMDAT[0][nowinrow]&NOISE_TRIGGER)==0) {
                            for (int i2=lastinrow+1; i2<nowinrow; ++i2) {
                                if (VGMDAT[0][i2]&NOISE_TRIGGER) {
                                    VGMDAT[0][nowinrow] |= NOISE_TRIGGER;
                                    ++triggersadded;
                                    break;
                                }
                            }
                        }
                    }
                } else {
                    if (checkArpeggio) {
                        // tone, so check for missing arpeggio
                        if (VGMDAT[0][lastinrow] == VGMDAT[0][nowinrow]) {
                             for (int i2=lastinrow+1; i2<nowinrow; ++i2) {
                                 // just take the first one that differs
                                 if (VGMDAT[0][i2] != VGMDAT[0][lastinrow]) {
                                     VGMDAT[0][nowinrow] = VGMDAT[0][i2];
                                     ++arpeggiosmoved;
                                     break;
                                 }
                             }
                        }
                    }
                }
                if (saveVolume) {
                    // either one, keep the loudest volume
                    for (int i2=lastinrow+1; i2<nowinrow; ++i2) {
                        if (VGMVOL[0][i2] > VGMVOL[0][nowinrow]) {
                            VGMVOL[0][nowinrow] = VGMVOL[0][i2];
                            ++volumessaved;
                        }
                    }
                }
            }
            lastinrow = nowinrow;

            VGMVOL[1][outrow] = VGMVOL[0][nowinrow];
            VGMDAT[1][outrow] = VGMDAT[0][nowinrow];
            ++outrow;
            inrow+=ratio;
        }
        if (verbose) printf("Sampled %d rows\n", outrow);
        if ((verbose)&&(triggersadded>0)) printf("%d noise triggers inserted\n", triggersadded);
        if ((verbose)&&(arpeggiosmoved>0)) printf("%d arpeggios moved\n", arpeggiosmoved);
        if ((verbose)&&(volumessaved>0)) printf("%d volumes saved\n", volumessaved);

        // now we just have to spit the data back out to a new file
        // but it has a new filename, too!
        if (idx < 100) {
            sprintf(testFile, szFilename, "ton", idx, outputHz);
        } else {
            sprintf(testFile, szFilename, "noi", idx-100, outputHz);
        }

        FILE *fout = fopen(testFile, "w");
        if (NULL == fout) {
            printf("Failed to open output file '%s', err %d\n", testFile, errno);
            return 1;
        }
        printf("Writing: %s\n", testFile);
        for (int idx=0; idx<outrow; ++idx) {
            fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[1][idx], VGMVOL[1][idx]);
        }
        fclose(fout);

        ++converted;
    }

    printf("%d channels converted to %dHz\n", converted, outputHz);
    
    printf("** DONE **\n");
    
    return 0;
}


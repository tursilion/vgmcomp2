// reducetonecnt.cpp : Defines the entry point for the console application.
// Lossy resample of frequencies - intended to get complex tones under 256
// distinct frequencies.
//
// This one does the entire song because otherwise we don't know that we
// have reached the limit.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool isNoise[MAXCHANNELS];
#define TONEMASK 0xfff                      // note portion only, we have to be careful with noise flags
#define NOISEFLAGS 0xF0000
int oldIdx[MAXCHANNELS];                    // so we write back with the same index
int Freqs[0x1000];                          // our max note is supposed to be 0xFFF
bool verbose = true;

#define RENAME ".reducetone.old"

// musical note table (from Editor/Assembler manual) - used in the lossy note reduction code
int musicalnotes[] = {
	1017,960,906,855,807,762,719,679,641,605,571,539,
	508,480,453,428,404,381,360,339,320,302,285,269,
	254,240,226,214,202,190,180,170,160,151,143,135,
	127,120,113,107,101,95,90,85,80,76,71,67,64,60,
	57,53,50,48,45,42,40,38,36,34,32,30,28,27,25,24,
	22,21,20
};

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
    int noteTarget = 256;
    int noteTol = 1;
    bool accept = false;
    bool nonoise = false;

	printf("VGMComp2 Tone Count reduction Tool - v20201005\n\n");

    if (argc < 2) {
        printf("reducetonecount [-q] [-nonoise] [-accept] [-notes <x>] [-notetol <x>] <song>\n");
        printf("Removes less popular notes to reduce the total count in the song.\n");
        printf("For compression, 256 notes or fewer are required.\n");
        printf("-q - quiet verbose output\n");
        printf("-accept - accept a too-large note count - specify before -notes\n");
        printf("-notes <x> - reduce note count to 'x' or less. Set to 256 if not specified.\n");
        printf("-notetol <x> - tolerance - notes within 'x' of the TI musical frequency are considered tuned (default 1).\n");
        printf("-nonoise - do not include any noise channels in the conversion (helpful for SN)\n");
        printf("song - pass in the entire name of ONE channel, the entire song will be found and updated.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-q") == 0) {
            verbose = false;
        } else if (strcmp(argv[arg], "-accept") == 0) {
            accept = true;
        } else if (strcmp(argv[arg], "-nonoise") == 0) {
            nonoise = true;
        } else if (strcmp(argv[arg], "-notes") == 0) {
            ++arg;
            if (arg >= argc) {
                printf("Not enough arguments for -notes\n");
                return -1;
            }
            noteTarget = atoi(argv[arg]);
            if (noteTarget < 2) {
                // though what's the point of 2, either?
                printf("Invalid -notes count of %d\n", noteTarget);
                return -1;
            }
            if (noteTarget > 256) {
                printf("WARNING: note count of %d is more than a song can accept (max 256)\n", noteTarget);
                if (accept) {
                    printf("Assuming you know what you are doing...\n");
                } else {
                    return -1;
                }
            }
        } else if (strcmp(argv[arg], "-notetol") == 0) {
            ++arg;
            if (arg >= argc) {
                printf("Not enough arguments for -notetol\n");
                return -1;
            }
            noteTol = atoi(argv[arg]);
            if (noteTol < 0) {
                printf("Invalid -notetol count of %d\n", noteTol);
                return -1;
            }
            if (noteTol > 25) {
                printf("WARNING: note tolerance of %d doesn't make much sense... (max 25)\n", noteTol);
                if (accept) {
                    printf("Assuming you know what you are doing...\n");
                } else {
                    return -1;
                }
            }
        } else {
            printf("Unrecognized option '%s'\n", argv[arg]);
            return -1;
        }
        ++arg;
    }

    if (arg >= argc) {
        printf("Not enough arguments for filename.\n");
        return -1;
    }

    if (verbose) printf("Using musical note tolerance of %d\n", noteTol);

    // we're going to use a similar hacky loader that testplayer does
    char szFilename[1024];
    strncpy(szFilename, argv[arg], 1024);
    szFilename[1023] = '\0';

    // we expect the file to end with "_ton00.60hz", except the ton might be noi, the
    // 00 might be any number from 00 to 99, and the 60hz might be any other number of hz.
    // Fun, eh? Of course, we don't process the noi channel here, but we'll allow it as
    // input since all the similar commands do.
    char *p = strrchr(szFilename, '_'); // this should be the start of our area of interest
    if (NULL == p) {
        printf("Please enter the full name of a single channel, not the prefix. We'll find the rest!\n");
        return -1;
    }
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

    // try to load the song
    int clips = 0;      // should always be
    int chancnt = 0;
    int row = 0;
    for (int idx=0; idx<200; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (idx < 100) {
            sprintf(testFile, szFilename, "ton", idx, hz);
            isNoise[chancnt] = false;
            oldIdx[chancnt] = idx;
        } else {
            // we're done if noise is not to be processed
            if (nonoise) break;
            // Technically, the SN should not require any reduction
            // here, but the way we have structured this toolchain, we
            // can't make that assumption here. If you know better, you
            // can always set the limit to MORE than 256 tones. ;)
            // Or specify nonoise.
            sprintf(testFile, szFilename, "noi", idx-100, hz);
            isNoise[chancnt] = true;
            oldIdx[chancnt] = idx-100;
        }

        FILE *fp = fopen(testFile, "r");
        if (NULL == fp) continue;       // in fact, most will fail

        if (verbose) printf("Opened channel %d: %s\n", idx%100, testFile);

        // read until the channel ends
        int oldrow = row;
        row = 0;
        bool cont = true;
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
            if (2 != sscanf(buf, "0x%X,0x%X", &VGMDAT[chancnt][row], &VGMVOL[chancnt][row])) {
                if (2 != sscanf(buf, "%d,%d", &VGMDAT[chancnt][row], &VGMVOL[chancnt][row])) {
                    printf("Failed to parse line %d for channel %d\n", row+1, idx%100);
                    cont = false;
                    break;
                }
            }
            if ((VGMDAT[chancnt][row]&TONEMASK) > 0xfff) {
                ++clips;
                VGMDAT[chancnt][row] = 0xfff | (VGMDAT[chancnt][row]&NOISEFLAGS);
            }
            if (cont) {
                ++row;
                if (row >= MAXTICKS) {
                    printf("Maximum song length reached, truncating.\n");
                    break;
                }
            }
        }

        fclose(fp);

        if ((oldrow != 0) && (row != oldrow)) {
            printf("Inconsistent row length (%d vs %d) found. Please correct.\n", oldrow, row);
            return -1;
        }

        ++chancnt;
        if (chancnt >= MAXCHANNELS) {
            printf("Too many channels!\n");
            return -1;
        }
    }
    if (chancnt < 1) {
        printf("No channels found!\n");
        return -1;
    }

    if (clips > 0) {
        printf("Warning: %d notes clipped (>0xfff)\n", clips);
    }

    // okay, now the whole song is loaded. chancnt has
    // the number of loaded channels, and isNoise[] tracks if
    // they are noise or not for the save code. row has the
    // number of rows (verified to match), and oldIdx has the
    // file index.

    // this loop renames all the old files
    for (int idx = 0; idx<chancnt; ++idx) {
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (isNoise[idx]) {
            sprintf(testFile, szFilename, "noi", oldIdx[idx], hz);
        } else {
            sprintf(testFile, szFilename, "ton", oldIdx[idx], hz);
        }

        char buf[1100];
        sprintf(buf, "%s" RENAME, testFile);
        if (rename(testFile, buf)) {
            // this is kind of bad - we might be partially through converting the song?
            printf("Error renaming '%s' to '%s', code %d\n", testFile, buf, errno);
            return 1;
        }
    }

    // now do the reduction - each loop we re-count the number of frequencies
    int nTotalFreqs = noteTarget+1;
    int FreqCompress = 1;   // how much difference we ignore
    int converted = 0;

    while (nTotalFreqs > noteTarget) {
		if (verbose) printf("Counting number of frequencies...");
		nTotalFreqs=0;
		for (int idx=0; idx<row; ++idx) {
			for (int chan=0; chan<chancnt; ++chan) {
				int cnt;
				for (cnt=0; cnt<nTotalFreqs; cnt++) {
					if ((VGMDAT[chan][idx]&TONEMASK) == Freqs[cnt]) break;
				}
				if (cnt >= nTotalFreqs) {
					Freqs[nTotalFreqs++] = (VGMDAT[chan][idx]&TONEMASK);
					if (nTotalFreqs >= 0xfff) {
                        // this should be impossible since we clipped on load...
						printf("\rToo many frequencies to process tune (>4096!)\n");
						return -1;
					}
				}
			}
		}
		if (verbose) printf("%d frequencies used (want %d).\n", nTotalFreqs, noteTarget);
    
        if (nTotalFreqs > noteTarget) {
		    // do note removal by popularity instead of by stepping slides
		    // actual musical notes get their popularity tripled automatically
		    // this seems to work better, although I worry it could completely destroy 
		    // sides in a song with just one of them... much better on Monty though!
		    int pop[4096];		// only 4096 possible frequencies here
		    memset(pop, 0, sizeof(pop));

		    ++FreqCompress;

			for (int chan=0; chan<chancnt; ++chan) {
				for (int idx=0; idx<row; ++idx) {
					int freq = (VGMDAT[chan][idx]&TONEMASK);
					++pop[freq];
				}
			}

		    // tripling popularity of real notes
		    for (int i2=0; i2<sizeof(musicalnotes)/sizeof(musicalnotes[0]); i2++) {
                for (int i3=i2-noteTol; i3<=i2+noteTol; ++i3) {
                    if ((i3 >=0) && (i3 < 4096)) {
        			    pop[i3]*=3;
                    }
                }
		    }

		    // go through the song and replace all the weakest notes
		    int cnt = 0;

            // if the user wants to leave noise alone, there's a flag for that, handled at load.
			for (int chan=0; chan<chancnt; ++chan) {
				for (int idx=0; idx<row; idx++) {
                    if ((VGMDAT[chan][idx]&TONEMASK) == 0) continue;
					if (pop[(VGMDAT[chan][idx]&TONEMASK)] < FreqCompress) {
						// make it a rounded multiple of step (this sometimes reintroduces notes previously removed...)
                        int noiseflags = (VGMDAT[chan][idx]&NOISEFLAGS);
                        int note = (VGMDAT[chan][idx]&TONEMASK);
						int replace = ((note + (FreqCompress/2)) / FreqCompress) * FreqCompress + noiseflags;
						VGMDAT[chan][idx] = replace;
						++cnt;
					}
				}
			}
		    if (verbose) printf("-Popularity adjusted %d ticks at step %d\n", cnt, FreqCompress);
            converted += cnt;
        }
    }

    if ((verbose)&&(clips>0)) printf("%d notes clipped\n", clips);
    if ((verbose)&&(converted>0)) printf("%d/%d notes adjusted\n", converted, chancnt*row);
    if (verbose) printf("%d frequencies in final.\n", nTotalFreqs);

    for (int idx=0; idx<chancnt; ++idx) {
        // now we just have to spit the data back out to new files
        char testFile[1024];

        // try 100 ton files, then 100 noi files ;)
        if (isNoise[idx]) {
            sprintf(testFile, szFilename, "noi", oldIdx[idx], hz);
        } else {
            sprintf(testFile, szFilename, "ton", oldIdx[idx], hz);
        }

        FILE *fout = fopen(testFile, "w");
        if (NULL == fout) {
            printf("Failed to open output file '%s', err %d\n", testFile, errno);
            return 1;
        }
        printf("Writing: %s\n", testFile);
        for (int idx2=0; idx2<row; ++idx2) {
            fprintf(fout, "0x%08X,0x%02X\n", VGMDAT[idx][idx2], VGMVOL[idx][idx2]);
        }
        fclose(fout);
    }
    
    printf("** DONE **\n");
    
    return 0;
}


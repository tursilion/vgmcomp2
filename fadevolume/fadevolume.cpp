// fadeevolume.cpp : Defines the entry point for the console application.
// Fades out a channel over a set number of rows.

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 1
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool verbose = false;
FILE *fp[MAXCHANNELS];
const char *szFilename[MAXCHANNELS];

#define RENAME ".fadevolume.old"

int main(int argc, char *argv[])
{
	printf("VGMComp2 Song Volume Fading Tool - v20260611\n\n");

    if (argc < 2) {
        printf("fadevolume [-q] [-lines <n>] <channel>\n");
        printf("Maximizes the volume of a channel.\n");
        printf("-v - verbose output\n");
        printf("-lines <n> - sets the number of lines to fade out over (default 30)\n");
        printf("song - pass in the name of one channel.\n");
        printf("Original files are renamed to " RENAME "\n");
        return 1;
    }

    // check arguments
    int nLines = 30;
    int arg = 1;
    while ((arg < argc) && (argv[arg][0] == '-')) {
        if (strcmp(argv[arg], "-v") == 0) {
            verbose = true;
        } else if (strcmp(argv[arg], "-lines") == 0) {
            if (arg+1>=argc) {
                printf("Not enough arguments for lines\n");
                return 1;
            }
            ++arg;
            nLines = atoi(argv[arg]);
            if (nLines < 1) {
                printf("Invalid line count.\n");
                return 1;
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

    // open input file(s)
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

    if (nLines > row) {
        printf("* WARNING: Requested %d lines, but song only has %d. Trimming value.\n", nLines, row);
        nLines = row;
    }

    printf("Imported %d rows, fading last %d lines\n", row, nLines);
    
    // fix all the volumes
    int nStartRow = row-nLines;
    if (verbose) {
        printf("Start row %d, rows to fade %d\n", nStartRow, nLines);
    }
    for (int i2=0; i2<row; ++i2) {
        if (i2 < nStartRow) continue;
        double scale = 1.0-((double)(i2-nStartRow)/nLines);
        VGMVOL[0][i2] = (int)(VGMVOL[0][i2]*scale);
        if (verbose) {
            printf("%3d - 0x%08X,0x%02X (%lf)\n", i2, VGMDAT[0][i2], VGMVOL[0][i2], scale);
        }
    }
    
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


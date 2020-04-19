// vgmcomp2.cpp : Defines the entry point for the console application.
//
// 9 Channels:
// Tone1	Vol1	Tone2	Vol2	Tone3	Vol3	Noise	Vol4	Timing
//
// Tone   - byte index into note table
// Vol    - high nibble - frames till next byte, low nibble - volume
// Noise  - high nibble - trigger flag (0x80), low nibble - noise type
// Timing - high nibble - frames till next byte, low nibble - channels to update (1234)
//
// AY notes:
// Noise channel has 5 bits of type, high bit is still trigger (not sure it's needed)
// Vol4 is now a mixer command. REVERSED from TI - high nibble is noise enable CBA, low nibble is count
//      This means xCBA#### - the mixer needs 00CBA000 - so shift right once then AND #$38
//      This should be faster than left shifting twice from the low nibble.

#include "stdafx.h"
#include <stdio.h>
#include <string.h>

#define NOISE_MASK     0x003FF              // actual PSG limit here, not 0xFFF like the convert tools
#define NOISE_TRIGGER  0x10000
#define NOISE_OUT_TRIGGER 0x80

// stream names - don't reorder - TONE1-3 and NOISE must be 0-3
enum {
    TONE1,
    TONE2,
    TONE3,
    NOISE,
    VOL1,
    VOL2,
    VOL3,
    VOL4,
    TIMESTREAM,

    MAXSTREAMS
};

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 4                       // channels 0-2 are tone, and 3 is noise
int VGMDAT2[MAXCHANNELS][MAXTICKS];         // used for testing
int VGMVOL2[MAXCHANNELS][MAXTICKS];

#define MAXSONGS 16                         // kind of arbitrary
struct SONG {
    // raw tick data from the file
    int VGMDAT[MAXCHANNELS][MAXTICKS];
    int VGMVOL[MAXCHANNELS][MAXTICKS];

    // compressed output data
    int outStream[MAXSTREAMS][MAXTICKS];        // storage for the output streams
    int streamCnt[MAXSTREAMS];                  // count for each stream

    // informational data
    int lastnote[MAXSTREAMS];                   // used to check for RLE
    int outCnt;
} songs[MAXSONGS];

#define MAXNOTES 256
int noteCnt;
int noteTable[MAXNOTES];                    // maximum 256 notes in note table

// song counts
int currentSong = 0;

// options
char szFileOut[256]={0};
bool isPSG=false, isAY=false;
bool verbose = false;
bool debug = false;

// pass in file pointer (will be reset), channel index, last row count, first input column (from 0), and true if noise
// set scalevol to true to scale PSG volumes back to 8-bit linear
bool loadData(FILE *fp, int &chan, SONG *s, int column, bool noise) {
    // up to 8 columns allowed
    int in[8];
    int cnt = 0;

    // back to start of file
    fseek(fp, 0, SEEK_SET);

    cnt = 0;
    while (!feof(fp)) {
        char b2[256];
        if (fgets(b2, sizeof(b2), fp)) {
            int cols;
            b2[sizeof(b2)-1]='\0';
            cols = sscanf_s(b2, "0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X,0x%X", &in[0], &in[1], &in[2], &in[3], &in[4], &in[5], &in[6], &in[7]);
            if (cols == 0) {
                // try decimal
                cols = sscanf_s(b2, "%d,%d,%d,%d,%d,%d,%d,%d", &in[0], &in[1], &in[2], &in[3], &in[4], &in[5], &in[6], &in[7]);
            }
            if (cols < column+2) {
                printf("Error parsing line %d\n", cnt+1);
                return false;
            }
            s->VGMDAT[chan][cnt] = in[column];
            s->VGMVOL[chan][cnt] = in[column+1];

            if (noise) {
                if (s->VGMDAT[chan][cnt] & NOISE_TRIGGER) {
                    s->VGMDAT[chan][cnt] &= NOISE_MASK|NOISE_OUT_TRIGGER;    // limit input
                } else {
                    s->VGMDAT[chan][cnt] &= NOISE_MASK;    // limit input
                }
                s->VGMVOL[chan][cnt] &= 0xff;
            } else {
                s->VGMDAT[chan][cnt] &= NOISE_MASK;    // limit input
                s->VGMVOL[chan][cnt] &= 0xff;
            }
            ++cnt;
            if (cnt >= MAXTICKS) {
                printf("(maximum tick count reached) ");
                break;
            }
        }
    }
    ++chan;

    if ((s->outCnt > 0) && (s->outCnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", s->outCnt, cnt);
    }
    s->outCnt = cnt;

    return true;
}

bool testOutputRLE(SONG *s) {
    // output outStream[][] and streamCnt[] into a VGMDAT2[][] amd VGMVOL2[][]
    // so that we can make sure it packed correctly
    int incnt[MAXSTREAMS];
    int testCnt[MAXSTREAMS];    // we can do tones and volumes separately anyway
    bool ret = true;

    // prepare the arrays
    for (int idx=0; idx<MAXSTREAMS; ++idx) {
        incnt[idx]=0;
        testCnt[idx]=0;
    }

    // load the default data
    for (int idx=TONE1; idx<=NOISE; ++idx) {
        VGMDAT2[idx][testCnt[idx]] = 1;
        VGMVOL2[idx][testCnt[idx]] = 0xf;
    }

    // unpack the count channels
    int timecnt = 0;

    for (;;) {
        if (--timecnt < 0) {
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                if (s->outStream[TIMESTREAM][incnt[TIMESTREAM]] & (0x80>>idx)) {
                    VGMDAT2[idx][testCnt[idx]++] = s->outStream[idx][incnt[idx]++];
                } else {
                    if (testCnt[idx] > 0) {
                        // this test not normally needed, since a real tool would
                        // just remember what was currently set. It's only because
                        // I'm storing every frame into an array for later compare
                        VGMDAT2[idx][testCnt[idx]] = VGMDAT2[idx][testCnt[idx]-1];
                    }
                    testCnt[idx]++;
                }
            }

            timecnt = s->outStream[TIMESTREAM][incnt[TIMESTREAM]]&0x0f;
            
            if (debug) {
                printf("-- 0x%02X\n", s->outStream[TIMESTREAM][incnt[TIMESTREAM]]);
            }

            if ((s->outStream[TIMESTREAM][incnt[TIMESTREAM]]==0) && (s->outStream[TIMESTREAM][incnt[TIMESTREAM]+1]==0)) {
                break;
            }

            incnt[TIMESTREAM]++;
        } else {
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                VGMDAT2[idx][testCnt[idx]] = VGMDAT2[idx][testCnt[idx]-1];
                testCnt[idx]++;
            }
        }

        // make sure the rows are staying in sync
        if ((testCnt[0]!=testCnt[1])||(testCnt[2]!=testCnt[3])||(testCnt[0]!=testCnt[3])) {
            printf("VERIFY PHASE 1 - failed unpacking - rows out of sync: %d %d %d %d\n",
                   testCnt[0], testCnt[1], testCnt[2], testCnt[3]);
            return false;
        }

        if (debug) {
            // debug current row
            int lastrow = testCnt[0]-1;
            printf("%04d(%02d): ", lastrow, timecnt);
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                printf("0x%05X   ", VGMDAT2[idx][lastrow]);
            }
            printf("===   ");
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                printf("0x%05X   ", s->VGMDAT[idx][lastrow]);
            }
            printf("\n");
        }
    }

    // unpack the volume channels - volume is command plus hold time (0xVT)
    bool cont = true;
    int volcnt[4] = {0,0,0,0};
    while (cont) {
        for (int idx=VOL1; idx<=VOL4; ++idx) {
            if (--volcnt[idx-VOL1] < 0) {
                volcnt[idx-VOL1] = s->outStream[idx][incnt[idx]++];
                VGMVOL2[idx-VOL1][testCnt[idx]++] = (volcnt[idx-VOL1]>>4)&0xf;
                volcnt[idx-VOL1] &= 0x0f;

                // check next row and this row for 0's, marking the end
                if (idx == VOL4) {
                    if ((s->outStream[idx][incnt[idx]] == 0) && (s->outStream[idx][incnt[idx]-1] == 0)) {
                        cont=false;
                        break;
                    }
                }
            } else {
                VGMVOL2[idx-VOL1][testCnt[idx]] = VGMVOL2[idx-VOL1][testCnt[idx]-1];
                testCnt[idx]++;
            }
        }

        if (debug) {
            // debug current row
            int lastrow = testCnt[VOL1]-1;
            printf("%04d: ", lastrow);
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("0x%02X%c ", VGMVOL2[idx-VOL1][lastrow], volcnt[idx-VOL1]==0?'<':' ');
            }
            printf("===   ");
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("0x%02X  ", s->VGMVOL[idx-VOL1][lastrow]);
            }
            printf("===   ");
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("%02d  ", volcnt[idx-VOL1]);
            }
            printf("\n");
        }
    }

    // and test it - VERIFY PHASE 1 means this whole RLE test
    for (int idx=0; idx<TIMESTREAM; ++idx) {
        if (testCnt[idx] != s->outCnt+1) {   // +1 for the ending beats.. I hope ;)
            printf("VERIFY PHASE 1 - Failed on song length - got %d rows on %s %d, song was %d rows\n", 
                   testCnt[idx], idx<VOL1?"channel":"volume", idx, s->outCnt);
            ret = false;
        }
    }
    for (int idx=0; idx<s->outCnt; ++idx) {
        for (int idx2=0; idx2<MAXCHANNELS; ++idx2) {
            if (s->VGMDAT[idx2][idx] != VGMDAT2[idx2][idx]) {
                printf("VERIFY PHASE 1 - failed on song data - channel %d, row %d, got 0x%X vs desired 0x%X (aborting check)\n",
                       idx2, idx, VGMDAT2[idx2][idx], s->VGMDAT[idx2][idx]);
                idx=s->outCnt;
                ret = false;
                break;
            }
            if (s->VGMVOL[idx2][idx] != VGMVOL2[idx2][idx]) {
                printf("VERIFY PHASE 1 - failed on song volume - channel %d, row %d, got 0x%X vs desired 0x%X (aborting check)\n",
                       idx2, idx, VGMVOL2[idx2][idx], s->VGMVOL[idx2][idx]);
                idx=s->outCnt;
                ret = false;
                break;
            }
        }
    }

    return ret;
}

// the first pass RLE stage
bool initialRLE(SONG *s) {
    // build a new set of output streams for the 3 tones and noise, plus timestream channels
    // (noise handling here is different on the AY)
    // Basically, we need to RLE all four channels at the same time
    // Output goes into outStream[]
    
    // set up first block defaults
    s->streamCnt[TIMESTREAM]=0;
    s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]=0xf0;  // start with all channels changing

    // tones and noise - first note
    for (int idx=TONE1; idx<=NOISE; ++idx) {
        s->streamCnt[idx]=0;
        s->lastnote[idx]=s->VGMDAT[idx][0];
        s->outStream[idx][s->streamCnt[idx]++] = s->lastnote[idx];
    }

    // now RLE the rest (tones and noise only)
    // TODO: we need to break this up at outCnt[song] so that each
    // song has a distinct starting point. Then update outCnt[song]
    for (int idx=0; idx<s->outCnt; ++idx) {
        int diff=0;
        // check each tone/noise channel
        for (int idx2=TONE1; idx2<=NOISE; ++idx2) {
            if (s->lastnote[idx2] != s->VGMDAT[idx2][idx]) {
                diff|=0x80>>idx2; 
                s->lastnote[idx2] = s->VGMDAT[idx2][idx];   
                s->outStream[idx2][s->streamCnt[idx2]++] = s->lastnote[idx2];    // saving off the new note

                // don't remember the trigger flag on the noise channel, so it matches non-triggers
                if (idx == 3) {
                    s->lastnote[idx2] &= ~NOISE_OUT_TRIGGER;
                }
            }
        }

        if (diff != 0) {
            // don't decrement the timestream count if we didn't count any at all - handles
            // the case of timeout plus change on the same frame
            if (s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]] > 0) {
                // decrement the old timestream count
                s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]--;
                s->streamCnt[TIMESTREAM]++;
            }
            // init the new timestream count
            s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]=diff+1;    // include this row
        } else {
            // we're still counting - this timeout costs 4 bytes per second on no changes
            s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]++;
            if ((s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]&0x0f) == 0) {
                // wraparound, so decrement
                s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]--;
                // maximum count, so move to the next one
                s->streamCnt[TIMESTREAM]++;
                s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]=0;     // no channel changes, this row already counted
            }
        }
    }

    // reduce the final count by 1, since we didn't get a chance above (must do this before the nibble tweaking)
    // if it was zero (only possible in the maximum size case), then we can delete the entry entirely
    if ((s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]&0xf) == 0) {
        s->streamCnt[TIMESTREAM]--;
    } else {
        s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]--;
    }

    // at the end, re-set the first timestream command nibble to remove unused channels
    if (s->streamCnt[TONE1] == 0) s->outStream[TIMESTREAM][0] &= 0x7f;
    if (s->streamCnt[TONE2] == 0) s->outStream[TIMESTREAM][0] &= 0xbf;
    if (s->streamCnt[TONE3] == 0) s->outStream[TIMESTREAM][0] &= 0xdf;
    if (s->streamCnt[NOISE] == 0) s->outStream[TIMESTREAM][0] &= 0xef;

    // dual-zero the timestream (there might already be one at the end, but unlikely)
    s->streamCnt[TIMESTREAM]++;
    s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]=0;
    s->streamCnt[TIMESTREAM]++;
    s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]=0;

    // Individually RLE each volume channel (not included in the streams above)
    for (int idx=VOL1; idx<=VOL4; ++idx) {
        s->streamCnt[idx]=0;
        s->lastnote[idx]=s->VGMVOL[idx-VOL1][0];
        s->outStream[idx][s->streamCnt[idx]]=0;
    }
    for (int idx=0; idx<s->outCnt; ++idx) {
        // check each volume
        for (int idx2=VOL1; idx2<=VOL4; ++idx2) {
            if (s->lastnote[idx2] != s->VGMVOL[idx2-VOL1][idx]) {
                // check for special case - we wrapped and counted out at the same time,
                // so a new entry is not actually needed, just reuse it
                if (s->outStream[idx2][s->streamCnt[idx2]] > 0) {
                    // decrement the count and add the note
                    s->outStream[idx2][s->streamCnt[idx2]]--;
                    s->outStream[idx2][s->streamCnt[idx2]] |= (s->lastnote[idx2]<<4)&0xf0;
                    s->streamCnt[idx2]++;
                }
                s->outStream[idx2][s->streamCnt[idx2]]=1;         // include this row
                s->lastnote[idx2] = s->VGMVOL[idx2-VOL1][idx];
            } else {
                // we're still counting
                s->outStream[idx2][s->streamCnt[idx2]]++;
                if ((s->outStream[idx2][s->streamCnt[idx2]]&0xf) == 0) {
                    // maximum count, so move to the next one
                    s->outStream[idx2][s->streamCnt[idx2]]--;
                    s->outStream[idx2][s->streamCnt[idx2]] |= (s->lastnote[idx2]<<4)&0xf0;
                    s->streamCnt[idx2]++;
                    s->outStream[idx2][s->streamCnt[idx2]]=0;     // already counted
                }
            }
        }
    }

    // reduce the final counts by 1, since we didn't get a chance above, also insert the data value
    // if it was zero (only possible in the maximum size case), then we can delete the entry entirely
    for (int idx2=VOL1; idx2<=VOL4; ++idx2) {
        if ((s->outStream[idx2][s->streamCnt[idx2]]&0xf) == 0) {
            s->streamCnt[idx2]--;
        } else {
            s->outStream[idx2][s->streamCnt[idx2]]--;
            s->outStream[idx2][s->streamCnt[idx2]] |= (s->lastnote[idx2]<<4)&0xf0;
        }
    }

    // I don't think it matters, but double-zero these too
    for (int idx2=VOL1; idx2<=VOL4; ++idx2) {
        s->streamCnt[idx2]++;
        s->outStream[idx2][s->streamCnt[idx2]]=0;
        s->streamCnt[idx2]++;
        s->outStream[idx2][s->streamCnt[idx2]]=0;
    }

    return true;
}

int main(int argc, char *argv[])
{
    // parse arguments
    int nextarg = -1;
    for (int idx=1; idx<argc; ++idx) {
        if (argv[idx][0]=='-') {
            // parse switch
            if (argv[idx][1] == 'h') {
                // cover 'help' by not loading the filename arg
                break;
            } else if (argv[idx][1] == 'd') {
                debug = true;
            } else if (argv[idx][1] == 'v') {
                verbose= true;
            } else if (0 == strcmp(argv[idx], "-psg")) {
                isPSG=true;
                if (isAY) {
                    printf("Can't select both PSG and AY options.\n");
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-ay")) {
                isAY=true;
                if (isPSG) {
                    printf("Can't select both PSG and AY options.\n");
                    return 1;
                }
            } else {
                printf("Unknown switch '%s'\n", argv[idx]);
            }
        } else {
            if (strlen(szFileOut) == 0) {
                strcpy(szFileOut, argv[idx]);
                nextarg = idx+1;
                break;
            }
        }
    }
    if ((strlen(szFileOut)==0)||((isAY==false)&&(isPSG==false))) {
        printf("vgmcomp2 [-d] [-v] <-ay|-psg> <filenameout.psg> <filenamein1.psg> [<filenamein2.psg>...]\n");
        printf("Provides a compressed (sound compressed format) file from\n");
        printf("an input file containing either PSG or AY-3-8910 data\n");
        printf("Except for the noise handling, the output is the same.\n");
        printf("Specify either -ay for the AY-3-8910 data or -psg for PSG data\n");
        printf("Then input file and output filename.\n");
        printf("-d - add extra parser debug output\n");
        printf("-v - add extra verbose information\n");
        return 1;
    }

    // loop through all the songs
    int totalC = 0;
    for (int fileIdx = nextarg; fileIdx < argc; ++fileIdx) {
        // load data file through loadData(FILE *fp, int &chan, int &cnt, int column, bool noise)
        FILE *fp = fopen(argv[fileIdx], "r");
        if (NULL == fp) {
            printf("Failed to open input file '%s'\n", argv[fileIdx]);
            return 2;
        }
        int chan = 0;
        for (int idx=TONE1; idx<=NOISE; ++idx) {
            if (!loadData(fp, chan, &songs[currentSong], idx*2, idx==NOISE)) {
                printf("Failed reading channel %d from '%s'\n", idx, argv[fileIdx]);
                return 1;
            }
        }
        fclose(fp);
        if (verbose) printf("Read %d lines from '%s'\n", songs[currentSong].outCnt, argv[fileIdx]);
        totalC += songs[currentSong].outCnt;
        ++currentSong;
    }
    if (totalC == 0) {
        printf("No input files specified, nothing to do.\n");
        return 1;
    }
    if (verbose) printf("Total of %d rows from %d songs\n", totalC, currentSong);

    // at this point the 4 tone channels are in VGMDAT[chan][rows]
    // and the 4 volume channels are in VGMVOL[chan][rows].

    // build a 256 entry note table in noteTable[], and convert the notes to it
    // the first note needs to be 001, for consistency. We use that for mutes alot.
    // Not sure if the wasted entry will bite us long term, but we can change it later.
    noteTable[0]=1;
    noteCnt = 1;
    int oldNoteCnt = 0;
    for (int thisSong = 0; thisSong < currentSong; ++thisSong) {
        for (int idx=0; idx<songs[thisSong].outCnt; ++idx) {
            for (int chan=TONE1; chan<=TONE3; ++chan) {   // we don't need to scan the noise channel, it needs no reduction
                int note = songs[thisSong].VGMDAT[chan][idx];
                int match = -1;
                for (int idx2=0; idx2<noteCnt; ++idx2) {
                    if (note == noteTable[idx2]) {
                        match = idx2;
                        break;
                    }
                }
                if (match == -1) {
                    if (noteCnt >= MAXNOTES) {
                        printf("Too many notes in song, try reducing count%s\n",
                               currentSong>1 ? " or try stacking fewer songs.":".");
                        return 1;
                    }
                    noteTable[noteCnt] = note;
                    match = noteCnt++;
                }
                songs[thisSong].VGMDAT[chan][idx] = match;
            }
        }
        if (currentSong > 1) {
            if (verbose) printf("Song %d adds %d notes to note table\n", thisSong, noteCnt - oldNoteCnt);
        }
        oldNoteCnt = noteCnt;
    }
    if (verbose) printf("Songbank contains %d/%d notes.\n", noteCnt, MAXNOTES);

    // TODO: why are we pre-RLE-ing the data again? don't we have stronger RLE in the main pass?
    // try with and without this step
#if 1
    for (int idx=0; idx<currentSong; ++idx) {
        if (verbose) printf("RLE Packing song %d (%d rows)...\n", idx, songs[idx].outCnt);
        if (!initialRLE(&songs[idx])) {
            return 1;
        }
        if (verbose) {
            printf("  - Tone 1 to %d rows\n", songs[idx].streamCnt[TONE1]);
            printf("  - Tone 2 to %d rows\n", songs[idx].streamCnt[TONE2]);
            printf("  - Tone 3 to %d rows\n", songs[idx].streamCnt[TONE3]);
            printf("  - Noise  to %d rows\n", songs[idx].streamCnt[NOISE]);
            printf("  - Vol  1 to %d rows\n", songs[idx].streamCnt[VOL1]);
            printf("  - Vol  2 to %d rows\n", songs[idx].streamCnt[VOL2]);
            printf("  - Vol  3 to %d rows\n", songs[idx].streamCnt[VOL3]);
            printf("  - Vol  4 to %d rows\n", songs[idx].streamCnt[VOL4]);
            printf("  - TimeSt to %d rows\n", songs[idx].streamCnt[TIMESTREAM]);
            printf("  = %d bytes to %d bytes\n", songs[idx].outCnt*(2+2+2+1+1+1+1+1), 
                   songs[idx].streamCnt[TONE1]+songs[idx].streamCnt[TONE2]+songs[idx].streamCnt[TONE3]+
                   songs[idx].streamCnt[NOISE]+songs[idx].streamCnt[VOL1]+songs[idx].streamCnt[VOL2]+
                   songs[idx].streamCnt[VOL3]+songs[idx].streamCnt[VOL4]+songs[idx].streamCnt[TIMESTREAM]);
        }
    }
    // test the RLE process
    for (int idx=0; idx<currentSong; ++idx) {
        if (verbose) printf("RLE Testing song %d...\n", idx);
        if (!testOutputRLE(&songs[idx])) {
            return 1;
        }
    }
#endif

    // prepare the output binary blob

    // Individually compress each of the 9 channels (all treated the same)

    

    return 0;
}


// vgmcomp2.cpp : Defines the entry point for the console application.
//
// 9 Channels:
// Tone1	Vol1	Tone2	Vol2	Tone3	Vol3	Noise	Vol4	Timing
//
// Tone   - byte index into note table
// Vol    - high nibble - frames till next byte, low nibble - volume
// Noise  - high nibble - trigger flag (0x80), low nibble - noise type (5 bits for AY)
// Timing - high nibble - frames till next byte, low nibble - channels to update (1234)
//
// AY notes:
// Noise channel has 5 bits of type, high bit is still trigger (not sure it's needed)
// Vol4 is now a mixer command. I had considered mixing it up, but it only requires 
// 1 extra SLA on the Z80 (2 cycles) and that's worth avoiding the confusion. So the 
// data is the same - high nibble is count, low nibble is mixer command, partially:
// - This means ####CBA0 - the mixer needs 00CBA000 - so shift left twice then AND #$38
//
// SID notes:
// Noise and tone channels are identical, the player needs to know which mode to set.
// This also means channels can be waves other than square, as it's outside the scope
// of this packing engine. There is a full 16-bits of frequency counter, but the
// rates are incompatible with the AY and SN. Channel 3 (normally noise) is expected to
// be empty but not currently enforced.

// TODO: allow the command line to specify input type before every PSG file so
// that multiple chip types can be mixed.

// TODO: investigate whether the SID can be forced to share an end-stream command from
// another channel to save a couple of bytes. I think the volume encoding mechanism may
// cause problems with this (ie: volume streams never end). Currently it eats 24 bytes
// (5 for the single row of noise and 19 for the volume commands) in afterburner (84s),
// so we can estimate about 10 bytes per minute plus some change, if the rest of
// the file can't provide a nice stream of 0x0f bytes for the mutes. ;)
// Of course, the truly desperate could just hack those streams out...

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOISE_TRIGGER  0x10000              // input version of noise trigger bit
#define NOISE_OUT_TRIGGER 0x80              // output version of noise trigger bit

// this is used by enabling RUNBACKREFTEST
// and passing the -dd switch
int tstcase[] = {
    1,2,3,4,5,
    1,2,3,4,5,
    1,2,3,4,5,
    1,2,3,4,5,
    1,2,3,4,5
};
//#define RUNBACKREFTEST

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

// this are the bitmasks for encoding
const int BACKREFBITS = 0xc0;   // 11x
const int BACKREFBITS2= 0xe0;   // 11x
const int RLE16BITS   = 0x80;   // 100
const int RLE24BITS   = 0xA0;   // 101
const int RLEBITS     = 0x40;   // 010
const int RLE32BITS   = 0x60;   // 011
const int INLINEBITS  = 0x00;   // 00x
const int INLINEBITS2 = 0x20;   // 00x

// some basic settings
#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 4                       // channels 0-2 are tone, and 3 is noise
#define MAXSONGS 16                         // kind of arbitrary

struct SONG {
    // note: the VGMDAT, VGMVOL, and outStream must all be the SAME base type (int today)

    // raw tick data from the file
    int VGMDAT[MAXCHANNELS][MAXTICKS];
    int VGMVOL[MAXCHANNELS][MAXTICKS];

    // RLE compressed data
    int outStream[MAXSTREAMS][MAXTICKS];        // storage for the output streams
    int streamCnt[MAXSTREAMS];                  // count for each stream

    // informational data
    int lastnote[MAXSTREAMS];                   // used to check for RLE
    int outCnt;

    // stream data
    int streamOffset[MAXSTREAMS];               // where in the binary blob does each stream start
} songs[MAXSONGS], testSong;                    // testSong is used for the unpack testing

#define MAXNOTES 256
int noteCnt;
int noteTable[MAXNOTES];                    // maximum 256 notes in note table

#define STREAMTABLE 0                       // offset to the pointer to the stream pointer table
#define NOTETABLE 2                         // offset to the pointer to the note table
unsigned char outputBuffer[64*1024];        // format max size is 64k
int outputPos = 0;

// song counts
int currentSong = 0;

// options
char szFileOut[256]={0};
bool isPSG=false, isAY=false, isSID=false;
bool verbose = false;
bool debug = false;
bool debug2 = false;
bool doDeepDive = false;
int noiseMask, toneMask;
int minRun = 1;
int minRunMin = 1;
int minRunMax = 8;
bool noRLE = false;
bool noRLE16 = false;
bool noRLE24 = false;
bool noRLE32 = false;
bool noFwdSearch = false;
bool noBwdSearch = false;
bool alwaysRLE = false;
bool checkAnyway = false;

// stats for minrun loops
#define MAXRUN 21
struct OneVal {
    int cnt;
    int total;

    int avg(int add) {
        if (cnt == 0) return 0;
        // add is used to give the offset each type has
        return total / cnt + add;
    }
    struct OneVal& operator+= (const struct OneVal &b) {
        cnt += b.cnt;
        total += b.total;
        return *this;
    }
    void reset() {
        cnt=0;
        total=0;
    }
};
struct {
    struct OneVal cntRLEs;
    struct OneVal cntRLE16s;
    struct OneVal cntRLE24s;
    struct OneVal cntRLE32s;
    struct OneVal cntBacks;
    struct OneVal cntInlines;
    struct OneVal cntFwds;
} minRunData[MAXRUN+1];
unsigned char bestRunBuffer[65536];   // this is a little wasteful, but that's okay

// byte swap for the unpack test
inline short byteswap(short x) {
    return ((x&0xff)<<8) | ((x&0xff00)>>8);
}

// pass in file pointer (will be reset), channel index, last row count, first input column (from 0), and true if noise
// set scalevol to true to scale PSG volumes back to 8-bit linear
bool loadDataCSV(FILE *fp, int &chan, SONG *s, int column, bool noise) {
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
                    s->VGMDAT[chan][cnt] &= noiseMask|NOISE_OUT_TRIGGER;    // limit input
                } else {
                    s->VGMDAT[chan][cnt] &= noiseMask;    // limit input
                }
                s->VGMVOL[chan][cnt] &= 0xff;
            } else {
                s->VGMDAT[chan][cnt] &= toneMask;    // limit input
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
    // output outStream[][] and streamCnt[] into testSong's VGMDAT[][] amd VGMVOL[][]
    // so that we can make sure it packed correctly
    // NOTE: this still processes for isSID, even though we won't use the last channel
    int incnt[MAXSTREAMS];
    int testCnt[MAXSTREAMS];    // we can do tones and volumes separately anyway
    bool ret = true;
    SONG *t = &testSong;
    memset(&testSong, 0, sizeof(testSong));

    // prepare the arrays
    for (int idx=0; idx<MAXSTREAMS; ++idx) {
        incnt[idx]=0;
        testCnt[idx]=0;
    }

    // load the default data
    for (int idx=TONE1; idx<=NOISE; ++idx) {
        t->VGMDAT[idx][testCnt[idx]] = 1;
        t->VGMVOL[idx][testCnt[idx]] = 0xf;
    }

    // unpack the count channels
    int timecnt = 0;

    for (;;) {
        if (--timecnt < 0) {
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                if (s->outStream[TIMESTREAM][incnt[TIMESTREAM]] & (0x80>>idx)) {
                    t->VGMDAT[idx][testCnt[idx]++] = s->outStream[idx][incnt[idx]++];
                } else {
                    if (testCnt[idx] > 0) {
                        // this test not normally needed, since a real tool would
                        // just remember what was currently set. It's only because
                        // I'm storing every frame into an array for later compare
                        t->VGMDAT[idx][testCnt[idx]] = t->VGMDAT[idx][testCnt[idx]-1];
                    }
                    testCnt[idx]++;
                }
            }

            timecnt = s->outStream[TIMESTREAM][incnt[TIMESTREAM]]&0x0f;
            
            if (debug) {
                printf("-- 0x%02X\n", s->outStream[TIMESTREAM][incnt[TIMESTREAM]]);
            }

            incnt[TIMESTREAM]++;
            
            // TODO: this is not a great way to end the RLE test either...
            if ((s->outStream[TIMESTREAM][incnt[TIMESTREAM]]==0) && (s->outStream[TIMESTREAM][incnt[TIMESTREAM]+1]==0)) {
                break;
            }
        } else {
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                t->VGMDAT[idx][testCnt[idx]] = t->VGMDAT[idx][testCnt[idx]-1];
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
                printf("0x%05X   ", t->VGMDAT[idx][lastrow]);
            }
            printf("===   ");
            for (int idx=TONE1; idx<=NOISE; ++idx) {
                printf("0x%05X   ", s->VGMDAT[idx][lastrow]);
            }
            printf("\n");
        }
    }

    // unpack the volume channels - volume is command plus hold time (0xVT)
    // end the volume rows by the channel counts, like the real thing does
    bool cont = true;
    int volcnt[4] = {0,0,0,0};
    while (cont) {
        if (debug) printf("%05d - ", testCnt[VOL1]);
        for (int idx=VOL1; idx<=VOL4; ++idx) {
            if (debug) printf("%X: ", idx);
            if (--volcnt[idx-VOL1] < 0) {
                if (debug) printf("%X/%2d ", (s->outStream[idx][incnt[idx]]>>4)&0xf, (s->outStream[idx][incnt[idx]]&0xf));
                volcnt[idx-VOL1] = s->outStream[idx][incnt[idx]++];
                t->VGMVOL[idx-VOL1][testCnt[idx]++] = (volcnt[idx-VOL1]>>4)&0xf;
                volcnt[idx-VOL1] &= 0x0f;

                // check if we're done only when VOL4 finishes
                // they SHOULD all line up, but we need to add that check...
                if (idx == VOL4) {
                    if (debug) printf("(%d%d%d%d) ",
                        (testCnt[VOL1] > testCnt[TONE1]),
                        (testCnt[VOL2] > testCnt[TONE2]),
                        (testCnt[VOL3] > testCnt[TONE3]),
                        (testCnt[VOL4] > testCnt[NOISE]));

                    // volume length is linked to voice length
                    if ((testCnt[VOL1] > testCnt[TONE1]) &&
                        (testCnt[VOL2] > testCnt[TONE2]) &&
                        (testCnt[VOL3] > testCnt[TONE3]) &&
                        (testCnt[VOL4] > testCnt[NOISE])) {
                        // decrement them all - exact length checked below
                        --testCnt[VOL1];
                        --testCnt[VOL2];
                        --testCnt[VOL3];
                        --testCnt[VOL4];
                        cont = false;
                        break;
                    }
                }
            } else {
                if (debug) printf(".... ");
                if ((debug)&&(idx == VOL4) ) printf("       ");
                t->VGMVOL[idx-VOL1][testCnt[idx]] = t->VGMVOL[idx-VOL1][testCnt[idx]-1];
                testCnt[idx]++;
            }
        }

        if (debug) {
            // debug current row
            int lastrow = testCnt[VOL1]-1;
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("0x%02X%c ", t->VGMVOL[idx-VOL1][lastrow], volcnt[idx-VOL1]==0?'<':' ');
            }
            printf("=== ");
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("0x%02X ", s->VGMVOL[idx-VOL1][lastrow]);
            }
            printf("=== ");
            for (int idx=VOL1; idx<=VOL4; ++idx) {
                printf("%02d ", volcnt[idx-VOL1]);
            }
            printf("\n");
        }
    }

    // and test it - VERIFY PHASE 1 means this whole RLE test
    for (int idx=0; idx<TIMESTREAM; ++idx) {
        if (testCnt[idx] != s->outCnt) {
            printf("VERIFY PHASE 1 - Failed on song length - got %d rows on %s %d, song was %d rows\n", 
                   testCnt[idx], idx<VOL1?"channel":"volume", idx, s->outCnt);
            ret = false;
        }
    }
    for (int idx=0; idx<s->outCnt; ++idx) {
        for (int idx2=0; idx2<MAXCHANNELS; ++idx2) {
            if (s->VGMDAT[idx2][idx] != t->VGMDAT[idx2][idx]) {
                printf("VERIFY PHASE 1 - failed on song data - channel %d, row %d, got 0x%X vs desired 0x%X (aborting check)\n",
                       idx2, idx, t->VGMDAT[idx2][idx], s->VGMDAT[idx2][idx]);
                idx=s->outCnt;
                ret = false;
                break;
            }
            if (s->VGMVOL[idx2][idx] != t->VGMVOL[idx2][idx]) {
                printf("VERIFY PHASE 1 - failed on song volume - channel %d, row %d, got 0x%X vs desired 0x%X (aborting check)\n",
                       idx2, idx, t->VGMVOL[idx2][idx], s->VGMVOL[idx2][idx]);
                idx=s->outCnt;
                ret = false;
                break;
            }
        }
    }

    if (ret == true) {
        printf("Got %d rows... ", testCnt[0]);
    }

    return ret;
}

bool testOutputSBF(SONG *s, int songidx) {
    // unpack outputBuffer into testSong's outStream[][] and streamCnt[]
    // so that we can make sure it packed correctly
    // TODO: this function does not have much error checking... for this
    // tool it's okay, but if you pull it out you need to check the buffer size.
    bool ret = true;
    SONG *t = &testSong;
    memset(&testSong, 0, sizeof(testSong));

    // first, look up each stream offset
    int noteOffset = outputBuffer[NOTETABLE]*256 + outputBuffer[NOTETABLE+1];
    unsigned short *noteTable = (unsigned short*)(&outputBuffer[noteOffset]); // remember endianess is swapped!

    // each stream is independent and can be unpacked separately
    for (int st=0; st<MAXSTREAMS; ++st) {
        if (debug) printf("Testing stream %d\n", st);
        int offset = (songidx*18) + (st*2) + outputBuffer[STREAMTABLE]*256 + outputBuffer[STREAMTABLE+1];
        offset = outputBuffer[offset]*256 + outputBuffer[offset+1];

        if (isSID) {
            if ((st==3)||(st==7)) {
                if (offset != 0) {
                    printf("VERIFY PHASE 2 - failed on stream %d, got %04X vs desired 0x0000\n",
                        st, offset);
                    ret = false;
                }
                continue;
            }
        }

        t->streamCnt[st] = 0;

        if (verbose) printf("SBF Testing song %d, stream %d at offset 0x%04x...\n", songidx, st, offset);

        bool keepgoing=true;

        while (keepgoing) {
            if (offset >= outputPos) {
                printf("* Parse error - offset of 0x%04X exceeds data size of 0x%04x\n", offset, outputPos);
                return false;
            }
            switch (outputBuffer[offset]&0xe0) {
                case BACKREFBITS2:
                case BACKREFBITS:   // min count is 4
                {
                    int cnt = (outputBuffer[offset++]&0x3f)+4;
                    int off = outputBuffer[offset]*256 + outputBuffer[offset+1];
                    offset+=2;
                    if (off == 0) {
                        if (debug) printf(" ENDSTREAM()");
                        // end of stream (bytes are already encoded?)
                        keepgoing = false;
                    } else {
                        if (debug) printf(" BACKREF(%d @ 0x%04X)", cnt, off);
                        while (cnt--) {
                            t->outStream[st][t->streamCnt[st]++] = outputBuffer[off++];
                        }
                    }
                }
                break;

                case RLE16BITS:     // min count is 2
                {
                    // cnt is thus 1 bit smaller
                    int cnt = (outputBuffer[offset++]&0x1f)+2;
                    int b1 = outputBuffer[offset++];
                    int b2 = outputBuffer[offset++];
                    if (debug) printf(" RLE16(%d x 0x%02X%02X)", cnt, b1,b2);
                    while (cnt--) {
                        t->outStream[st][t->streamCnt[st]++] = b1;
                        t->outStream[st][t->streamCnt[st]++] = b2;
                    }
                }
                break;

                case RLE24BITS:     // min count is 2
                {
                    // cnt is thus 1 bit smaller
                    int cnt = (outputBuffer[offset++]&0x1f)+2;
                    int b1 = outputBuffer[offset++];
                    int b2 = outputBuffer[offset++];
                    int b3 = outputBuffer[offset++];
                    if (debug) printf(" RLE24(%d x 0x%02X%02X%02X)", cnt, b1,b2,b3);
                    while (cnt--) {
                        t->outStream[st][t->streamCnt[st]++] = b1;
                        t->outStream[st][t->streamCnt[st]++] = b2;
                        t->outStream[st][t->streamCnt[st]++] = b3;
                    }
                }
                break;

                case RLE32BITS:     // min count is 2
                {
                    // cnt is thus 1 bit smaller
                    int cnt = (outputBuffer[offset++]&0x1f)+2;
                    int b1 = outputBuffer[offset++];
                    int b2 = outputBuffer[offset++];
                    int b3 = outputBuffer[offset++];
                    int b4 = outputBuffer[offset++];
                    if (debug) printf(" RLE32(%d x 0x%02X%02X%02X%02X)", cnt, b1,b2,b3,b4);
                    while (cnt--) {
                        t->outStream[st][t->streamCnt[st]++] = b1;
                        t->outStream[st][t->streamCnt[st]++] = b2;
                        t->outStream[st][t->streamCnt[st]++] = b3;
                        t->outStream[st][t->streamCnt[st]++] = b4;
                    }
                }
                break;

                case RLEBITS:       // min count is 3
                {
                    // cnt is thus 1 bit smaller
                    int cnt = (outputBuffer[offset++]&0x1f)+3;
                    int b1 = outputBuffer[offset++];
                    if (debug) printf(" RLE(%d x 0x%02X)", cnt, b1);
                    while (cnt--) {
                        t->outStream[st][t->streamCnt[st]++] = b1;
                    }
                }
                break;

                case INLINEBITS2:
                case INLINEBITS:    // min count is 1
                {
                    int cnt = (outputBuffer[offset++]&0x3f)+1;
                    if (debug) printf(" INLINE(%d)", cnt);
                    while (cnt--) {
                        t->outStream[st][t->streamCnt[st]++] = outputBuffer[offset++];
                    }
                }
                break;
            }
        }
        if (debug) printf("\n");
    }

    // and test it - VERIFY PHASE 2 means this whole SBF test
    for (int st=0; st<MAXSTREAMS; ++st) {
        if ((isSID) && ((st==3)||(st==7))) {
            continue;
        }

        // check length
        if (t->streamCnt[st] != s->streamCnt[st]) {
            printf("VERIFY PHASE 2 - failed on stream %d length, got 0x%X vs desired 0x%X\n",
                    st, t->streamCnt[st], s->streamCnt[st]);
            ret = false;
        }
        
        if ((ret)||(checkAnyway)) {
            if (!ret) printf("... checking anyway (may crash!)...\n");
            // check each row
            for (int idx = 0; idx<t->streamCnt[st]; ++idx) {
                if (t->outStream[st][idx] != s->outStream[st][idx]) {
                    printf("VERIFY PHASE 2 - failed on stream %d entry %d, got 0x%X vs desired 0x%X (aborting check)\n",
                            st, idx, t->outStream[st][idx], s->outStream[st][idx]);
                    ret = false;
                    break;
                }
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
    // NOTE: this still processes for isSID, even though we won't use the last channel

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
        // do nothing, this row is discarded
        //s->streamCnt[TIMESTREAM]--;
    } else {
        s->outStream[TIMESTREAM][s->streamCnt[TIMESTREAM]]--;
        ++s->streamCnt[TIMESTREAM]; // count this row
    }

    // at the end, re-set the first timestream command nibble to remove unused channels
    if (s->streamCnt[TONE1] == 0) s->outStream[TIMESTREAM][0] &= 0x7f;
    if (s->streamCnt[TONE2] == 0) s->outStream[TIMESTREAM][0] &= 0xbf;
    if (s->streamCnt[TONE3] == 0) s->outStream[TIMESTREAM][0] &= 0xdf;
    if (s->streamCnt[NOISE] == 0) s->outStream[TIMESTREAM][0] &= 0xef;

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
                if ((s->outStream[idx2][s->streamCnt[idx2]]&0x0f) > 0) {
                    // decrement the count and add the note
                    s->outStream[idx2][s->streamCnt[idx2]]--;
                    s->outStream[idx2][s->streamCnt[idx2]] |= (s->lastnote[idx2]<<4)&0xf0;
                    s->streamCnt[idx2]++;
                } else {
                    printf("Special case of wrap and count out (if a problem occurs, tell Tursi this line appeared)\n");
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
                    s->outStream[idx2][s->streamCnt[idx2]]=0;   // zero means 1, we already counted
                }
            }
        }
    }

    // reduce the final counts by 1, since we didn't get a chance above, also insert the data value
    // if it was zero (only possible in the maximum size case), then we can delete the entry entirely
    for (int idx2=VOL1; idx2<=VOL4; ++idx2) {
        if ((s->outStream[idx2][s->streamCnt[idx2]]&0xf) == 0) {
            // do nothing - it's empty - although how did we get here?
            //s->streamCnt[idx2]--;
            printf("Impossible RLE 0 case\n");  // haven't seen this actually hit...
        } else {
            s->outStream[idx2][s->streamCnt[idx2]]--;
            s->outStream[idx2][s->streamCnt[idx2]] |= (s->lastnote[idx2]<<4)&0xf0;
            s->streamCnt[idx2]++;
        }
    }

#ifdef _DEBUG
    // output the RLE streams
    for (int idx=TONE1; idx<=TIMESTREAM; ++idx) {
        char buf[128];
        sprintf(buf, "d:\\new\\rle%d.txt", idx);
        FILE *fp = fopen(buf, "w");
        for (int i2=0; i2<s->streamCnt[idx]; ++i2) {
            sprintf(buf, "%05d: %02X\n", i2, s->outStream[idx][i2]);
            fprintf(fp, "%s", buf);
        }
        fclose(fp);
    }
#endif

#if 0
    // Not using double-zero to mark the end - don't do this
    // I don't think it matters, but double-zero these too
    for (int idx2=VOL1; idx2<=VOL4; ++idx2) {
        s->streamCnt[idx2]++;
        s->outStream[idx2][s->streamCnt[idx2]]=0;
        s->streamCnt[idx2]++;
        s->outStream[idx2][s->streamCnt[idx2]]=0;
    }
#endif

    return true;
}

// return a score for bytes saved if we encode an RLE here (cnt is the remaining buffer size)
int RLEsize(int *str, int cnt) {
    if (cnt < 2) return 0;

    int out = 0;
    int match = *(str++);
    --cnt;

    while (cnt--) {
        if (*(str++) == match) {
            ++out;
        } else {
            break;
        }
    }

    // plus the first
    if (out) ++out;

    // limits
    if (out < 3) out = 0;
    if (out > 34) out = 34;

    // bytes saved - 2 bytes to encode
    out -= 2;
    if (out < 0) out = 0;

    return out;
}

// return a score for bytes saved if we encode an RLE16 here (cnt is the remaining buffer size)
int RLE16size(int *str, int cnt) {
    if (cnt < 4) return 0;

    int out = 0;
    int match1 = *(str++);
    int match2 = *(str++);
    cnt -= 2;

    while (cnt >= 2) {
        if (*(str++) == match1) {
            if (*(str++) == match2) {
                ++out;
            } else {
                break;
            }
        } else {
            break;
        }
        cnt -= 2;
    }

    // plus the first
    if (out) ++out;

    // limits
    if (out < 2) out = 0;
    if (out > 33) out = 33;

    // scale
    out *= 2;

    // bytes saved - 3 bytes to encode
    out -= 3;
    if (out < 0) out = 0;

    return out;
}

// return a score for bytes saved if we encode an RLE24 here (cnt is the remaining buffer size)
int RLE24size(int *str, int cnt) {
    if (cnt < 4) return 0;

    int out = 0;
    int match1 = *(str++);
    int match2 = *(str++);
    int match3 = *(str++);
    cnt -= 3;

    while (cnt >= 3) {
        if (*(str++) == match1) {
            if (*(str++) == match2) {
                if (*(str++) == match3) {
                    ++out;
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            break;
        }
        cnt -= 3;
    }

    // plus the first
    if (out) ++out;

    // limits
    if (out < 2) out = 0;
    if (out > 33) out = 33;

    // scale
    out *= 3;

    // bytes saved - 4 bytes to encode
    out -= 4;
    if (out < 0) out = 0;

    return out;
}

// return a score for bytes saved if we encode an RLE32 here (cnt is the remaining buffer size)
int RLE32size(int *str, int cnt) {
    if (cnt < 4) return 0;

    int out = 0;
    int match1 = *(str++);
    int match2 = *(str++);
    int match3 = *(str++);
    int match4 = *(str++);
    cnt -= 4;

    while (cnt >= 4) {
        if (*(str++) == match1) {
            if (*(str++) == match2) {
                if (*(str++) == match3) {
                    if (*(str++) == match4) {
                        ++out;
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        } else {
            break;
        }
        cnt -= 4;
    }

    // plus the first
    if (out) ++out;

    // limits
    if (out < 2) out = 0;
    if (out > 33) out = 33;

    // scale
    out *= 4;

    // bytes saved - 5 bytes to encode
    out -= 5;
    if (out < 0) out = 0;

    return out;
}

// string search a stream for all full and partial string matches forward
// - the source string is str and is strLen ints long
// - the search space is buf and is bufLen ints long
// - bestLen holds the longest match found and is increased as needed
// - bestScore contains the expected number of bytes saved accumulative with this string,
//   and can include strings shorter than bestLen!
// Note that buf is allowed to overlap str, and we should deal with that
// str MUST be part of buf for this to work
void stringSearchFwd(int *str, int strLen, int *buf, int bufLen, int &bestLen, int &bestScore) {
    // maximum length for an inline string is 64 bytes
    if (strLen > 64) {
        strLen = 64;
    }

    // walk through the buffer and count how many bytes match
    for (int x=0; x<bufLen; ++x) {
        // calculate a working length
        int tmpLen = strLen;
        // limit the search length if we are working on a substring
        // so we don't extend into the buffer we are matchin
        if (&buf[x] < (str+strLen)) {
            tmpLen = &buf[x]-str;
        }
        // handle the end of the buffer
        if (x+tmpLen >= bufLen) tmpLen = bufLen-x;

        // we just need to know how many ints match
        {
            int cnt = 0;
            while ((cnt < tmpLen) && (str[cnt] == buf[x+cnt])) {
                ++cnt;
            }
            
            // when the score gets longer, we disregard the previous score as
            // it's part of this one. But if we find a shorter match, that's
            // an ADDITIONAL score. That works for the first part, anyway...
            // it costs 3 to encode a backref, every time
            if (cnt > 3) {
                if (cnt > bestLen) {
                    bestScore -= bestLen-3;
                    if (bestScore < 0) bestScore = 0;
                    bestLen = cnt;
                }
                // it costs 3 to encode a backref, every time
                bestScore += cnt-3;
            }
        }
    }
}

// string search a char memory buffer for all full and partial string matches - used for back search
// - the source string is str and is strLen ints long (and assumed to hold bytes in ints!)
// - the search space is buf and is bufLen ints long (and assumed to be bytes!)
// - bestLen holds the longest match found
// - bestPos holds the offet (from buf) of the best match
// - bestScore holds the best score in terms of bytes saved (separate from len due to subtractions)
void memorySearchBack(int *str, int strLen, unsigned char *buf, int bufLen, int &bestLen, int &bestPos, int &bestScore) {
    int totalOut = 0;
    bestLen = 0;

    // maximum backref len is 67 bytes
    if (strLen > 67) {
        strLen = 67;
    }

    // strings need to be at least 4 bytes long to be useful
    // so starting at each position, see how many bytes match the string
    for (int bufpos = 0; bufpos < bufLen-4; ++bufpos) {
        int idx = 0;
        while ((unsigned char)(str[idx]) == buf[bufpos+idx]) {
            ++idx;
            if ((idx>strLen)||(idx+bufpos>=bufLen)) break;
        }
        int score = idx-3;  // a backref costs 3
        if (score < 0) score = 0;
        if (score > bestScore) {
            bestScore = score;
            bestLen = idx;
            bestPos = bufpos;
        }
    }
}

unsigned int xxx[100000]; // should be plenty, biggest song I have is 55k total
int xcnt=-1;
int xtot = 0;
int lastSt=-1;

// actually compress a single stream into the output buffer
// outputBuffer[64*1024] and outputPos
bool compressStream(int song, int st, int minstep) {
    int *str = songs[song].outStream[st];
    int cnt = songs[song].streamCnt[st];

    if (song == -1) {
        printf("Running test case\n");
        str = tstcase;
        cnt = sizeof(tstcase)/4;
    }

    // maximum size
    const int MAXSIZE = 63;         // 00111111

    // counter for debug throttling
    int dbg = 0;

    // used for collapsing consecutive inlines
    bool lastWasFwd = false;
    int lastFwdPos = 0;
    int lastFwdSize = 0;

    // like the old one, force a fixed minrun at the beginning for backrefs to jump into
    if (minRun > 1) {
        int firstSize = minRun-1;
        if (firstSize > cnt) firstSize = cnt;
        if (debug2) printf("Initial FWD %d bytes\n", firstSize);

        // remember this one - store size BEFORE we subtract 1
        lastWasFwd = true;
        lastFwdPos = outputPos;
        lastFwdSize = firstSize;

        // inline string - 1 + length bytes space to encode
        if (outputPos >= (signed)sizeof(outputBuffer)-firstSize-1) {
            printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
            return false;
        }

        // 1 is the smallest legal length, so map it down
        firstSize -= 1;
        if (firstSize > MAXSIZE) firstSize=MAXSIZE;   // that's all the bits we have
        outputBuffer[outputPos++] = INLINEBITS | firstSize;
        for (int idx=0; idx<=firstSize; ++idx) {
            outputBuffer[outputPos++] = ((*(str++)) & 0xff);
            --cnt;
        }

        // Already updated position and cnt above
        ++minRunData[minRun].cntFwds.cnt;
        minRunData[minRun].cntFwds.total += firstSize+1;
    }

    while (cnt > 0) {
        if (verbose) {
            if (dbg++ % 100 == 0) {
                printf("  %5d rows (%2d passes) remain...      \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b", 
                    cnt, minRunMax-minRun+(minstep < 500?minRunMax-minRunMin : 0));
            }
        }

        // First, check if this is a single-byte RLE run (each function defines its own limits)
        // Forcing minRun on the RLE typically results in worse performance
        // the various runs are rated in /bytes saved/
        int rleRun = 0;
        if (!noRLE) {
            rleRun = RLEsize(str, cnt);
        }
        int rle16Run = 0;
        if (!noRLE16) {
            rle16Run = RLE16size(str, cnt);
        }
        int rle24Run = 0;
        if (!noRLE24) {
            rle24Run = RLE24size(str, cnt);
        }
        int rle32Run = 0;
        if (!noRLE32) {
            rle32Run = RLE32size(str, cnt);
        }

        // then check for string matches
        // The optimization of string searches makes or breaks this algorithm.

        // Find the ending point of this string
        int thisLen = (cnt < 67 ? cnt : 67);        // maximum possible run for backref - cnt is rows remaining in data
        int fakeLen = thisLen;                      // we only truncate for fakeFwd, not the search - this appears to help

        // determine if there are any good overlaps inside this string
        // and stop at the first repeated data, if there are. We do this by sliding the string over itself
        // and checking how many sequential matches we find.
        // for each possible offset. Confirmed useful
        if ((!noFwdSearch)||(!noBwdSearch)) {
            int match=0;
            int matchpos=0;
            int bestmatch=0;
            int bestpos=0;
            for (int off=1; off<thisLen; ++off) {
                // it's okay to search off the string if there's room
                // tested checking only half the string, but that was worse
                int testlen = (off+thisLen > cnt ? cnt : thisLen);
                for (int tst=0; tst<testlen; ++tst) {
                    if (off+tst >= cnt) break;  // off end of string
                    if ((match)&&(tst >= matchpos)) {
                        // past estimated truncation point
                        match = 0;
                        matchpos = 0;
                    }                        
                    if (str[off+tst] != str[tst]) {
                        // we don't break cause we are looking for ANY substrings
                        // but we have to clear the counters. We already saved
                        // bestmatch if it was one.
                        match = 0;
                        matchpos = 0;
                    } else {
                        if (match == 0) {
                            match=1;
                            matchpos=off+tst;
                        } else {
                            ++match;
                            if (match > bestmatch) {
                                // skipping early is much worse - this search is worth it
                                bestmatch=match;
                                bestpos=matchpos;
                            }
                        }
                    }
                }
            }
            // minimum 4 because a backref only saves if it's more than 3 bytes long
            // again, this is used only if we have to store a fakefwd (an inline with no
            // decent forward matches found).
            if ((bestmatch >= 4) && (bestpos < fakeLen)) {
//                if (debug2) printf("FakeFwd length reduced from %d to %d for substring match\n", fakeLen, bestpos);
                fakeLen = bestpos;
            }
        }

        // now we have a string at str, length of thisLen
        int bestBwdScore = 0;
        int bestBwdLen = 0;
        int bestBwdPos = 0; // no need for a fwdPos because that's "here".

        // Search backward and see if this string or any subset matches, save the best score
        // bestBwdLen, bestBwdPos and bestBwdScore will be updated a needed
        if (!noBwdSearch) {
            memorySearchBack(str, thisLen, outputBuffer, outputPos, bestBwdLen, bestBwdPos, bestBwdScore);
        }

        // now try the forward search
        // save the best hit in terms of bytes saved. of course a forward ref isn't a
        // guarantee, but we should EXPECT it to be used later!
        int bestFwdLen = 0;
        int bestFwdScore = 0;
        if (!noFwdSearch) {
            // note that we include the region INSIDE the current string, just in case!
            // minstep varies between '1' and '999' (which will
            // keep the whole string), cause despite my best efforts it is proven that
            // sometimes searching inside the string is better, sometimes keeping it all
            // is better, and it's all too interdependent to not just try both.
            int off = thisLen > minstep ? minstep: thisLen;
            stringSearchFwd(str, thisLen, str+off, cnt-off, bestFwdLen, bestFwdScore);

            // now check the remaining streams for forward refs
            for (int sng=song; sng<currentSong; ++sng) {
                for (int stx=(sng==song?st+1:0); stx<MAXSTREAMS; ++stx) {
                    // again, for SID we have to skip the omitted streams so we don't
                    // base compression decisions based on them...
                    if ((isSID) && ((stx==3)||(stx==7))) continue;
                    stringSearchFwd(str, thisLen, songs[sng].outStream[stx], songs[sng].streamCnt[stx], bestFwdLen, bestFwdScore);
                }
            }
        }

        // fwdSearch contains the expected bytes saved (minus the backreference costs) if we save bestFwd bytes
        // as a string. bestBwd contains the best backwards match for this string.
        // if bestBwd >= bestFwd, then we always take the back reference (because the forward references will
        // also find it). Otherwise, fwdSearch should be greater than bestBw-3 (which is our backref savings).
        // This still isn't necessarily perfect, since the later matches would still find parts of the backref,
        // but in theory it should turn out nicely. It might be a place to try a tuning parameter and go
        // for some level of additional slack...
        bool isBackref = false;
        bool fakeFwd = false;
        // backref takes priority over forward ref, since a forward ref can
        // use at least part of the same backref, and this seemed to work best
        // even after testing various scales
        if (bestBwdScore > 0) {
            isBackref = true;
        } else if (bestFwdScore < minRun) {
            // we have nothing, not even a forward, so check for RLE to end the string
            // confirmed useful
            for (int idx=1; idx<thisLen-1; ++idx) {
                if (!noRLE) {
                    if (RLEsize(str+idx, thisLen-idx) > 0) {
                        thisLen = idx;
                        break;
                    }
                }
                if (!noRLE16) {
                    if (RLE16size(str+idx, thisLen-idx) > 0) {
                        thisLen = idx;
                        break;
                    }
                }
                if (!noRLE24) {
                    if (RLE24size(str+idx, thisLen-idx) > 0) {
                        thisLen = idx;
                        break;
                    }
                }
                if (!noRLE32) {
                    if (RLE32size(str+idx, thisLen-idx) > 0) {
                        thisLen = idx;
                        break;
                    }
                }
            }

            // don't take more than the RLE determined
            if (thisLen < fakeLen) {
//              if (debug2) printf("RLE potential reduced length from %d to %d\n", fakeLen, thisLen);
                fakeLen = thisLen;
            }

            // going to do a fake fwd, but only as far as the determined string
            bestFwdLen = fakeLen;
            thisLen = fakeLen;
            fakeFwd = true;     // tells us to ignore forward for RLE test
        }

        // finally, determine whether we should RLE instead of doing a string
        // we only need one flag, since both RLEs are not possible
        // if always RLE is set, we'll take it whether it looks better than
        // the string or not
        bool doRLE = alwaysRLE;
        if (isBackref) {
            if (rleRun > bestBwdScore) doRLE=true;
            if (rle16Run > bestBwdScore) doRLE=true;
            if (rle24Run > bestBwdScore) doRLE=true;
            if (rle32Run > bestBwdScore) doRLE=true;
        } else {
            if (fakeFwd) doRLE = true;
            if (rleRun > bestFwdScore) doRLE=true;
            if (rle16Run > bestFwdScore) doRLE=true;
            if (rle24Run > bestFwdScore) doRLE=true;
            if (rle32Run > bestFwdScore) doRLE=true;
        }

        if (debug2) {
            // print the decision making data
            printf("%d:[%d] Search: %3d BestBwd: %3d BestFwd: %3d%s RLE: %3d RLE16: %3d RLE24: %3d  RLE32: %3d Decided: ", 
                st, minRun, thisLen, bestBwdScore, bestFwdScore, fakeFwd?"*":" ",
                rleRun, rle16Run, rle24Run, rle32Run);
        }

        if (doRLE) {
            // if we don't have an RLE (such as possible in the fakeFwd case)
            // then we'll fall out of here into ths string handler
            if (rle16Run > rleRun) {
                // force use of the 16-bit one instead - rare case but larger range
                rleRun = 0;
            }
            if (rle24Run > rleRun) {
                // force use of the 24-bit one instead - rare case but larger range
                rleRun = 0;
            }
            if (rle24Run > rle16Run) {
                // force use of the 24-bit one instead - rare case but larger range
                rle16Run = 0;
            }
            if (rle32Run > rleRun) {
                // force use of the 32-bit one instead - rare case but larger range
                rleRun = 0;
            }
            if (rle32Run > rle24Run) {
                // force use of the 32-bit one instead - rare case but larger range
                rle24Run = 0;
            }
            if (rle32Run > rle16Run) {
                // force use of the 32-bit one instead - rare case but larger range
                rle16Run = 0;
            }

            // the RLEs have already been gated by minimum size, so if not zero they are valid
            if (rleRun > 0) {
                if (debug2) printf("RLE\n");
                lastWasFwd = false;
                // we need 2 bytes space to encode
                if (outputPos >= sizeof(outputBuffer)-2) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // rleRun is scores as bytes saved, so that's number of RLE bytes minus 2. An encode value of 0
                // means 3. But we want to encode the number of RLE runs (well, in this case that's the same)
                rleRun--;   // so now it's the count minus 3, which is what we encode
                if (rleRun > 31) rleRun=31;   // that's all the bits we have
                outputBuffer[outputPos++] = RLEBITS | rleRun;
                outputBuffer[outputPos++] = (*str) & 0xff;

                // update the input position - careful of the offset tweaking above
                str += rleRun+3;
                cnt -= rleRun+3;
                ++minRunData[minRun].cntRLEs.cnt;
                minRunData[minRun].cntRLEs.total += rleRun+3;
                continue;
            }
            if (rle16Run > 0) {
                if (debug2) printf("RLE16\n");
                lastWasFwd = false;
                // we need 3 bytes space to encode
                if (outputPos >= sizeof(outputBuffer)-3) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // rle16Run is scores as bytes saved, so that's number of RLE cyles times 2, minus 3.
                // But we want to encode the number of RLE runs minus 2
                rle16Run = (rle16Run+3)/2 - 2;
                if (rle16Run > 0x1f) rle16Run=0x1f;   // that's all the bits we have
                outputBuffer[outputPos++] = RLE16BITS | rle16Run;
                outputBuffer[outputPos++] = (*(str)) & 0xff;
                outputBuffer[outputPos++] = (*(str+1)) & 0xff;

                // update the input position - careful of the offset tweaking above
                str += (rle16Run+2)*2;
                cnt -= (rle16Run+2)*2;
                ++minRunData[minRun].cntRLE16s.cnt;
                minRunData[minRun].cntRLE16s.total += (rle16Run+2)*2;
                continue;
            }
            if (rle24Run > 0) {
                if (debug2) printf("RLE24\n");
                lastWasFwd = false;
                // we need 4 bytes space to encode
                if (outputPos >= sizeof(outputBuffer)-4) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // rle24Run is scores as bytes saved, so that's number of RLE cyles times 3, minus 4.
                // But we want to encode the number of RLE runs minus 2
                rle24Run = (rle24Run+4)/3 - 2;
                if (rle24Run > 31) rle24Run=31;   // that's all the bits we have
                outputBuffer[outputPos++] = RLE24BITS | rle24Run;
                outputBuffer[outputPos++] = (*(str)) & 0xff;
                outputBuffer[outputPos++] = (*(str+1)) & 0xff;
                outputBuffer[outputPos++] = (*(str+2)) & 0xff;

                // update the input position - careful of the offset tweaking above
                str += (rle24Run+2)*3;
                cnt -= (rle24Run+2)*3;
                ++minRunData[minRun].cntRLE24s.cnt;
                minRunData[minRun].cntRLE24s.total += (rle24Run+2)*3;
                continue;
            }
            if (rle32Run > 0) {
                if (debug2) printf("RLE32\n");
                lastWasFwd = false;
                // we need 5 bytes space to encode
                if (outputPos >= sizeof(outputBuffer)-5) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // rle32Run is scores as bytes saved, so that's number of RLE cyles times 4, minus 5.
                // But we want to encode the number of RLE runs minus 2
                rle32Run = (rle32Run+5)/4 - 2;
                if (rle32Run > 31) rle32Run=31;   // that's all the bits we have
                outputBuffer[outputPos++] = RLE32BITS | rle32Run;
                outputBuffer[outputPos++] = (*(str)) & 0xff;
                outputBuffer[outputPos++] = (*(str+1)) & 0xff;
                outputBuffer[outputPos++] = (*(str+2)) & 0xff;
                outputBuffer[outputPos++] = (*(str+3)) & 0xff;

                // update the input position - careful of the offset tweaking above
                str += (rle32Run+2)*4;
                cnt -= (rle32Run+2)*4;
                ++minRunData[minRun].cntRLE32s.cnt;
                minRunData[minRun].cntRLE32s.total += (rle32Run+2)*4;
                continue;
            }
        }

        // handle it as a string
        if (isBackref) {
            if (debug2) printf("Back\n");
            lastWasFwd = false;
            // we need 3 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-3) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }

            // 4 is the smallest legal length, so map it down
            bestBwdLen -= 4;
            if (bestBwdLen > MAXSIZE) bestBwdLen=MAXSIZE;   // that's all the bits we have
            outputBuffer[outputPos++] = BACKREFBITS | bestBwdLen;
            outputBuffer[outputPos++] = bestBwdPos / 256;
            outputBuffer[outputPos++] = bestBwdPos % 256;

            // update the input position - careful of the offset tweaking above
            str += bestBwdLen+4;
            cnt -= bestBwdLen+4;
            ++minRunData[minRun].cntBacks.cnt;
            minRunData[minRun].cntBacks.total += bestBwdLen+4;
            continue;
        } else {
            if (debug2) if (fakeFwd) printf("FakeFwd"); else printf("Fwd");

            // don't take more than this - there should be a backref there
            if (fakeLen < bestFwdLen) {
                bestFwdLen = fakeLen;
                thisLen = fakeLen;
            }

            // if the last was also an inline reference, we might collapse this one
            // into it, if it fits. This is simpler than changing the code which
            // tries to decide how much of an inline to evaluate.
            // The maximum size of a forward is 64 bytes - if this one
            // will not fit, then we don't bother. Two sequences are two sequences,
            // and it doesn't matter if one is full. It may be better not, if the
            // earlier code correctly detected a split point.
            if ((lastWasFwd) && (lastFwdSize+bestFwdLen <= 64)) {
                if (debug2) printf(" (Merged)\n");

                // inline string - just need to add the length to the old encoding
                if (outputPos >= (signed)sizeof(outputBuffer)-bestFwdLen) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // fix up the size output - remember the /actual/ size, not the encoded one
                // lastWasFwd and lastFwdPos don't change.
                lastFwdSize += bestFwdLen;
                // subtract 1 when encoding for the minimum count of 1 entry
                outputBuffer[lastFwdPos] = INLINEBITS | (lastFwdSize-1);

                // now append the bytes
                // we do the subtraction here just to make the loop the same as below
                --bestFwdLen;
                for (int idx=0; idx<=bestFwdLen; ++idx) {
                    outputBuffer[outputPos++] = ((*(str++)) & 0xff);
                    --cnt;
                }

                // we'll update the appropriate total, but stats gets screwy here
                // we'll end up mixing fakes and fwds, but it'll probably be close enough
                // Don't update the count, though, we didn't add a new one!
                if (fakeFwd) {
                    minRunData[minRun].cntInlines.total += bestFwdLen-1;
                } else {
                    minRunData[minRun].cntFwds.total += bestFwdLen-1;
                }
            } else {
                if (debug2) printf("\n");

                // remember this one - store size BEFORE we subtract 1
                lastWasFwd = true;
                lastFwdPos = outputPos;
                lastFwdSize = bestFwdLen;

                // inline string - 1 + length bytes space to encode
                if (outputPos >= (signed)sizeof(outputBuffer)-bestFwdLen-1) {
                    printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                    return false;
                }

                // 1 is the smallest legal length, so map it down
                bestFwdLen -= 1;
                if (bestFwdLen > MAXSIZE) bestFwdLen=MAXSIZE;   // that's all the bits we have
                outputBuffer[outputPos++] = INLINEBITS | bestFwdLen;
                for (int idx=0; idx<=bestFwdLen; ++idx) {   // <= will make sure we include the -1'd count
                    outputBuffer[outputPos++] = ((*(str++)) & 0xff);
                    --cnt;
                }

                // Already updated position and cnt above
                if (fakeFwd) {
                    ++minRunData[minRun].cntInlines.cnt;
                    minRunData[minRun].cntInlines.total += bestFwdLen+1;
                } else {
                    ++minRunData[minRun].cntFwds.cnt;
                    minRunData[minRun].cntFwds.total += bestFwdLen+1;
                }
            }
            continue;
        }
    }

    // Set the end flag for the stream - three bytes - a long back reference offset of 0
    if (outputPos >= sizeof(outputBuffer)-3) {
        printf("Ran out of room in outputBuffer (maximum 64k!!) Encode failed.\n");
        return false;
    }
    outputBuffer[outputPos++] = BACKREFBITS;
    outputBuffer[outputPos++] = 0;
    outputBuffer[outputPos++] = 0;

    if (verbose) {
        printf("                                        \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    }
    return true;
}

// these are stored in noiseMask instead
#define NOISE_MASK     0x0000F              // noise should be pre-converted to target
#define NOISE_MASK_AY  0x0001F              // AY has larger range
#define NOISE_MASK_SID 0x0ffff              // SID has full range
// these in toneMask
#define TONE_MASK      0x003ff
#define TONE_MASK_AY   0x00fff
#define TONE_MASK_SID  0x0ffff

int main(int argc, char *argv[])
{
	printf("VGMComp2 Compression Tool - v20200625\n\n");

    // parse arguments
    int nextarg = -1;
    for (int idx=1; idx<argc; ++idx) {
        if (argv[idx][0]=='-') {
            // parse switch
            if (argv[idx][1] == 'h') {
                // cover 'help' by not loading the filename arg
                break;
            } else if ((argv[idx][1] == 'd')&&(argv[idx][2] == 'd')) {
                // secret: "-dd" for encoding debug. You still need "-d" for the test debug
                debug2 = true;
            } else if ((argv[idx][1] == 'd') && (argv[idx][2] == '\0')) {
                debug = true;
            } else if (argv[idx][1] == 'v') {
                verbose= true;
            } else if (0 == strcmp(argv[idx], "-sn")) {
                isPSG=true;
                noiseMask = NOISE_MASK;
                toneMask = TONE_MASK;
                if ((isAY)||(isSID)) {
                    printf("Can't select multiple PSG options.\n");
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-ay")) {
                isAY=true;
                noiseMask = NOISE_MASK_AY;
                toneMask = TONE_MASK_AY;
                if ((isPSG)||(isSID)) {
                    printf("Can't select multiple PSG options.\n");
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-sid")) {
                isSID=true;
                noiseMask = NOISE_MASK_SID;
                toneMask = TONE_MASK_SID;
                if ((isPSG)||(isAY)) {
                    printf("Can't select multiple PSG options.\n");
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-minrun")) {
                ++idx;
                if (idx>=argc) {
                    printf("Not enough arguments for minrun!\n");
                    return 1;
                }
                if (2 != sscanf(argv[idx], "%d,%d", &minRunMin, &minRunMax)) {
                    printf("Parse error on minrun, must be like: -minrun 1,7\n");
                    return 1;
                }
                if (minRunMin < 1) {
                    printf("Minimum run must be at least 1\n");
                    return 1;
                }
                ++minRunMax;
                if (minRunMax < minRunMin) {
                    printf("MinRunMax must be larger than MinRunMin, got %d,%d\n", minRunMin, minRunMax);
                    return 1;
                }
                if (minRunMax > MAXRUN) {
                    printf("MinRunMax must be less than %d\n", MAXRUN);
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-alwaysrle")) {
                alwaysRLE = true;
            } else if (0 == strcmp(argv[idx], "-norle")) {
                noRLE = true;
            } else if (0 == strcmp(argv[idx], "-norle16")) {
                noRLE16 = true;
            } else if (0 == strcmp(argv[idx], "-norle24")) {
                noRLE24 = true;
            } else if (0 == strcmp(argv[idx], "-norle32")) {
                noRLE32 = true;
            } else if (0 == strcmp(argv[idx], "-nofwd")) {
                noFwdSearch = true;
            } else if (0 == strcmp(argv[idx], "-nobwd")) {
                noBwdSearch = true;
                noFwdSearch = true; // no point searching forward if a back search will never find it
            } else if (0 == strcmp(argv[idx], "-deepdive")) {
                doDeepDive = true;  // secret option that tries most switches on each stream - very slow
            } else if (0 == strcmp(argv[idx], "-checkanyway")) {
                checkAnyway = true; // secret option that does a stream decode in phase 2 even if length is wrong
            } else {
                printf("Unknown switch '%s'\n", argv[idx]);
            }
        } else {
            // no more arguments, into the filenames
            nextarg = idx;
            break;
        }
    }

    // get the output file as the last one
    if (nextarg < argc-1) {
        strcpy(szFileOut, argv[argc-1]);
        --argc;
    } else {
        printf("Not enough input and output arguments!\n");
        return 1;
    }

    if ((strlen(szFileOut)==0)||((isAY==false)&&(isPSG==false)&&(isSID==false))) {
        if ((isAY==false)&&(isPSG==false)&&(isSID==false)) {
            printf("* You must specify -ay, -sn or -sid for output type\n");
        }
        printf("vgmcomp2 [-d] [-dd] [-v] [-minrun s,e] [-alwaysrle] [-norle] [-norle16] [-norle24] [-norle32] [-nofwd] [-nobwd] <-ay|-sn> <filenamein1.psg> [<filenamein2.psg>...] <filenameout.sbf>\n");
        printf("\nProvides a compressed (sound compressed format) file from\n");
        printf("an input file containing either PSG or AY-3-8910 data\n");
        printf("Except for the noise handling, the output is the same.\n");
        printf("Specify either -ay for the AY-3-8910 data or -sn for PSG data\n");
        printf("Then input file(s) and lastly output filename.\n");
        printf("\n-d - add extra parser debug output\n");
        printf("-dd - add extra compressor debug output (does not include -d)\n");
        printf("-v - add extra verbose information\n");
        printf("-minrun - specify start and end for minrun search. Default %d,%d. Maximum 0,20\n", minRunMin, minRunMax);
        printf("\nThe following tuning options may very rarely be helpful. They apply to the SBF compression and not the initial RLE pack:\n");
        printf("-alwaysrle - always use RLE over string if available - the following disables are still honored\n");
        printf("-norle - disable single-byte RLE encoding\n");
        printf("-norle16 - disable 16-bit RLE encoding\n");
        printf("-norle24 - disable 24-bit RLE encoding\n");
        printf("-norle32 - disable 32-bit RLE encoding\n");
        printf("-nofwd - disable forward searching\n");
        printf("-nobwd - disable backward searching (implies nofwd, unlikely to be useful)\n");
        return 1;
    }

#ifdef RUNBACKREFTEST
    if (debug2) {
        compressStream(-1, 0, 1);
        printf("Compression: %d -> %d bytes\n", sizeof(tstcase)/4, outputPos);
        return 0;
    }
#endif

    // loop through all the songs
    int totalC = 0;
    for (int fileIdx = nextarg; fileIdx < argc; ++fileIdx) {
        // load data file through loadDataCSV(FILE *fp, int &chan, int &cnt, int column, bool noise)
        FILE *fp = fopen(argv[fileIdx], "r");
        if (NULL == fp) {
            printf("Failed to open input file '%s'\n", argv[fileIdx]);
            return 2;
        }
        int chan = 0;
        for (int idx=TONE1; idx<=NOISE; ++idx) {
            if (!loadDataCSV(fp, chan, &songs[currentSong], idx*2, idx==NOISE)) {
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
                note &= toneMask;
                if (note != songs[thisSong].VGMDAT[chan][idx]) {
                    printf("Warning - out of range note clipped (song %d, row %d, channel %d). Songs should be pre-processed for target chip!\n",
                           thisSong, idx, chan);
                }

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
    if (verbose) printf("Songbank contains %d/%d notes.\n\n", noteCnt, MAXNOTES);

    // do the RLE pack. It's necessary because it allows us to have RLE in the post-encoded stream
    // Confirmed this makes an important difference 99.99% of the time (okay, 99.99 is a guess... but up there)
    for (int idx=0; idx<currentSong; ++idx) {
        if (verbose) printf("RLE Packing song %d (%d rows)...\n", idx, songs[idx].outCnt);
        if (!initialRLE(&songs[idx])) {
            return 1;
        }
        if (verbose) {
            printf("  - Tone 1 to %d rows\n", songs[idx].streamCnt[TONE1]);
            printf("  - Tone 2 to %d rows\n", songs[idx].streamCnt[TONE2]);
            printf("  - Tone 3 to %d rows\n", songs[idx].streamCnt[TONE3]);
            if (!isSID) printf("  - Noise  to %d rows\n", songs[idx].streamCnt[NOISE]);
            printf("  - Vol  1 to %d rows\n", songs[idx].streamCnt[VOL1]);
            printf("  - Vol  2 to %d rows\n", songs[idx].streamCnt[VOL2]);
            printf("  - Vol  3 to %d rows\n", songs[idx].streamCnt[VOL3]);
            if (!isSID) printf("  - Vol  4 to %d rows\n", songs[idx].streamCnt[VOL4]);
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
    if (verbose) printf("RLE tests successful.\n\n");

#ifdef _DEBUG
    // dump the streams so I can look at them
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            char buf[128];
            sprintf(buf, "D:\\new\\stream%d_%d.bin", song, st);
            FILE *fp=fopen(buf, "wb");
            // need to write bytes, but the data is stored in ints
            for (int idx=0; idx<songs[song].streamCnt[st]; ++idx) {
                fputc(songs[song].outStream[st][idx]&0xff, fp);
            }
            fclose(fp);
        }
    }
#endif

    // prepare the output binary blob
    memset(outputBuffer, 0xff, sizeof(outputBuffer));
    outputBuffer[STREAMTABLE]=0; outputBuffer[STREAMTABLE+1]=0;   // song stream pointer table, filled in later
    outputBuffer[NOTETABLE]=0;   outputBuffer[NOTETABLE+1]=0;     // note table pointer table, filled in later
    outputPos = NOTETABLE+2;

    // store totals here
    minRunData[MAXRUN].cntBacks.reset();
    minRunData[MAXRUN].cntFwds.reset();
    minRunData[MAXRUN].cntInlines.reset();
    minRunData[MAXRUN].cntRLE32s.reset();
    minRunData[MAXRUN].cntRLE24s.reset();
    minRunData[MAXRUN].cntRLE16s.reset();
    minRunData[MAXRUN].cntRLEs.reset();

    // Individually compress each of the 9 channels (all treated the same)
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            // now here we DO have to skip the SID tracks, because otherwise
            // they might be used for compression by other tracks. This means
            // a bit of ugly checks since it's hard to skip two interleaved streams
            if ((isSID) && ((st==3)||(st==7))) {
                if (verbose) {
                    printf("SBF skip    song %d, stream %d for SID...\n", song, st);
                }
                songs[song].streamOffset[st] = 0;   // no stream encoded
                continue;
            }

            if (verbose) {
                printf("SBF packing song %d, stream %d...", song, st);
            }
            songs[song].streamOffset[st] = outputPos;

            // it appears that the minRun changing is still worth it, and that it needs
            // to be per stream. In addition, it appears that the full range is still
            // useful, as my first test case had bests from 0 to 13 in it. Over 400 bytes
            // were saved doing this, so gonna say worth it. Minruns /might/ also help
            // order matter less (based on just one test, mind you)
            int bestRun = 0;
            int bestDive = 0;
            int bestSize = 65536;

            // experimental deep dive code - tries all the likely combinations of switches
            // this takes a really long time and frankly doesn't help much. It only saves
            // 178 bytes on my 680 rock title (from 32136 to 31958). So... hidden switch. ;)
            for (int deepdive = 0; deepdive<0x40; ++deepdive) {
                if (doDeepDive) {
                    if (deepdive&0x01) noRLE = true; else noRLE = false;
                    if (deepdive&0x02) noRLE16 = true; else noRLE16 = false;
                    if (deepdive&0x04) noRLE24 = true; else noRLE24 = false;
                    if (deepdive&0x08) noRLE32 = true; else noRLE32 = false;
                    if (deepdive&0x10) noFwdSearch = true; else noFwdSearch = false;
                    if (deepdive&0x20) noBwdSearch = true; else noBwdSearch = false;
                    if (noBwdSearch == true && noFwdSearch == false) continue;
                    if (verbose) {
                        printf("\rSBF packing song %d, stream %d, dive %d...", song, st, deepdive);
                    }
                    if (debug2) {
                        printf("DEEP DIVE: %d noRLE:%d noRLE16:%d noRLE24:%d noRLE32:%d noFwd:%d noBwd:%d\n", deepdive,
                            noRLE, noRLE16, noRLE24, noRLE32, noFwdSearch, noBwdSearch);
                    }
                } else {
                    // jump right to last loop, don't touch options
                    deepdive = 0x3f;
                }

                // this loop only tests 2 starting steps - 4 and 998 (and that's more of a dummy for 'all')
                // the intent is '4' searches inside the string starting at 4, and 998 starts the search
                // after the string max size (normally 63, unless we're near the end of the buffer)
                for (int stepLen = 4; stepLen < 1000;  stepLen+=994) {
                    for (minRun = minRunMin; minRun < minRunMax; ++minRun) {
                        minRunData[minRun].cntBacks.reset();
                        minRunData[minRun].cntFwds.reset();
                        minRunData[minRun].cntInlines.reset();
                        minRunData[minRun].cntRLE32s.reset();
                        minRunData[minRun].cntRLE24s.reset();
                        minRunData[minRun].cntRLE16s.reset();
                        minRunData[minRun].cntRLEs.reset();

                        outputPos = songs[song].streamOffset[st];

                        if (!compressStream(song, st, stepLen)) {
                            return 1;
                        }

                        if (debug2) {
                            printf("[%d] got size %d (Mode %d)", minRun, outputPos-songs[song].streamOffset[st], stepLen < 500 ? 1:2);
                        }

                        if (outputPos-songs[song].streamOffset[st] < bestSize) {
                            if (debug2) printf(" (BEST!)");
                            bestSize = outputPos-songs[song].streamOffset[st];
                            bestRun = minRun;
                            bestDive = deepdive;
                            memcpy(bestRunBuffer, &outputBuffer[songs[song].streamOffset[st]], bestSize);
                        }

                        if (debug2) printf("\n");
                    }
                }

            }// deep dive

            // reload the best one...
            if (verbose) {
                printf("%4d bytes at minrun %d\n", bestSize, bestRun);
                if (doDeepDive) {
                    // okay to change these, they will be changed again before use
                    if (bestDive&0x01) noRLE = true; else noRLE = false;
                    if (bestDive&0x02) noRLE16 = true; else noRLE16 = false;
                    if (bestDive&0x04) noRLE24 = true; else noRLE24 = false;
                    if (bestDive&0x08) noRLE32 = true; else noRLE32 = false;
                    if (bestDive&0x10) noFwdSearch = true; else noFwdSearch = false;
                    if (bestDive&0x20) noBwdSearch = true; else noBwdSearch = false;
                    printf("  BEST DIVE: %d noRLE:%d noRLE16:%d noRLE24:%d noRLE32:%d noFwd:%d noBwd:%d\n", 
                        bestDive, noRLE, noRLE16, noRLE24, noRLE32, noFwdSearch, noBwdSearch);
                }
            }

            memcpy(&outputBuffer[songs[song].streamOffset[st]], bestRunBuffer, bestSize);
            outputPos = songs[song].streamOffset[st] + bestSize;
            // update totals
            minRunData[MAXRUN].cntBacks += minRunData[bestRun].cntBacks;
            minRunData[MAXRUN].cntFwds += minRunData[bestRun].cntFwds;
            minRunData[MAXRUN].cntInlines += minRunData[bestRun].cntInlines;
            minRunData[MAXRUN].cntRLE32s += minRunData[bestRun].cntRLE32s;
            minRunData[MAXRUN].cntRLE24s += minRunData[bestRun].cntRLE24s;
            minRunData[MAXRUN].cntRLE16s += minRunData[bestRun].cntRLE16s;
            minRunData[MAXRUN].cntRLEs += minRunData[bestRun].cntRLEs;
        }
    }

    // word align the tables so the TI can pull a word at a time
    if (outputPos&1) {
        outputBuffer[outputPos++]=0;
    }

    // write out the tables - the stream table is first
    outputBuffer[STREAMTABLE]   = outputPos/256; 
    outputBuffer[STREAMTABLE+1] = outputPos%256;
    if (debug) printf("Stream table @ 0x%02X%02X:\n", outputBuffer[STREAMTABLE], outputBuffer[STREAMTABLE+1]);
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            // we need 2 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-2) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }
            outputBuffer[outputPos++] = songs[song].streamOffset[st] / 256;
            outputBuffer[outputPos++] = songs[song].streamOffset[st] % 256;
            if (debug) printf("  [%d]: 0x%04X -> 0x%02X%02X\n", st, songs[song].streamOffset[st], outputBuffer[outputPos-2], outputBuffer[outputPos-1]);
        }
    }

    // then the note table
    outputBuffer[NOTETABLE]   = outputPos/256; 
    outputBuffer[NOTETABLE+1] = outputPos%256;
    if (debug) printf("Note table @ 0x%02X%02X:\n", outputBuffer[NOTETABLE], outputBuffer[NOTETABLE+1]);
    for (int nt=0; nt<noteCnt; ++nt) {
        // we need 2 bytes space to encode
        if (outputPos >= sizeof(outputBuffer)-2) {
            printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
            return false;
        }
        if (isPSG) {
            // PSG uses a two byte format, where the first byte contains the
            // least significant nibble plus a command nibble, and the second
            // byte contains the most significant 6 bits
            outputBuffer[outputPos++] = noteTable[nt] & 0xf;
            outputBuffer[outputPos++] = (noteTable[nt] >> 4) & 0x3f;
        } else if (isAY) {
            // AY uses two registers. The even register contains the least
            // significant 8 bits, and the odd register contains the most
            // significant 4 bits
            outputBuffer[outputPos++] = noteTable[nt] & 0xff;
            outputBuffer[outputPos++] = (noteTable[nt] >> 8) & 0xf;
        } else if (isSID) {
            // SID uses two registers, and is stored little endian first, full 16 bits
            outputBuffer[outputPos++] = noteTable[nt] & 0xff;
            outputBuffer[outputPos++] = (noteTable[nt] >> 8) & 0xff;
        } else {
            printf("Output failed - neither PSG, AY, nor SID mode set?\n");
            return false;
        }
        if (debug) printf("  [%3d]: 0x%04X -> 0x%02X%02X\n", nt, noteTable[nt], outputBuffer[outputPos-2], outputBuffer[outputPos-1]);
    }

#ifdef _DEBUG
    // dump the compressed streams so I can look at them
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            char buf[128];
            sprintf(buf, "D:\\new\\stream%d_%d.sbf", song, st);
            FILE *fp=fopen(buf, "wb");
            // can just dump it now that it's binary
            int start = songs[song].streamOffset[st];
            int end;
            if (start == 0) continue;
            if (st == MAXSTREAMS-1) {
                if (song == currentSong-1) {
                    // last stream, last song, take till the note table
                    end = outputBuffer[NOTETABLE]*256+outputBuffer[NOTETABLE+1];
                } else {
                    // last stream, more songs, take till the next song's first stream
                    end = songs[song+1].streamOffset[0];
                }
            } else {
                // more streams, take till the next stream
                end = songs[song].streamOffset[st+1];
            }
            fwrite(&outputBuffer[start], 1, end-start, fp);
            fclose(fp);
        }
    }
#endif

    // test the SBF process
    for (int idx=0; idx<currentSong; ++idx) {
        if (!testOutputSBF(&songs[idx], idx)) {
            return 1;
        }
    }

    // it all seems to have worked
    printf("Successful!\n");

    // emit some stats
    int totalRows = 0;
    for (int idx=0; idx<currentSong; ++idx) {
        totalRows += songs[idx].outCnt;
    }
    double totalTime = int(totalRows / 60.0 * 100 + 0.5) / 100.0;
    printf("\n%d songs (%f seconds) compressed to %d bytes (%f bytes/second)\n",
           currentSong, totalTime, outputPos, int(outputPos / totalTime * 1000 + 0.5) / 1000.0);

    // deepdive blows these stats away cause we don't keep all the run data across every combination
    if ((verbose) && (!doDeepDive)) {
        printf("  %d RLE encoded sequences, avg length %d\n", minRunData[MAXRUN].cntRLEs.cnt, minRunData[MAXRUN].cntRLEs.avg(3));
        printf("  %d RLE16 encoded sequences, avg length %d\n", minRunData[MAXRUN].cntRLE16s.cnt, minRunData[MAXRUN].cntRLE16s.avg(2));
        printf("  %d RLE24 encoded sequences, avg length %d\n", minRunData[MAXRUN].cntRLE24s.cnt, minRunData[MAXRUN].cntRLE24s.avg(2));
        printf("  %d RLE32 encoded sequences, avg length %d\n", minRunData[MAXRUN].cntRLE32s.cnt, minRunData[MAXRUN].cntRLE32s.avg(2));
        printf("  %d backref sequences, avg length %d\n", minRunData[MAXRUN].cntBacks.cnt, minRunData[MAXRUN].cntBacks.avg(4));
        printf("  %d inline sequences, avg length %d\n", minRunData[MAXRUN].cntInlines.cnt, minRunData[MAXRUN].cntInlines.avg(1));
        printf("  %d fwdref inlines, avg length %d\n", minRunData[MAXRUN].cntFwds.cnt, minRunData[MAXRUN].cntFwds.avg(1));
    }

    // write out the final file
    {
        FILE *fp = fopen(szFileOut, "wb");
        if (NULL == fp) {
            printf("Failed to open output file '%s', code %d\n", szFileOut, errno);
            return 1;
        }
        fwrite(outputBuffer, 1, outputPos, fp);
        fclose(fp);
    }

    printf("\n** DONE **\n");

    return 0;
}


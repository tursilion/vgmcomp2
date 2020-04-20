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


#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOISE_TRIGGER  0x10000              // input version of noise trigger bit
#define NOISE_OUT_TRIGGER 0x80              // output version of noise trigger bit

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
} songs[MAXSONGS];

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
bool isPSG=false, isAY=false;
bool verbose = false;
bool debug = false;
int noiseMask;
int minRun = 0;

// stats for minrun loops
#define MAXRUN 21
struct {
    int cntRLEs;
    int cntRLE16s;
    int cntBacks;
    int cntInlines;
} minRunData[MAXRUN+1];
unsigned char bestRunBuffer[65536];   // this is a little wasteful, but that's okay

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
                    s->VGMDAT[chan][cnt] &= noiseMask|NOISE_OUT_TRIGGER;    // limit input
                } else {
                    s->VGMDAT[chan][cnt] &= noiseMask;    // limit input
                }
                s->VGMVOL[chan][cnt] &= 0xff;
            } else {
                s->VGMDAT[chan][cnt] &= noiseMask;    // limit input
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

// return how many repeated ints exist at the passed in location, no more than cnt ints exist
// does NOT include the first one!
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

    return out;
}

// return how many repeated two-int cases exist at the passed in location, no more than cnt ints exist
// does NOT include the first one!
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

    return out;
}

// string search a stream for all full and partial string matches
// - the source string is str and is strLen ints long
// - the search space is buf and is bufLen ints long
// - bestLen holds the longest match found
// - return value is the total of all matches (full and partial) found
// Note that buf is allowed to overlap str, and we should deal with that
int stringSearch(int *str, int strLen, int *buf, int bufLen, int &bestLen) {
    int totalOut = 0;
    bestLen = 0;

    // matches need to be at least 4 bytes long
    for (int x=0; x<bufLen; ++x) {
        // calculate a working length
        int tmpLen = strLen;
        if (&buf[x] < (str+strLen)) {
            tmpLen = &buf[x]-str;
        }
        if (x+tmpLen >= bufLen) continue;

        for (int cnt = 4; cnt < tmpLen; ++cnt) {
            if (0 == memcmp(str, &buf[x], cnt)) {   // matching ints to ints, with byte padding, should always work
                bestLen = cnt;
                totalOut += cnt;
            }
        }
    }

    return totalOut;
}

// string search a char memory buffer for all full and partial string matches
// - the source string is str and is strLen ints long (and assumed to hold bytes in ints!)
// - the search space is buf and is bufLen ints long (and assumed to be bytes!)
// - bestLen holds the longest match found
// - bestPos holds the offet (from buf) of the best match
// - return value is the total of all matches (full and partial) found
int memorySearch(int *str, int strLen, unsigned char *buf, int bufLen, int &bestLen, int &bestPos) {
    int totalOut = 0;
    bestLen = 0;

    // matches need to be at least 4 bytes long
    for (int cnt = 4; cnt < strLen; ++cnt) {
        for (int x=0; x<bufLen-cnt; ++x) {
            // matching different data types, we have to comp by hand
            bool match = true;
            for (int idx = 0; idx<cnt; ++idx) {
                if ((str[idx]&0xff) != buf[idx+x]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                bestLen = cnt;
                bestPos = x;
                totalOut += cnt-3;  // cost of a backreference is 3
            }
        }
    }

    return totalOut;
}


// actually compress a single stream into the output buffer
// outputBuffer[64*1024] and outputPos
bool compressStream(int song, int st) {
    int *str = songs[song].outStream[st];
    int cnt = songs[song].streamCnt[st];

    // tuning as noted below. You can't change this as the unpacker hard codes it too,
    // but having this information provides clarity. These are the 1 byte saved thresholds.
    const int RLECOST=3;        // AAA -> xA    0-63 means 3 to 66 times
    const int RLE16COST=2;      // ABAB -> xAB  0-63 means 2 to 65 times
    const int BACKREFCOST=4;    // 1234 -> xOO  0-63 means 1 to 64 bytes

    // this are the bitmasks for encoding
    const int BACKREFBITS = 0xc0;   // 11
    const int RLE16BITS   = 0x80;   // 10
    const int RLEBITS     = 0x40;   // 01
    const int INLINEBITS  = 0x00;   // 00

    // maximum size
    const int MAXSIZE = 63;         // 00111111

    int dbg = 0;

    while (cnt > 0) {
        if (verbose) {
            if (dbg++ % 100 == 0) {
                printf("  %5d rows (%2d passes) remain...      \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b", cnt, MAXRUN-minRun);
            }
        }
        // First, check if this is a single-byte RLE run (must be at least 3 bytes, 0-63 means 3-66 times)
        int x = RLEsize(str, cnt);
        if (x >= 2) {
            // we need 2 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-2) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }

            // '2' means this one, plus 2 more, so that's three. We encode that as zero. There's also an upper limit.
            x -= 2;
            if (x > MAXSIZE) x=MAXSIZE;   // that's all the bits we have
            outputBuffer[outputPos++] = RLEBITS | x;
            outputBuffer[outputPos++] = (*str) & 0xff;

            // update the input position - careful of the offset tweaking above
            str += x+3;
            cnt -= x+3;
            ++minRunData[minRun].cntRLEs;
            continue;
        }

        // Second, check if this is a 16-bit RLE run (must be at least twice, 0-63 means 2-65 times)
        x = RLE16size(str, cnt);
        if (x >= 1) {
            // we need 3 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-3) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }

            // '1' means this one, plus 1 more, so that's two. We encode that as zero. There's also an upper limit.
            x -= 1;
            if (x > MAXSIZE) x=MAXSIZE;   // that's all the bits we have
            outputBuffer[outputPos++] = RLE16BITS | x;
            outputBuffer[outputPos++] = (*(str)) & 0xff;
            outputBuffer[outputPos++] = (*(str+1)) & 0xff;

            // update the input position - careful of the offset tweaking above
            str += (x+2)*2;
            cnt -= (x+2)*2;
            ++minRunData[minRun].cntRLE16s;
            continue;
        }

        // The optimization of string searches makes or breaks this algorithm.

        // Find the ending point of this string (must be at least 1 byte, 0-63 means 1-64, or stop at first matching RLE)
        int *endpoint = str+(cnt < 64 ? cnt : 64);        // maximum possible run
        for (int idx=1; idx<64; ++idx) {
            if ((RLEsize(str+idx, cnt-idx) >= 2) || (RLE16size(str+idx, cnt-idx) >= 1)) {
                endpoint = str + idx;
                break;
            }
        }
        int thisLen = endpoint - str;
        // now we have a string from str to endpoint (non-inclusive), length of thisLen

        // Search forward and see how many times this string or any subset of it matches, save the best hit (minimum 4 bytes) AND the total
        int bestFwd = 0;
        int bestBwd = 0;
        int bestBwdPos = 0;

        // note that we include the region INSIDE the current string, just in case!
        int off = thisLen > 8 ? 8: thisLen;
        int fwdSearch = stringSearch(str, thisLen, str+off, cnt-off, bestFwd);

        // now check the remaining streams for forward refs
        for (int sng=song; sng<currentSong; ++sng) {
            for (int stx=(sng==song?st+1:0); stx<MAXSTREAMS; ++stx) {
                int altbest = 0;
                int altSearch = stringSearch(str, thisLen, songs[sng].outStream[stx], songs[sng].streamCnt[stx], altbest);
                if ((altSearch > fwdSearch) && (altbest >= bestFwd)) {
                    fwdSearch = altSearch;
                    bestFwd = altbest;
                }
            }
        }

        // Search backward and see if this string or any subset matches, save the best hit (minimum 4 bytes)
        // Don't need to save the total, it doesn't help us here.
        memorySearch(str, thisLen, outputBuffer, outputPos, bestBwd, bestBwdPos);

        // fwdSearch contains the expected bytes saved (minus the backreference costs) if we save bestFwd bytes
        // as a string. bestBwd contains the best backwards match for this string.
        // if bestBwd >= bestFwd, then we always take the back reference (because the forward references will
        // also find it). Otherwise, fwdSearch should be greater than bestBw-3 (which is our backref savings).
        // This still isn't necessarily perfect, since the later matches would still find parts of the backref,
        // but in theory it should turn out nicely. It might be a place to try a tuning parameter and go
        // for some level of additional slack...
        bool isBackref = false;
        if ((bestBwd >= 4+minRun) && (bestFwd >= 4+minRun)) {
            // if we found both, decide which one to use
            if (bestBwd >= bestFwd) {
                // the later refs can use the same backref we found
                isBackref = true;
            } else if (fwdSearch > bestBwd-3) {
                // we are probably the best case for those later matches
                isBackref = false;
            } else {
                // we should just take the backref
                isBackref = true;
            }
        } else if (bestBwd >= 4+minRun) {
            // we have backward but no forward
            isBackref = true;
        } else if (bestFwd < 4+minRun) {
            // we have nothing, not even a forward, so fake it
            bestFwd = thisLen;
        }

        if (isBackref) {
            // we need 3 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-3) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }

            // 4 is the smallest legal length, so map it down
            bestBwd -= 4;
            if (bestBwd > MAXSIZE) bestBwd=MAXSIZE;   // that's all the bits we have
            outputBuffer[outputPos++] = BACKREFBITS | bestBwd;
            outputBuffer[outputPos++] = bestBwdPos / 256;
            outputBuffer[outputPos++] = bestBwdPos % 256;

            // update the input position - careful of the offset tweaking above
            str += bestBwd+4;
            cnt -= bestBwd+4;
            ++minRunData[minRun].cntBacks;
            continue;
        } else {
            // inline string - 1 + length bytes space to encode
            if (outputPos >= (signed)sizeof(outputBuffer)-bestFwd-1) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }

            // 1 is the smallest legal length, so map it down
            bestFwd -= 1;
            if (bestFwd > MAXSIZE) bestFwd=MAXSIZE;   // that's all the bits we have
            outputBuffer[outputPos++] = INLINEBITS | bestFwd;
            for (int idx=0; idx<=bestFwd; ++idx) {
                outputBuffer[outputPos++] = ((*(str++)) & 0xff);
                --cnt;
            }

            // Already updated position and cnt above
            ++minRunData[minRun].cntInlines;
            continue;
        }
    }

    if (verbose) {
        printf("                                        \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    }
    return true;
}

// these are stored in noiseMask instead
#define NOISE_MASK     0x0000F              // noise should be pre-converted to target
#define NOISE_MASK_AY  0x0001F              // AY has larger range

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
                noiseMask = NOISE_MASK;
                if (isAY) {
                    printf("Can't select both PSG and AY options.\n");
                    return 1;
                }
            } else if (0 == strcmp(argv[idx], "-ay")) {
                isAY=true;
                noiseMask = NOISE_MASK_AY;
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
                if (isPSG) note &= 0x3ff;   // 10 bit PSG
                if (isAY) note &= 0xfff;    // 12 bit AY
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
    if (verbose) printf("Songbank contains %d/%d notes.\n", noteCnt, MAXNOTES);

    // do the RLE pack. It's necessary because it allows us to have RLE in the post-encoded stream
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
    minRunData[MAXRUN].cntBacks = 0;
    minRunData[MAXRUN].cntInlines = 0;
    minRunData[MAXRUN].cntRLE16s = 0;
    minRunData[MAXRUN].cntRLEs = 0;

    // Individually compress each of the 9 channels (all treated the same)
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            if (verbose) {
                printf("Working on song %d, stream %d...", song, st);
            }
            songs[song].streamOffset[st] = outputPos;

#if 1
            // it appears that the minRun changing is still worth it, and that it needs
            // to be per stream. In addition, it appears that the full range is still
            // useful, as my first test case had bests from 0 to 13 in it. Over 400 bytes
            // were saved doing this, so gonna say worth it. Minruns /might/ also help
            // order matter less (based on just one test, mind you)
            int bestRun = 0;
            int bestSize = 65536;

            for (minRun = 0; minRun < MAXRUN; ++minRun) {
                minRunData[minRun].cntBacks = 0;
                minRunData[minRun].cntInlines = 0;
                minRunData[minRun].cntRLE16s = 0;
                minRunData[minRun].cntRLEs = 0;

                outputPos = songs[song].streamOffset[st];

                if (!compressStream(song, st)) {
                    return 1;
                }

                if (outputPos-songs[song].streamOffset[st] < bestSize) {
                    bestSize = outputPos-songs[song].streamOffset[st];
                    bestRun = minRun;
                    memcpy(bestRunBuffer, &outputBuffer[songs[song].streamOffset[st]], bestSize);
                }
            }

            // reload the best one...
            printf("%4d bytes at minrun %d\n", bestSize, bestRun);
            memcpy(&outputBuffer[songs[song].streamOffset[st]], bestRunBuffer, bestSize);
            outputPos = songs[song].streamOffset[st] + bestSize;
            // update totals
            minRunData[MAXRUN].cntBacks += minRunData[bestRun].cntBacks;
            minRunData[MAXRUN].cntInlines += minRunData[bestRun].cntInlines;
            minRunData[MAXRUN].cntRLE16s += minRunData[bestRun].cntRLE16s;
            minRunData[MAXRUN].cntRLEs += minRunData[bestRun].cntRLEs;
#else
            if (!compressStream(song, st)) {
                return 1;
            }
            if (verbose) {
                printf("%d bytes\n", outputPos-songs[song].streamOffset[st]);
            }
#endif
        }
    }


    // write out the tables - the stream table is first
    outputBuffer[STREAMTABLE]   = outputPos/256; 
    outputBuffer[STREAMTABLE+1] = outputPos%256;
    for (int song=0; song<currentSong; ++song) {
        for (int st=0; st<MAXSTREAMS; ++st) {
            // we need 2 bytes space to encode
            if (outputPos >= sizeof(outputBuffer)-2) {
                printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
                return false;
            }
            outputBuffer[outputPos++] = songs[song].streamOffset[st] / 256;
            outputBuffer[outputPos++] = songs[song].streamOffset[st] % 256;
        }
    }

    // then the note table
    outputBuffer[NOTETABLE]   = outputPos/256; 
    outputBuffer[NOTETABLE+1] = outputPos%256;
    for (int nt=0; nt<noteCnt; ++nt) {
        // we need 2 bytes space to encode
        if (outputPos >= sizeof(outputBuffer)-2) {
            printf("Ran out of room in outputBuffer (maximum 64k!) Encode failed.\n");
            return false;
        }
        outputBuffer[outputPos++] = noteTable[nt] / 256;
        outputBuffer[outputPos++] = noteTable[nt] % 256;
    }

    // emit some stats
    int totalRows = 0;
    for (int idx=0; idx<currentSong; ++idx) {
        totalRows += songs[idx].outCnt;
    }
    double totalTime = int(totalRows / 60.0 * 100 + 0.5) / 100.0;
    printf("%d songs (%f seconds) compressed to %d bytes (%f bytes/second)\n",
           currentSong, totalTime, outputPos, int(outputPos / totalTime * 1000 + 0.5) / 1000.0);

    if (verbose) {
        printf("  %d RLE encoded sequences\n", minRunData[MAXRUN].cntRLEs);
        printf("  %d RLE16 encoded sequences\n", minRunData[MAXRUN].cntRLE16s);
        printf("  %d backref sequences\n", minRunData[MAXRUN].cntBacks);
        printf("  %d inline sequences\n", minRunData[MAXRUN].cntInlines);
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

    printf("** DONE **\n");

    return 0;
}


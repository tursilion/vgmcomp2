// testPlayPSG.cpp : This program reads in the PSG data files and plays them
// so you can hear what it currently sounds like.

// We need to read in the channels, and there can be any number of channels.
// They have the following naming schemes:
// voice channels are <name>_tonXX.60hz
// noise channels are <name>_noiXX.60hz
// simple ASCII format, values stored as hex (but import should support decimal!), row number is implied frame

// TODO: the "60hz" can also be "50hz", "30hz" or "25hz". We want to support all of those here.
// We also want the program to be able to /find/ the channels given a prefix, or
// be given specific channel numbers.

#include "stdafx.h"
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <cmath>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include "sound.h"

extern LPDIRECTSOUNDBUFFER soundbuf;		// sound chip audio buffer
struct StreamData soundDat;

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
#define MAXHEATMAP 45
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
unsigned int VGMADR[MAXTICKS][MAXHEATMAP];  // a scaled address, always range 0-65535, for heatmap (max read is 4 bytes times 9 streams - 45 bytes (ouch))
bool isNoise[MAXCHANNELS];

bool shownotes = true;                      // whether to scroll the notes
bool heatmap = false;                       // show a heatmap for SCF files (forces hidenotes)
char NoteTable[4096][4];                    // note names - we'll just lookup the whole range
bool testay = false;
bool testpsg = false;

// heatmap statistics - gives an average bytes per decode (ignoring frames with no decode)
int thisHeatMap;
int totalHeatMap;
int cntHeatMap;
int smallestHeatMap = 9999;
int biggestHeatMap = 0;

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
};

// structure for unpacking a stream of data
struct STREAM {
    int mainPtr;    // the main index in the decompression. If 0, we are done.
    int curPtr;     // where are are currently getting data from
    int curBytes;   // current bytes left
    // post compression data
    int framesLeft; // number of frames left on the current RLE (not used for tone channels)
    int curType;    // the mask of the type
};
#define TYPEBACKREF 0xc0
#define TYPEBACKREF2 0xe0
#define TYPERLE16 0x80
#define TYPERLE24 0xa0
#define TYPERLE 0x40
#define TYPERLE32 0x60
#define TYPEINLINE 0x00
#define TYPEINLINE2 0x20

// input: 8 bit unsigned audio (centers on 128)
// output: 15 (silent) to 0 (max)
int mapVolume(int nTmp) {
	int nBest = -1;
	int nDistance = INT_MAX;
	int idx;

	// testing says this is marginally better than just (nTmp>>4)
	for (idx=0; idx < 16; idx++) {
		if (abs(volumeTable[idx]-nTmp) < nDistance) {
			nBest = idx;
			nDistance = abs(volumeTable[idx]-nTmp);
		}
	}

    // don't return mute unless the input was mute
    if ((nBest == 15) && (nTmp != 0)) --nBest;

	// return the index of the best match
	return nBest;
}

void buildNoteTable() {
    // note scales are confusing. ;) But anyway, we'll start at
    // A0 in the tempered scale which is 27.5 hz, or 4068 counts.
    // That's the lowest this protocol supports (and is far lower
    // than the TI can manage.)
    static const char *names[12] = { "C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-" };
    int scale = 0;  // scale increments at C
    int namepos = 9;    // A
    double currentNote = 27.5;  // A

    // so first init the table blank
    for (int idx=0; idx<sizeof(NoteTable)/sizeof(NoteTable[0]); ++idx) {
        strcpy(NoteTable[idx],"---");
    }

    // okay! Now we build, increasing frequency until it is under 20
    int lastPer = 4095;
    for (;;) {
        int period = int(111860.8/currentNote+0.5);
        if (period < 20) break;     // no longer tuned around here
        int split = (lastPer-period) / 2;
        for (int run = period-split; run<period+split; ++run) {
            sprintf(NoteTable[run], "%s%d", names[namepos], scale);
        }
        ++namepos;
        if (namepos > 11) {
            namepos = 0;
            ++scale;
        }
        currentNote = currentNote * pow(2, 1.0/12.0);
        lastPer = period;
    }
    // close enough...
}

// MUST return a 3-character string
const char *getNoteStr(int note, int vol) {
    if (vol == 0) return "---";
    return NoteTable[note&0xfff];
}

// pass in file pointer (will be reset), channel index, last row count, first input column (from 0), and true if noise
// set scalevol to true to scale PSG volumes back to 8-bit linear
bool loadDataCSV(FILE *fp, int &chan, int &cnt, int column, bool noise, bool scalevol) {
    // up to 8 columns allowed
    int in[8];
    
    // remember last
    int oldcnt = cnt;

    // get total size for heatmap
    fseek(fp, 0, SEEK_END);
    int maxsize=ftell(fp);

    // back to start of file
    fseek(fp, 0, SEEK_SET);

    cnt = 0;
    while (!feof(fp)) {
        // just to put something in there, scale to 0-65535
        for (int idx=1; idx<MAXHEATMAP; ++idx) {
            VGMADR[cnt][idx] = 0;
        }
        // have to divide first to avoid 32-bit int overflow
        VGMADR[cnt][chan] = (int)(((double)ftell(fp) / maxsize) * 65535);

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
            VGMDAT[chan][cnt] = in[column];
            if (scalevol) {
                VGMVOL[chan][cnt] = volumeTable[in[column+1]&0x0f];
            } else {
                VGMVOL[chan][cnt] = in[column+1];
            }

            if (noise) {
                VGMDAT[chan][cnt] &= NOISE_MASK|NOISE_TRIGGER|NOISE_PERIODIC;    // limit input
                VGMVOL[chan][cnt] &= 0xff;
                if (scalevol) {
                    // the noise needs to be converted back to a shift rate too
                    int mask = VGMDAT[chan][cnt] & NOISE_TRIGGER;
                    int periodic = (VGMDAT[chan][cnt] & 0x4) == 0;
                    int type = VGMDAT[chan][cnt] & 3;
                    switch (type) {
                        case 0: type = 0x10; break;
                        case 1: type = 0x20; break;
                        case 2: type = 0x40; break;
                        case 3: if (chan > 0) {type = VGMDAT[chan-1][cnt]&0x3ff;} break;  // works if you aren't being weird
                    }
                    VGMDAT[chan][cnt] = mask | (periodic?NOISE_PERIODIC:0) | type;
                }
            } else {
                VGMDAT[chan][cnt] &= NOISE_MASK;    // limit input
                VGMVOL[chan][cnt] &= 0xff;
            }
            ++cnt;
            if (cnt >= MAXTICKS) {
                printf("(maximum tick count reached) ");
                break;
            }
        }
    }
    printf("read %d lines\n", cnt);
    isNoise[chan] = noise;
    registerChan(chan, noise);
    ++chan;

    if ((oldcnt > 0) && (oldcnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", oldcnt, cnt);
    }

    return true;
}

// extract a byte from the buffer, range check it, and
// add an address to the heatmap adr table (not called in realtime code)
unsigned char getBufferByte(int cnt, unsigned char *buf, int adr, int maxbytes) {
    int idx;

    // either of these tripping likely indicates a bug or a corrupt file...
    if (adr < 0) {
        printf("* bad SCF unpack address of %d (max %d)\n", adr, maxbytes);
        exit(0);
    }
    if (adr >= maxbytes) {
        printf("* bad SCF unpack address of %d (max %d)\n", adr, maxbytes);
        exit(0);
    }

    // calculate the scaled address (0-65535)
    unsigned int z = (unsigned int)(((double)adr / maxbytes) * 65535 + 0.5);

    // find the first free data slot for this frame
    for (idx=0; idx<MAXHEATMAP; ++idx) {
        if (VGMADR[cnt][idx] == 0) {
            VGMADR[cnt][idx] = (int)z;
            break;
        }
    }

    // if there's no room, we just ignore it
    // this is surprisingly useful to provide some end of song statistics on the packing...
    // we just save the largest value stored - gives us an idea of bytes this frame
    thisHeatMap = idx+1;

    // either way, return the data requested
    return buf[adr];
}

// unpack a stream byte - offset and maxbytes are used to write a scaled
// address for the heatmap to display later
// cnt is row count, and maxbytes is used for scaling, max size of data
int getCompressedByte(STREAM *str, unsigned char *buf, int cnt, int maxbytes) {
    // bytes left in the current stream?
    if (str->curBytes > 0) {
        --str->curBytes;

        switch(str->curType) {
        case TYPEINLINE:
        case TYPEINLINE2:
        case TYPEBACKREF:
        case TYPEBACKREF2:
            // just pull a string of bytes
            return getBufferByte(cnt, buf, str->curPtr++, maxbytes);

        case TYPERLE32:
            // pull the last four bytes over and over
            // mainPtr is assumed already incremented
            if (str->curPtr == str->mainPtr) {
                str->curPtr -= 4;
            }
            return getBufferByte(cnt, buf, str->curPtr++, maxbytes);

        case TYPERLE24:
            // pull the last three bytes over and over
            // mainPtr is assumed already incremented
            if (str->curPtr == str->mainPtr) {
                str->curPtr -= 3;
            }
            return getBufferByte(cnt, buf, str->curPtr++, maxbytes);

        case TYPERLE16:
            // pull the last two bytes over and over
            // mainPtr is assumed already incremented
            if (str->curPtr == str->mainPtr) {
                str->curPtr -= 2;
            }
            return getBufferByte(cnt, buf, str->curPtr++, maxbytes);

        case TYPERLE:
            // pull the single byte - no increment
            return getBufferByte(cnt, buf, str->curPtr, maxbytes);
        }
    }

    // start a new stream
    int x = getBufferByte(cnt, buf, str->mainPtr++, maxbytes);
    str->curType = x&0xe0;
    switch (str->curType) {
    case TYPEBACKREF:  // long back reference
    case TYPEBACKREF2:
        {
            int x1 = getBufferByte(cnt, buf, str->mainPtr, maxbytes);
            int x2 = getBufferByte(cnt, buf, str->mainPtr+1, maxbytes);
            str->curPtr = x1*256 + x2;
            // check for end of stream
            if (str->curPtr == 0) {
                str->mainPtr = 0;
                return 0;
            }
            str->mainPtr += 2;
            str->curBytes = (x&0x3f) + 4;
        }
        break;
        
    case TYPERLE32:  // RLE-32
        str->curPtr = str->mainPtr;
        str->mainPtr += 4;
        str->curBytes = ((x&0x1f) + 2)*4;
        break;

    case TYPERLE24:  // RLE-24
        str->curPtr = str->mainPtr;
        str->mainPtr += 3;
        str->curBytes = ((x&0x1f) + 2)*3;
        break;

    case TYPERLE16:  // RLE-16
        str->curPtr = str->mainPtr;
        str->mainPtr += 2;
        str->curBytes = ((x&0x1f) + 2)*2;
        break;

    case TYPERLE:  // RLE
        str->curPtr = str->mainPtr;
        str->mainPtr++;
        str->curBytes = (x&0x1f) + 3;
        break;

    case TYPEINLINE:  // inline
    case TYPEINLINE2:
        str->curPtr = str->mainPtr;
        str->curBytes = (x&0x3f) + 1;
        str->mainPtr += str->curBytes;
        break;
    }

    // recurse call to get the next byte of data
    return getCompressedByte(str, buf, cnt, maxbytes);
}

int tonetable(unsigned char *buf, int toneoffset, int y) {
    if (testay) {
        return buf[toneoffset+y*2] + (buf[toneoffset+y*2+1]&0xf)*256;
    } else {
        // must be PSG
        return (buf[toneoffset+y*2]&0xf) + buf[toneoffset+y*2+1]*16;
    }
}


// import a fully featured SCF file
// fp: file pointer at beginning of data structure
// chan - first channel to import into (must be updated on exit)
// cnt - count of rows imported (checked and then updated)
// scfsong - which song to import - must be already verified
bool importSCF(FILE *fp, int &chan, int &cnt, int scfsong) {
    // 4 tone, 4 vol, 1 time
    struct STREAM strDat[9];
    // output data
    int curFreq[4];
    int curVol[4];
    bool customNoise = false;

    // a place to store the file
    unsigned char buf[64*1024];

    // zero the heatmap statistics
    totalHeatMap = 0;
    thisHeatMap = 0;
    cntHeatMap = 0;
    smallestHeatMap = 9999;
    biggestHeatMap = 0;

    // remember last - this is just to report a warning if the
    // line count is at all different
    int oldcnt = cnt;

    // back to start of file and suck it in
    fseek(fp, 0, SEEK_SET);
    int maxbytes = fread(buf, 1, sizeof(buf), fp);

    // new row count
    cnt = 0;

    // get the offsets to the tables we need to unpack
    int streamoffset=buf[0]*256+buf[1];
    int toneoffset=buf[2]*256+buf[3];

    // Checking for valid song here rather than externally cause it's safer
    if (streamoffset+18*scfsong >= toneoffset) {
        printf("Invalid song %d - SCF contains only %d song(s).\n", scfsong, (toneoffset-streamoffset)/18);
        return 1;
    }

    // point to the first stream of 9
    streamoffset+=18*scfsong;

    for (int idx=0; idx<9; ++idx) {
        strDat[idx].mainPtr = buf[streamoffset+idx*2]*256+buf[streamoffset+idx*2+1];
        strDat[idx].curPtr = 0;
        strDat[idx].curBytes = 0;
        strDat[idx].curType = 0xff;
        strDat[idx].framesLeft = 0;
    }

    // register our four channels
    for (int idx=0; idx<3; ++idx) {
        isNoise[chan+idx] = false;
        registerChan(chan+idx, false);
    }
    isNoise[chan+3] = true;
    registerChan(chan+3, true);

    /// default settings
    for (int idx=0; idx<4; ++idx) {
        curFreq[idx] = 1;
        curVol[idx] = 0;
    }

    bool done = false;
    while (!done) {
        // run until we have no more data
        done = true;
        // by default, this tick reads NO data
        for (int idx=0; idx<MAXHEATMAP; ++idx) {
            VGMADR[cnt][idx] = 0;
        }

        if (strDat[8].mainPtr != 0) {
            // check the timestream
            if (strDat[8].framesLeft) {
                --strDat[8].framesLeft;
                done = false;
            } else {
                // timestream data
                int x = getCompressedByte(&strDat[8], buf, cnt, maxbytes);
                if (strDat[8].mainPtr) {
                    done = false;
                    // song not over, x is valid
                    strDat[8].framesLeft = x & 0xf;
                    if (x&0x80) {
                        // voice 0
                        if (strDat[0].mainPtr) {
                            int y = getCompressedByte(&strDat[0], buf, cnt, maxbytes);
                            if (strDat[0].mainPtr) {
                                // look up frequency table
                                curFreq[0] = tonetable(buf, toneoffset, y);
                            } else {
                                curFreq[0] = 1;
                            }
                        }
                    }
                    if (x&0x40) {
                        // voice 1
                        if (strDat[1].mainPtr) {
                            int y = getCompressedByte(&strDat[1], buf, cnt, maxbytes);
                            if (strDat[1].mainPtr) {
                                // look up frequency table
                                curFreq[1] = tonetable(buf, toneoffset, y);
                            } else {
                                curFreq[0] = 1;
                            }
                        }
                    }
                    if (x&0x20) {
                        // voice 2
                        if (strDat[2].mainPtr) {
                            int y = getCompressedByte(&strDat[2], buf, cnt, maxbytes);
                            if (strDat[2].mainPtr) {
                                // look up frequency table
                                curFreq[2] = tonetable(buf, toneoffset, y);
                                // update noise if needed
                                if (customNoise) {
                                    curFreq[3] = curFreq[2];
                                }
                            } else {
                                curFreq[0] = 1;
                            }
                        }
                    }
                    if (x&0x10) {
                        // noise
                        if (strDat[3].mainPtr) {
                            int y = getCompressedByte(&strDat[3], buf, cnt, maxbytes);
                            if (strDat[3].mainPtr) {
                                if (testay) {
                                    // AY - frequency is frequency
                                    curFreq[3] = y;
                                } else {
                                    // PSG - map command to frequency
                                    customNoise = false;
                                    switch (y&0x03) {
                                    case 0: curFreq[3]=0x10; break;
                                    case 1: curFreq[3]=0x20; break;
                                    case 2: curFreq[3]=0x40; break;
                                    case 3: curFreq[3]=curFreq[2]; customNoise = true; break;
                                    }
                                    if (0==(y&0x04)) curFreq[3] |= NOISE_PERIODIC;
                                    curFreq[3] |= NOISE_TRIGGER;
                                }
                            }
                        }
                    }
                }
            }
        }

        // now handle the volume streams
        for (int str=4; str<8; ++str) {
            if (strDat[str].mainPtr != 0) {
                // check the RLE
                if (strDat[str].framesLeft) {
                    --strDat[str].framesLeft;
                    done = false;
                } else {
                    int x = getCompressedByte(&strDat[str], buf, cnt, maxbytes);
                    if (strDat[str].mainPtr) {
                        done = false;
                        strDat[str].framesLeft = x&0xf;
                        curVol[str-4] = volumeTable[(x>>4)&0xf];
                    } else {
                        curVol[str-4] = volumeTable[15];
                    }
                }
            }
        }

        // copy out the data
        for (int idx=0; idx<4; ++idx) {
            VGMDAT[chan+idx][cnt] = curFreq[idx];
            VGMVOL[chan+idx][cnt] = curVol[idx];
        }
        // clear the trigger flag
        curFreq[3] &= ~NOISE_TRIGGER;

        // update statistics from the heat map
        if (thisHeatMap != 0) {
            if (thisHeatMap > biggestHeatMap) biggestHeatMap = thisHeatMap;
            if (thisHeatMap < smallestHeatMap) smallestHeatMap = thisHeatMap;
            totalHeatMap += thisHeatMap;
            thisHeatMap = 0;
            ++cntHeatMap;
        }
        
        // next row
        ++cnt;
    }

    printf("Read %d lines\n", cnt);
    chan += 4;  // return 4 channels loaded

    printf("Number of lines with data: %d\n", cntHeatMap);
    printf("Largest data line: %d bytes\n", biggestHeatMap);
    printf("Smallest data line: %d bytes\n", smallestHeatMap);
    if (cntHeatMap > 0) {
        printf("Average data line: %d\n", (totalHeatMap+(cntHeatMap/2))/cntHeatMap);
    }

    if ((oldcnt > 0) && (oldcnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", oldcnt, cnt);
    }

    return true;
}

void updateVolume(int chan, int cnt) {
    // verifying the volume while it's 8-bit linear is a bit much, so instead
    // we'll reproduce the mapping step that prepare would do, and so lock
    // the volume into it's final values for better playback
    // We'll only output if we make any changes
    bool debug = false;

    for (int row=0; row<cnt; ++row) {
        for (int ch=0; ch<chan; ++ch) {
            int mappedvol = mapVolume(VGMVOL[ch][row]);
            int newvol = volumeTable[mappedvol];
            if (newvol != VGMVOL[ch][row]) {
                if (!debug) {
                    debug=true;
                    printf("Mapping volume to chip's range...\n");
                }
                VGMVOL[ch][row] = newvol;
            }
        }
    }
}

bool testAYData(int chan, int cnt) {
    // must be 4 channels or less, must be only 1 noise channel,
    // must be within frequency ranges and noise volume must
    // exactly match one of the tone channels
    if (chan > 4) {
        printf("Too many channels for AY8910 (%d channels found)\n", chan);
        return false;
    }

    int x = 0;
    for (int idx=0; idx<chan; ++idx) {
        if (!isNoise[idx]) ++x;
    }
    if (x > 3) {
        printf("Too many tone channels for AY8910 (%d tone channels found)\n", x);
        return false;
    }

    x = 0;
    for (int idx=0; idx<chan; ++idx) {
        if (isNoise[idx]) ++x;
    }
    if (x > 1) {
        printf("Too many noise channels for AY8910 (%d noise channels found)\n", x);
        return false;
    }

    // test the song itself
    for (int row=0; row<cnt; ++row) {
        for (int ch=0; ch<chan; ++ch) {
            if (isNoise[ch]) {
                // noise frequency must be 0x00-0x1f and volume must match a tone channel's
                if ((VGMDAT[ch][row] < 0) || ((VGMDAT[ch][row]&NOISE_MASK) > 0x1f)) {
                    printf("Noise frequency out of range for AY on row %d - got 0x%02X\n", row, VGMDAT[ch][row]);
                    return false;
                }
                bool match = false;
                for (int c2=0; c2<chan; ++c2) {
                    if (c2 == ch) continue;
                    if (VGMVOL[c2][row] == VGMVOL[ch][row]) {
                        match=true;
                        break;
                    }
                }
                if (!match) {
                    printf("Noise volume has no matching channel for AY on row %d. Noise volume 0x%X, Tones", row, VGMVOL[ch][row]);
                    for (int c2=0; c2<chan; ++c2) {
                        if (c2 == ch) continue;
                        printf(" 0x%X", VGMVOL[c2][row]);
                    }
                    printf("\n");
                    return false;
                }
            } else {
                // tone frequency must be 0x000-0xFFF. We don't do any volume tests, as at this step the full 8-bit range is legal.
                if ((VGMDAT[ch][row] < 0) || (VGMDAT[ch][row] > 0xfff)) {
                    printf("Tone frequency out of range for AY (0-0xfff) on row %d - got 0x%03X\n", row, VGMDAT[ch][row]);
                    return false;
                }
            }
        }
    }

    updateVolume(chan, cnt);

    return true;
}
bool testPSGData(int chan, int cnt) {
    // must be 4 channels or less, must be only 1 noise channel,
    // must be within frequency ranges and noise frequency must
    // match either a fixed rate or literally channel 2 (0-based)
    if (chan > 4) {
        printf("Too many channels for PSG (%d channels found)\n", chan);
        return false;
    }

    int x = 0;
    for (int idx=0; idx<chan; ++idx) {
        if (!isNoise[idx]) ++x;
    }
    if (x > 3) {
        printf("Too many tone channels for PSG (%d tone channels found)\n", x);
        return false;
    }

    x = 0;
    for (int idx=0; idx<chan; ++idx) {
        if (isNoise[idx]) ++x;
    }
    if (x > 1) {
        printf("Too many noise channels for PSG (%d noise channels found)\n", x);
        return false;
    }

    // test the song itself
    for (int row=0; row<cnt; ++row) {
        for (int ch=0; ch<chan; ++ch) {
            if (isNoise[ch]) {
                // noise frequency must be 0x000-0x3ff, and exactly match either a fixed
                // rate or literally channel 2. Volume is unrestricted
                if ((VGMDAT[ch][row] < 0) || ((VGMDAT[ch][row]&NOISE_MASK) > 0x3ff)) {
                    printf("Noise frequency out of range for PSG on row %d - got 0x%02X\n", row, VGMDAT[ch][row]);
                    return false;
                }
                bool match = false;
                switch (VGMDAT[ch][row]&NOISE_MASK) {
                    case 16:
                    case 32:
                    case 64:
                        match = true;
                }
                if ((VGMDAT[ch][row]&NOISE_MASK) == VGMDAT[2][row]) {
                    match=true;
                }
                if (!match) {
                    printf("Noise volume has no matching frequency for PSG on row %d. Got 0x%03X, must match 0x010, 0x020, 0x040 or chan 2 0x%03X\n", row, VGMDAT[ch][row], VGMDAT[2][row]);
                    return false;
                }
            } else {
                // tone frequency must be 0x000-0x3FF. Volume is unrestricted
                if ((VGMDAT[ch][row] < 0) || (VGMDAT[ch][row] > 0x3ff)) {
                    printf("Tone frequency out of range for PSG (0-0x3ff) on row %d - got 0x%03X\n", row, VGMDAT[ch][row]);
                    return false;
                }
            }
        }
    }

    updateVolume(chan, cnt);

    return true;
}

int main(int argc, char *argv[])
{
    char namebuf[128];
    char buf[1024];
    char ext[16];
    int delay = 16;
    int scfsong = 0;

	if (argc < 2) {
		printf("testPlayPSG [-ay|-psg] [-scfsong x] [-hidenotes] [-heatmap] [<file prefix> | <file.scf> | <track1> <track2> ...]\n");
        printf(" -ay - force AY restrictions\n");
        printf(" -psg - force PSG restrictions\n");
        printf(" -scfsong x - play SCF song 'x' instead of song 0\n");
		printf(" -hidenotes - do not display notes as the frames are played\n");
        printf(" -heatmap - visualize a heatmap instead of notes while playing - only useful with SCF import\n");
		printf(" <file prefix> - PSG file prefix (usually the name of the original VGM).\n");
        printf(" <track1> etc - instead of a prefix, you may explicitly list the files to play\n");
        printf("Prefix will search for 60hz, 50hz, 30hz, and 25hz in that order.\n");
        printf("Non-prefix can be a list of track files, or a single PSG file.\n");
		return -1;
	}

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
        if (0 == strcmp(argv[arg], "-ay")) {
            testay=true;
        } else if (0 == strcmp(argv[arg], "-psg")) {
            testpsg=true;
            if (testay) {
                printf("\rInvalid to specify both AY and PSG restrictions\n");
                return 1;
            }
        } else if (0 == strcmp(argv[arg], "-scfsong")) {
            ++arg;
            if (arg >= argc-1) {
                printf("\rInsufficient arguments for -scfsong\n");
                return 1;
            }
            if (!isdigit(argv[arg][0])) {
                printf("\rArgument for -scfsong must be numeric\n");
                return 1;
            }
            scfsong = atoi(argv[arg]);
            printf("Playing SCF song %d (if SCF is loaded)\n", scfsong);
        } else if (0 == strcmp(argv[arg], "-hidenotes")) {
			shownotes=false;
        } else if (0 == strcmp(argv[arg], "-heatmap")) {
			shownotes=false;
            heatmap=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		++arg;
	}

    if (shownotes) {
        buildNoteTable();
    }

    int cnt = 0;    // total rows in song
    int chan = 0;   // total channels in song

    // check for explicit list. The problem is the prefix file usually exists, so
    // we HAVE to try to parse first.
    while (arg < argc) {
        char buf[256];

        strncpy_s(namebuf, argv[arg], sizeof(namebuf));
        namebuf[sizeof(namebuf)-1]='\0';

        FILE *fp=fopen(namebuf, "rb");
        if (NULL == fp) {
            printf("Failed to open '%s', code %d\n", namebuf, errno);
            return 1;
        }

        // check if it's an SCF file first
        // Although it's binary data, it COULD theorhetically look like text.
        // It's pretty unlikely, but that's okay. There's a more reliable test:
        // the distance between the first and second 16-bit values must be a multiple
        // of 18. Since the ASCII files contain either decimal or hex, maybe with
        // a comma injected, it's extremely unlikely.
        // Well, that failed on my first test. 0x00. Who knew? So also check that val2 is within the
        // legal position for the end of the file. 
        unsigned char testbuf[4];
        memset(testbuf, 0, sizeof(testbuf));
        fread(testbuf, 1, 4, fp);
        fseek(fp, 0, SEEK_END);
        int end = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        int val1=testbuf[0]*256+testbuf[1];
        int val2=testbuf[2]*256+testbuf[3];
        if (((val2-val1)%18 == 0) && (val2 < end) && (val2 >= end-512)) {
            printf("SCF import %s...\n", namebuf);

            // this is probably an SCF file
            if ((testay==false)&&(testpsg==false)) {
                printf("SCF files must specify -ay or -psg to be imported!\n");
                fclose(fp);
                return 1;
            }
            if (!importSCF(fp, chan, cnt, scfsong)) {
                fclose(fp);
                return 1;
            }
        } else {
            // it might be a 60Hz or a might be a PSG, they have different loaders
            // look at the first line - just check for more than one comma ;)
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf)-1, fp);

            // find first comma and last comma
            char *fcomma = strchr(buf, ',');
            char *lcomma = strrchr(buf, ',');

            if ((fcomma==NULL)||(lcomma==NULL)) {
                if (chan != 0) {
                    // if we were processing...
                    printf("Failed to parse file at first line of file '%s'.\n", namebuf);
                    fclose(fp);
                    return 1;
                }
                // break and check for prefix
                break;
            }

            if (fcomma == lcomma) {
                // then it's a simple 60hz file with one channel
                // noise or tone by filename
                bool noise = (strstr(namebuf, "_noi") != NULL);
                printf("Reading %s channel %d from %s... ", noise?"NOISE":"TONE", chan, namebuf);
                if (!loadDataCSV(fp, chan, cnt, 0, noise, false)) {
                    fclose(fp);
                    return 1;
                }
            } else {
                // it's a PSG file, we need to load all four channels
                // This is not efficient, but it's simple
                for (int idx=0; idx<4; ++idx) {
                    printf("Reading %s channel %d from %s... ", (idx==3)?"NOISE":"TONE", chan, namebuf);
                    if (!loadDataCSV(fp, chan, cnt, idx*2, (idx==3), true)) {
                        fclose(fp);
                        return 1;
                    }
                }
            }
        }
        fclose(fp);
        ++arg;
    }

    if (chan == 0) {
        // maybe using a prefix, lots of guessing involved

        printf("Working with prefix '%s'... ", namebuf);

        // work out which extension - channel 0 must exist
        for (int idx=0; idx<5; ++idx) {
            switch (idx) {
                case 0: strcpy(ext,"60hz"); delay=1000/60; break;
                case 1: strcpy(ext,"50hz"); delay=1000/50; break;
                case 2: strcpy(ext,"30hz"); delay=1000/30; break;
                case 3: strcpy(ext,"25hz"); delay=1000/25; break;
                case 4: printf("\nCan't find the song. Channel 00 must exist as ton or noi!\n"); return 1;
            }
            sprintf_s(buf, "%s_ton00.%s", namebuf, ext);
            FILE *fp = fopen(buf, "rb");
            if (NULL != fp) {
                fclose(fp); 
                break;
            }

            sprintf_s(buf, "%s_noi00.%s", namebuf, ext);
            fp = fopen(buf, "rb");
            if (NULL != fp) {
                fclose(fp); 
                break;
            }
        }

        printf("Found extension %s\n", ext);

        // dumbest loader ever...
        // first try all the tones
        for (int idx=0; idx<99; idx++) {
            sprintf_s(buf, "%s_ton%02d.%s", namebuf, idx, ext);
            FILE *fp = fopen(buf, "rb");
            if (NULL != fp) {
                printf("Reading TONE channel %d from %s... ", idx, buf);
                if (!loadDataCSV(fp, chan, cnt, 0, false, false)) {
                    fclose(fp);
                    return 1;
                }
                fclose(fp);
            }
        }
        // then all the noises
        for (int idx=0; idx<99; idx++) {
            sprintf_s(buf, "%s_noi%02d.%s", namebuf, idx, ext);
            FILE *fp = fopen(buf, "rb");
            if (NULL != fp) {
                printf("Reading NOISE channel %d from %s... ", idx, buf);
                if (!loadDataCSV(fp, chan, cnt, 0, true, false)) {
                    fclose(fp);
                    return 1;
                }
                fclose(fp);
            }
        }
    }

    // do any requested verification
    if (testay) {
        if (!testAYData(chan, cnt)) {
            return 1;
        }
    }
    if (testpsg) {
        if (!testPSGData(chan, cnt)) {
            return 1;
        }
    }

    // setup the sound system
    if (!sound_init(22050)) return 1;

    // set up the heatmap, if that's what we're doing
    HANDLE hConsole = INVALID_HANDLE_VALUE;
    char *scrn = NULL;
    int scrnsize = 0;
    int consoleWidth=0;
    int consoleHeight=0;
    CONSOLE_CURSOR_INFO cursor;
    if (heatmap) {
        CONSOLE_SCREEN_BUFFER_INFO scrnBufInfo;
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleScreenBufferInfo(hConsole, &scrnBufInfo)) {
            printf("* Failed to get console information.\n");
            CloseHandle(hConsole);
            hConsole = INVALID_HANDLE_VALUE;
        } else {
            consoleWidth = scrnBufInfo.srWindow.Right - scrnBufInfo.srWindow.Left;
            consoleHeight = scrnBufInfo.srWindow.Bottom - scrnBufInfo.srWindow.Top - 1;
            scrnsize = consoleWidth * consoleHeight;
            printf("Heatmap with screen buffer of %d x %d\n", consoleWidth, consoleHeight);
            scrn = (char*)malloc(scrnsize+10);
            memset(scrn, ' ', scrnsize);
            SetConsoleMode(hConsole, ENABLE_WRAP_AT_EOL_OUTPUT);    // turn off processing to speed up output
            GetConsoleCursorInfo(hConsole, &cursor);
            cursor.bVisible = false;
            SetConsoleCursorInfo(hConsole, &cursor);        // turn off the cursor
            COORD newsize = {(SHORT)consoleWidth, (SHORT)consoleHeight};
            SetConsoleScreenBufferSize(hConsole, newsize);  // resize back buffer
            system("CLS");
        }
    }

    printf(".. and playing...\n");

    // some timing information so you can see if it's running slow
    int frames = 0;
    int fps = 0;
    time_t lastTime = time(NULL);

    // now play out the song, at delay ms per frame
    // cnt better be the same on each channel ;)
    for (int row = 0 ; row < cnt; ++row) {
        // to heck with correct timing...
        // works oddly well on Win10...
        Sleep(delay);

        // header for the shownotes
        if (shownotes) {
            printf("%04d | ", row);
        }

        // update the heatmap, if that's what we're doing
        // we update 1/5th per frame for performance and to
        // slow the fade a bit.
        if (heatmap) {
            static int nCnt = 0;
            // first, fade out old data: Oo.
            if (++nCnt >= 5) {
                nCnt = 0;
            }
            int start = (scrnsize/5)*nCnt;
            int end = start + scrnsize/5;
            assert(start>=0 && start<scrnsize);
            assert(end>=0 && end<=scrnsize);
            for (int idx=start; idx<end; ++idx) {
                switch (scrn[idx]) {
                case 'O': scrn[idx]='o'; break;
                case 'o': scrn[idx]='.'; break;
                case '.': scrn[idx]=' '; break;
                }
            }
        }

        // process the command line from each channel
        for (int idx=0; idx<chan; ++idx) {
            setfreq(idx, VGMDAT[idx][row], false);  // we never clipbass here anymore
            setvol(idx, mapVolume(VGMVOL[idx][row]));

            if (shownotes) {
                printf("%s %02X | ", getNoteStr(VGMDAT[idx][row],VGMVOL[idx][row]), VGMVOL[idx][row]);
            }
        }

        // calculate timing
        {
            ++frames;
            time_t tnow = time(NULL);
            if (tnow != lastTime) {
                fps = frames;
                frames = 0;
                lastTime = tnow;
            }
        }

        if (shownotes) {
            printf(" (%d fps)\n", fps);
        } else if (heatmap) {
            // add the new data
            for (int idx=0; idx<MAXHEATMAP; ++idx) {
                if (VGMADR[row][idx] == 0) break;
                int offset = (int)((VGMADR[row][idx]/65535.0)*scrnsize);
                assert(offset>=0 && offset<scrnsize);
                scrn[offset] = 'O';
            }

            // we also split up the output by 5ths
            static int nCnt = 0;
            if (++nCnt >= 5) {
                nCnt = 0;
                // copy timing info into the scrn buffer
                sprintf(scrn, "(%d fps)", fps);
            }

            // write output
            COORD pos;
            int start = (scrnsize/5)*nCnt;
            pos.X = start%consoleWidth;
            pos.Y = start/consoleWidth;
            SetConsoleCursorPosition(hConsole, pos);
            WriteConsole(hConsole, scrn+start, scrnsize/5, NULL, NULL);
        } else {
            printf("\r(%d fps)  ", fps);
        }

        if (NULL != soundbuf) {
        	UpdateSoundBuf(soundbuf, sound_update, &soundDat);
        }
    }

    if (heatmap) {
        free(scrn);
        cursor.bVisible = true;
        SetConsoleCursorInfo(hConsole, &cursor);        // turn off the cursor
        system("CLS");
        CloseHandle(hConsole);
    }

    printf("** DONE **\n");
    return 0;
}


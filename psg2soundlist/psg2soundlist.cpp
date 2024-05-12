// psg2soundlist.cpp : This program reads in the PSG data files and writes out a
// binary sound list for the TI player ROM.
// There's no good reason for this save perhaps comparing filesize.

// we take all the same inputs as testPlayer, but you must have no more
// than 3 tone channels and 1 noise channel.

#include "stdafx.h"
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include <cmath>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <time.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAXCHANNELS 100
int VGMDAT[MAXCHANNELS][MAXTICKS];
int VGMVOL[MAXCHANNELS][MAXTICKS];
bool isNoise[MAXCHANNELS];

char NoteTable[4096][4];                    // note names - we'll just lookup the whole range

#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

bool verbose = false;

// lookup table to map PSG volume to linear 8-bit. 
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

// return absolute value
inline int ABS(int x) {
    if (x<0) return -x;
    else return x;
}

// input: 8 bit unsigned audio (0-mute, 255-max)
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

// return true if the channel is muted (by frequency or by volume)
// We cut off at a frequency count of 7, which is roughly 16khz.
// The volume table at this point has been adjusted to the TI volume values
bool muted(int ch, int row) {
    if ((VGMDAT[ch][row] <= 7) || (VGMVOL[ch][row] == 0xf)) {
        return true;
    }
    return false;
}


// pass in file pointer (will be reset), channel index, last row count, first input column (from 0), and true if noise
// set scalevol to true to scale PSG volumes back to 8-bit linear
bool loadDataCSV(FILE *fp, int &chan, int &cnt, int column, bool noise, bool scalevol) {
    // up to 8 columns allowed
    int in[8];
    
    // remember last
    int oldcnt = cnt;

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
    ++chan;

    if ((oldcnt > 0) && (oldcnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", oldcnt, cnt);
    }

    return true;
}

// extract a byte from the buffer, range check it
unsigned char getBufferByte(int cnt, unsigned char *buf, int adr, int maxbytes) {
    // either of these tripping likely indicates a bug or a corrupt file...
    if (adr < 0) {
        printf("* bad SBF unpack address of %d (max %d)\n", adr, maxbytes);
        exit(0);
    }
    if (adr >= maxbytes) {
        printf("* bad SBF unpack address of %d (max %d)\n", adr, maxbytes);
        exit(0);
    }

    // either way, return the data requested
    return buf[adr];
}

// unpack a stream byte
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
    return (buf[toneoffset+y*2]&0xf) + buf[toneoffset+y*2+1]*16;
}


// import a fully featured SBF file
// fp: file pointer at beginning of data structure
// chan - first channel to import into (must be updated on exit)
// cnt - count of rows imported (checked and then updated)
// sbfsong - which song to import - must be already verified
bool importSBF(FILE *fp, int &chan, int &cnt, int sbfsong) {
    // 4 tone, 4 vol, 1 time
    struct STREAM strDat[9];
    // output data
    int curFreq[4];
    int curVol[4];
    bool customNoise = false;

    // a place to store the file
    unsigned char buf[64*1024];

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
    if (streamoffset+18*sbfsong >= toneoffset) {
        printf("Invalid song %d - SBF contains only %d song(s).\n", sbfsong, (toneoffset-streamoffset)/18);
        return 1;
    }

    // point to the first stream of 9
    streamoffset+=18*sbfsong;

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
    }
    isNoise[chan+3] = true;

    /// default settings
    for (int idx=0; idx<4; ++idx) {
        curFreq[idx] = 1;
        curVol[idx] = 0;
    }

    bool done = false;
    while (!done) {
        // run until we have no more data
        done = true;

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
                                curFreq[1] = 1;
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
                                curFreq[2] = 1;
                            }
                        }
                    }
                    if (x&0x10) {
                        // noise
                        if (strDat[3].mainPtr) {
                            int y = getCompressedByte(&strDat[3], buf, cnt, maxbytes);
                            if (strDat[3].mainPtr) {
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

        // next row
        ++cnt;
    }

    printf("Read %d lines\n", cnt);
    chan += 4;  // return 4 channels loaded


    if ((oldcnt > 0) && (oldcnt != cnt)) {
        printf("* Warning: line count changed from %d to %d...\n", oldcnt, cnt);
    }

    return true;
}

// at this point, assume 4 channels, and prepare for SN output
// We do need to check for less than 4...
void devolveData(int row, int chanLookup[3], int noiseLookup) {
    // pass through each imported row. We have three tasks to do:
    // 1) resample the volume to the appropriate logarithmic value
    // 2) Clip any bass notes with a greater shift than 0x3ff
    // 3) handle the noise channel - setting either a fixed shift rate or adapting with voice 3, if free
    int customNoises = 0;
    int noisesMapped = 0;
    int tonesMoved = 0;
    int tonesClipped = 0;

    // to improve compression, pre-process the channel, and any volume muted channels,
    // make sure they are the same frequency as the channel above them
    int mutemaps = 0;
    for (int idx=1; idx<row; ++idx) {   // start at 1
        for (int chi=0; chi<4; ++chi) {
            int ch;
            if (chi==3) ch = noiseLookup; else ch = chanLookup[chi];
            if (ch == -1) continue;
            if (VGMVOL[ch][idx] == 0) {   // NOT converted yet
                if (VGMDAT[ch][idx-1] != VGMDAT[ch][idx]) {
                    ++mutemaps;
                    VGMDAT[ch][idx] = VGMDAT[ch][idx-1];
                }
            }
        }
    }

    for (int idx = 0; idx<row; ++idx) {
        // the volume is easy
        for (int chi = 0; chi < 4; ++chi) {
            int ch;
            if (chi==3) ch = noiseLookup; else ch = chanLookup[chi];
            if (ch == -1) continue;
            VGMVOL[ch][idx] = mapVolume(VGMVOL[ch][idx]);
        }

        // the clipping is simple enough too - we also clip the noise channel
        for (int chi = 0; chi < 4; ++chi) {
            int ch;
            if (chi==3) ch = noiseLookup; else ch = chanLookup[chi];
            if (ch == -1) continue;
            if ((VGMDAT[ch][idx]&NOISE_MASK) > 0x3ff) {
                VGMDAT[ch][idx] = (VGMDAT[ch][idx] & (~NOISE_MASK)) | 0x3ff;
                ++tonesClipped;
            }
        }

        // now we try to map the noise. Three phases:
        // 1) Shift rate matches fixed value - just map directly
        // 2) Channel 3 is free (or can be moved) - apply to channel 3
        // 3) Map to nearest fixed shift rate
        // Remember to watch for the periodic flag and keep the trigger flag
        int noiseShift = VGMDAT[3][idx] & NOISE_MASK;
        bool bPeriodic = (VGMDAT[3][idx] & NOISE_PERIODIC) ? true : false;
        bool bTrigger = (VGMDAT[3][idx] & NOISE_TRIGGER) ? true : false;
        int out = -1;

        if (noiseLookup != -1) {
            if (VGMVOL[noiseLookup][idx] == 0xf) {
                // we're muted, nothing to be mapped - save last note
                if (idx == 0) {
                    out = 1;    // match the tones for default output (this is shift rate 32)
                } else {
                    out = VGMDAT[noiseLookup][idx-1]&0x03;    // take just the noise shift, we add trigger and periodic below
                }
            } else {
                // check for a fixed shift rate
                if (noiseShift == 16) {
                    out = 0;
                } else if (noiseShift == 32) {
                    out = 1;
                } else if (noiseShift == 64) {
                    out = 2;
                }

                if (-1 == out) {
                    // it wasn't fixed, so can we just copy it over into channel 2?
                    if ((muted(chanLookup[2], idx))||(muted(chanLookup[1], idx))||(muted(chanLookup[0], idx))) {
                        // at least one of them are muted
                        if (!muted(chanLookup[2],idx)) {
                            // we need to move channel 2
                            if (muted(chanLookup[1],idx)) {
                                VGMDAT[chanLookup[1]][idx] = VGMDAT[chanLookup[2]][idx];    // move 2->1
                                VGMVOL[chanLookup[1]][idx] = VGMVOL[chanLookup[2]][idx];
                                ++tonesMoved;
                            } else if (muted(chanLookup[0],idx)) {
                                // this must be true, otherwise something weird happened
                                VGMDAT[chanLookup[0]][idx] = VGMDAT[chanLookup[2]][idx];    // move 2->0
                                VGMVOL[chanLookup[0]][idx] = VGMVOL[chanLookup[2]][idx];
                                ++tonesMoved;
                            } else {
                                printf("Internal consistency error at row %d\n", idx);
                                return;
                            }
                        } else {
                            ++customNoises;
                        }
                        // channel 2 is ready to use, overwrite it
                        VGMDAT[chanLookup[2]][idx] = VGMDAT[noiseLookup][idx]&NOISE_MASK;    // take the custom shift rate
                        VGMVOL[chanLookup[2]][idx] = 0xf;   // keep it muted though
                        out = 3;                            // custom shift mode
                    }
                }

                if (-1 == out) {
                    // we still don't have one, so map to the closest shift
                    int diff16 = ABS(16 - (VGMDAT[3][idx]&NOISE_MASK));
                    int diff32 = ABS(32 - (VGMDAT[3][idx]&NOISE_MASK));
                    int diff64 = ABS(64 - (VGMDAT[3][idx]&NOISE_MASK));
                    if ((diff16 <= diff32) && (diff16 <= diff64)) {
                        out = 0;    // use 16
                    } else if ((diff32 <= diff16) && (diff32 <= diff64)) {
                        out = 1;    // use 32
                    } else {
                        out = 2;    // use 64
                    }
                    ++noisesMapped;
                }

                if (-1 == out) {
                    // not possible...
                    printf("Second internal consistency error at row %d\n", idx);
                    return;
                }
            }
        
            if (!bPeriodic) out += 4;               // make it white noise
            if (bTrigger) out |= NOISE_TRIGGER;     // save the trigger flag
            VGMDAT[noiseLookup][idx] = out;         // record the translated noise
        }
    }

    printf("%d custom noises (non-lossy)\n", customNoises);
    printf("%d tones moved   (non-lossy)\n", tonesMoved);
    printf("%d mutes mapped  (non-lossy)\n", mutemaps);
    printf("%d tones clipped (lossy)\n", tonesClipped);
    printf("%d noises mapped (lossy)\n", noisesMapped);
}

int main(int argc, char *argv[])
{
    char namebuf[128];
    char buf[1024];
    char ext[16];
    int delay = 16;
    int sbfsong = 0;

	printf("VGMComp VGM Test Output - v20240511\n");

	if (argc < 3) {
		printf("psg2playlist [-v] [-sbfsong x] (<file prefix> | <file.sbf> | <track1> <track2> ...) <outputfile.bin>\n");
        printf(" -v - verbose output\n");
        printf(" -sbfsong x - import SBF song 'x' instead of song 0\n");
		printf(" <file prefix> - PSG file prefix or full psg file\n");
        printf(" <track1> etc - instead of a prefix, you may explicitly list the files to play\n");
        printf(" <outputfile.bin> - data file to write\n");
        printf("Prefix will search for 60hz, 50hz, 30hz, and 25hz in that order.\n");
        printf("Non-prefix can be a list of track files, or a single PSG file.\n");
		return -1;
	}

	int arg=1;
	while ((arg < argc-2) && (argv[arg][0]=='-')) {
        if (0 == strcmp(argv[arg], "-v")) {
            verbose = true;
        } else if (0 == strcmp(argv[arg], "-sbfsong")) {
            ++arg;
            if (arg >= argc-1) {
                printf("\rInsufficient arguments for -sbfsong\n");
                return 1;
            }
            if (!isdigit(argv[arg][0])) {
                printf("\rArgument for -sbfsong must be numeric\n");
                return 1;
            }
            sbfsong = atoi(argv[arg]);
            printf("Importing SBF song %d (if SBF is loaded)\n", sbfsong);
        } else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		++arg;
	}

    int cnt = 0;    // total rows in song
    int chan = 0;   // total channels in song

    // check for explicit list. The problem is the prefix file usually exists, so
    // we HAVE to try to parse first.
    while (arg < argc-1) {
        char buf[256];

        strncpy_s(namebuf, argv[arg], sizeof(namebuf));
        namebuf[sizeof(namebuf)-1]='\0';

        FILE *fp=fopen(namebuf, "rb");
        if (NULL == fp) {
            printf("Failed to open '%s', code %d\n", namebuf, errno);
            return 1;
        }

        // check if it's an SBF file first
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
            printf("SBF import %s...\n", namebuf);

            // this is probably an SBF file
            printf("Warning: assuming SBF is an SN file, not AY or SID.\n");
            if (!importSBF(fp, chan, cnt, sbfsong)) {
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

    // to support random channel order, make a lookup table for channels
    // custom tone channel 3 must still be channel 3 though, for SN (we only warn)
    int chanLookup[3] = {-1,-1,-1};
    int noiseLookup = -1;
    for (int idx=0; idx<chan; ++idx) {
        if (isNoise[idx]) {
            if (noiseLookup != -1) {
                printf("Too many noise channels (maximum = 1)\n");
                return 1;
            }
            noiseLookup = idx;
        } else {
            if (chanLookup[0] == -1) {
                chanLookup[0] = idx;
            } else if (chanLookup[1] == -1) {
                chanLookup[1] = idx;
            } else if (chanLookup[2] == -1) {
                if (idx != 2) {
                    printf("Warning: tone channel 3 is not index 3 - may affect user defined noise\n");
                }
                chanLookup[2] = idx;
            } else {
                printf("Too many tone channels (maximum = 3)\n");
                return 1;
            }
        }
    }

    // figure out if it can be written out to a TI Playlist
    // maximum of 3 tone channels and 1 noise channels
    devolveData(cnt, chanLookup, noiseLookup);

    // TI Sound list format is very simple:
    // <number of bytes to load> <1-n sound bytes> <number of frames to delay>
    // A duration of 0 ends the sound list.
    //++arg;
    printf("Going to write TI Sound List format binary file '%s'\n", argv[arg]);
    FILE *fp = fopen(argv[arg], "wb");
    if (NULL == fp) {
        printf("Can't open output file '%s'\n", argv[arg]);
        return 1;
    }

    // we assume all four channels are muted to start
    int lastNote[4] = {-1,-1,-1,-1};
    int lastVol[4] = {0x0f,0x0f,0x0f,0x0f};
    int rowCnt = -1;

    // now write out the song - we just write out changes
    // cnt better be the same on each channel ;)
    for (int row = 0 ; row < cnt; ++row) {
        // first check if there are any audible differences on this row
        bool hasChange = false;
        bool changeVol[4] = {false, false, false, false};
        bool changeTone[4] = {false, false, false, false};
        for (int idx=0; idx<4; ++idx) {
            int rch;
            if (idx < 3) {
                // tone
                rch = chanLookup[idx];
            } else {
                rch = noiseLookup;
            }
            if (rch == -1) continue;
            if (VGMVOL[rch][row] != lastVol[idx]) {
                hasChange = true;
                changeVol[idx] = true;
            } 
            if (idx < 3) {
                if ((!muted(rch, row)) && (VGMDAT[rch][row] != lastNote[idx])) {
                    hasChange = true;
                    changeTone[idx] = true;
                }
            } else {
                // noise channel can't use muted() cause no frequency
                if ((VGMVOL[rch][row] != 0xf) && (VGMDAT[rch][row] != lastNote[idx])) {
                    hasChange = true;
                    changeTone[idx] = true;
                }
            }

            if (verbose) printf("%02X %03X ", VGMVOL[rch][row], VGMDAT[rch][row]);
        }

        if (verbose) printf(" -> ");

        if (!hasChange) {
            if (rowCnt < 0) {
                // we haven't found the first note yet, so just ignore this row
            } else {
                ++rowCnt;
            }
            if (verbose) printf("\n");
        } else {
            // we found a change, so first write out the last length
            if (rowCnt > 0) {
                // every time but the first
                fputc(rowCnt, fp);
            }
            // start next count with this row
            rowCnt = 1; 
            
            // then write out the new data
            int bytes = 0;
            for (int idx=0; idx<3; ++idx) {
                if (changeVol[idx]) ++bytes;
                if (changeTone[idx]) bytes+=2;
            }
            if (changeVol[3]) ++bytes;
            if (changeTone[3]) ++bytes; // noise is just 1 byte

            // number of bytes to load
            if (verbose) printf("%02X ", bytes);
            fputc(bytes, fp);

            // now what are those bytes?
            for (int idx=0; idx<3; ++idx) {
                int rch = chanLookup[idx];

                if (changeVol[idx]) {
                    int outval = VGMVOL[rch][row]+0x20*idx+0x90;
                    if (verbose) printf("%02X ", outval);
                    fputc(outval, fp);
                    lastVol[idx] = VGMVOL[rch][row];
                } else {
                    if (verbose) printf("   ");
                }
                if (changeTone[idx]) {
                    int out1 = (VGMDAT[rch][row]&0x0f)+0x20*idx+0x80;
                    int out2 = (VGMDAT[rch][row]&0xff0)>>4;
                    if (verbose) printf("%02X %02X ", out1, out2);
                    fputc(out1, fp);
                    fputc(out2, fp);
                    lastNote[idx] = VGMDAT[rch][row];
                } else {
                    if (verbose) printf("      ");
                }
            }
            if (changeVol[3]) {
                int outval = VGMVOL[noiseLookup][row]+0xf0;
                if (verbose) printf("%02X ", outval);
                fputc(outval, fp);
                lastVol[3] = VGMVOL[noiseLookup][row];
            } else {
                if (verbose) printf("   ");
            }
            if (changeTone[3]) {
                int outval = (VGMDAT[noiseLookup][row]&0x0f)+0xe0;
                if (verbose) printf("%02X ", outval);
                fputc(outval, fp);
                lastNote[3] = VGMDAT[noiseLookup][row]&0x0f;
            } else {
                if (verbose) printf("   ");
            }

            if (verbose) printf("\n");
        }
    }

    // finish the file
    if (rowCnt > 0) {
        fputc(rowCnt, fp);
    }
    // mute all channels and end
    fprintf(fp, "\x04\x9f\xbf\xdf\xff"); 
    fputc(0x00, fp);    // need to terminate with a zero byte

    printf("- Wrote %d bytes\n", ftell(fp));
    fclose(fp);

    printf("** DONE **\n");
    return 0;
}


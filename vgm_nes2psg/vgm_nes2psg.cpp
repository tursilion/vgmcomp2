// vgm_nes2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input NES APU input streams, and more
// or less emulate the NES hardware to get PSG data.

// Square: Okay
// Triangle: Okay
// Noise: Okay
// DMC: Okay? Some limitations maybe can't be overcome? SMB3 for instance.
// Length: Okay
// Sweep: Maybe?
// Envelope: Okay

// Currently, we set all channels (even noise) to countdown values
// compatible with the SN clock
// All volume is set to 8 bit values. This lets us take in a wider
// range of input datas, and gives us more flexibility on the output.
// Likewise, noise is stored as its frequency count, for more flexibility

// Currently data is stored in nCurrentTone[] with even numbers being
// tones and odd numbers being volumes, and the fourth tone is assumed
// to be the noise channel.

// Tones: scales to SN countdown rates, even on noise, 0 to 0xfff (4095) (other chips may go higher!)
// Volume: 8-bit linear (NOT logarithmic) with 0 silent and 0xff loudest
// Note that /either/ value can be -1 (integer), which means never set (these won't land in the stream though)

#include <stdio.h>
#include <stdarg.h>
#define MINIZ_HEADER_FILE_ONLY
#include "tinfl.c"						    // comes in as a header with the define above
										    // here to allow us to read vgz files as vgm

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary

// in general even channels are tone (or noise) and odd are volume
// They are mapped to PSG frequency counts and 8-bit volume (0=mute, 0xff=max)
// the NES has 5 channels, so that's 10 channels per chip here
#define MAXCHANNELS 20
int VGMStream[MAXCHANNELS][MAXTICKS];		// every frame the data is output

unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
int nCurrentTone[MAXCHANNELS];				// current value for each channel in case it's not updated
double freqClockScale = 1.0;                // doesn't affect noise channel, even though it would in reality
                                            // all chips assumed on same clock
int nTicks;                                 // this MUST be a 32-bit int
bool verbose = false;                       // emit more information
int debug = 0;                              // dump parser data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count

// codes for noise processing (NES periodic noise is a little random... PSG's is not)
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

// options
bool enableperiodic = false;            // enable the periodic noise mode - probably won't work well without freq scaling
bool disabledmcvolhack = false;         // the DMC reduces the volume of noise and triangle - this will disable that adjust
int waveMode = 0;                       // 0 - treat as tone, 1 - treat as noise, 2 - ignore
bool scaleFreqClock = true;			    // apply scaling from APU clock rates (not an option here)
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;
int triangleVolume = 8;                 // average volume output of the triangle channel

// DMG emulation

// the APU divides the input clock by 12 to get the VGM clock of 1789772
// there is then a 240hz feature clock (roughly), which divides that again
// by 7457.5 (but why that number? Probably a reverse engineering bug). We'll
// use the integer value which should be correct
#define SEQUENCECLK 7457

// max number of chip channels (5 each)
#define MAXCHIP (5*2)

// length table (bit 3, bits 4-7)
unsigned char lengthData[32] = {
 // table from apt_ref.txt - pre-multiplied by 2 for the inbuilt shifter
//3=0  3=1     state of bit 3, rows are bits 4-7
 0x0A, 0xFE,
 0x14, 0x02,
 0x28, 0x04,
 0x50, 0x06,
 0xA0, 0x08,
 0x3C, 0x0A,
 0x0E, 0x0C,
 0x1A, 0x0E,

 0x0C, 0x10,
 0x18, 0x12,
 0x30, 0x14,
 0x60, 0x16,
 0xC0, 0x18,
 0x48, 0x1A,
 0x10, 0x1C,
 0x20, 0x1E
};

// noise frequency table
unsigned int noiseData[16] = {
    0x002, 0x004, 0x008, 0x010, 0x020, 0x030, 0x040, 0x050,
    0x065, 0x07f, 0x0be, 0x0fe, 0x17d, 0x1fc, 0x3f9, 0x7f2
};

// dmc frequency table
unsigned int dmcData[16] = {
    0xd60, 0xbe0, 0xaa0, 0xa00, 0x8f0, 0x7f0, 0x710, 0x6b0,
    0x5f0, 0x500, 0x470, 0x400, 0x350, 0x2a8, 0x240, 0x1b0
};

#define MIN(x,y) ((x) < (y) ? (x) : (y))

class Channel {
public:
    Channel() { reset(); }

    void reset() {
        // set values to default?
        sweepEn = false;
        sweepPer = 0;
        sweepPerWork = 0;
        sweepNeg = false;
        sweepShift = 0;

        lengthEnable = false;
        length = 0;

        linear = 0;
        linearreload = 0;
        linearreloadflag=true;
        clearreloadflag = false;

        volumeEn = false;
        volumeLoop = false;
        volume = 0;
        volumeDecay = 0xf;
        volumePer = 0;
        volumePerWork = 0;

        dmcLoop = false;
        dmcBase = 0;
        dmcAdr = 0;
        dmcCnt = 0;
        dmcCntRaw = 0;
        dmcPer = 0;
        dmcPerWork = 0;
        dmcOutput = 0;
        dmcDelta = 0;
        dmcBuffer = 0;
        dmcBitsLeft = 0;

        frequency = 0xfff;
    }

    // sweep frequency generator
    bool sweepEn;
    int sweepPer, sweepPerWork;
    bool sweepNeg;
    int sweepShift;

    // length doesn't need a work register
    bool lengthEnable;
    int length;

    // triangle linear counter
    int linear, linearreload;
    bool linearreloadflag, clearreloadflag; // only need clear cause I don't save the registers

    // volume envelope generator
    bool volumeEn;
    bool volumeLoop;
    int volume, volumeDecay;
    int volumePer, volumePerWork;

    // DMC
    bool dmcLoop;
    int dmcAdr, dmcBase;
    int dmcCnt, dmcCntRaw;
    int dmcOutput;
    int dmcDelta;
    int dmcBuffer, dmcBitsLeft;
    double dmcPer, dmcPerWork;

    // Frequency
    // we don't emulate the frequency playback, so that's all we need
    int frequency;

};

// control registers
unsigned char sampleRam[2][32*1024];  // 32k of DMA sample RAM for 2 chips (note: only 1 block supported in VGM)
Channel chan[MAXCHIP];    // tone, tone, triangle, noise, dmc, two chips
int sequence[2];          // sequence step for each chip
bool seqlong[2];          // whether it's the long (5step) sequence
double seqClock;          // one seqclock for all chips ;)
unsigned int nClock;      // chip main clock, so we can run the sequence properly

// function generator
// 0x01 = run length and sweep, 0x02 = run envelope and linear counter (not emulated)
const unsigned char Seq4Step[4] = { 2,3,2,3 };
const unsigned char Seq5Step[5] = { 3,2,3,2,0 };

// I think the NES is meant to be linear
unsigned char volumeTable[16] = {
	240,224,208,192,176,160,144,
	128,
	112,96,80,64,48,32,16,0
};

#define ABS(x) ((x)<0?-(x):x)

// crc function for checking the gunzip
unsigned long crc(unsigned char *buf, int len);

// printf replacement I can use for verbosity ;)
void myprintf(const char *fmt, ...) {
	if (verbose) {
		va_list lst;
		va_start(lst, fmt);
		vfprintf(stdout, fmt, lst);
		va_end(lst);
	} else {
		static int spin=0;
		const char anim[] = "|/-\\";
		printf("\r%c", anim[spin++]);
		if (spin >= 4) spin=0;
	}
}

// try to unpack buffer as a gzipped vgm (vgz) - note that the extension
// is not always vgz in practice! I've seen gzipped vgm in the wild
// using miniz's "tinfl.c" as the decompressor
// returns difference between new length and buffersize so the caller
// can update statistics
int tryvgz() {
	size_t outlen=0;
	unsigned int npos = 0;
	myprintf("Signature not detected.. trying gzip (vgz)\n");
	// we have to parse the gzip header to find the packed data, then tinfl
	// can handle it. (miniz doesn't support gzip headers)
	// gzip header is pretty simple - especially since we don't care about much of it
	// see RFC 1952 (may 1996)
	if (buffersize < 10) {
		printf("\rToo short for GZIP header\n");
		return 0;
	}
	// 0x1f 0x8f -- identifier
	if ((buffer[npos]!= 0x1f)||(buffer[npos+1]!=0x8b)) {
		printf("\rNo GZIP signature.\n");
		return 0;
	}
	npos+=2;
	// compression method - if not '8', it's not gzip deflate
	if (buffer[npos] != 8) {
		printf("\rNot deflate method.\n");
		return 0;
	}
	npos++;
	// flags - we mostly only care to see if we need to skip other data
	unsigned int flags = buffer[npos++];
	// modification time, don't care
	npos+=4;
	// extra flags, don't care
	npos++;
	// OS, don't care
	npos++;
	// optional extra header
	if (flags&0x04) {
		// two byte header, then data
		int xlen = buffer[npos] + buffer[npos+1]*256;
		// don't care though
		npos+=2+xlen;
	}
	// optional filename
	if (flags & 0x08) {
		// zero-terminated ascii
		while ((npos < buffersize) && (buffer[npos] != 0)) npos++;
		npos++;
	}
	// optional comment
	if (flags & 0x10) {
		// zero-terminated ascii
		while ((npos < buffersize) && (buffer[npos] != 0)) npos++;
		npos++;
	}
	// header CRC
	if (flags & 0x02) {
		// two bytes of CRC32
		npos+=2;
		printf("\rWarning, skipping header CRC\n");
	}
	// unknown flags
	if (flags & 0xe0) {
		printf("\rUnknown flags, decompression may fail.\n");
	}

	// we should now be pointing at the data stream
	// AFTER the data stream, the last 8 bytes are
	// 4 bytes of CRC32, and 4 bytes of size, both
	// referring to the original, uncompressed data

	void *p = tinfl_decompress_mem_to_heap(&buffer[npos], buffersize-npos, &outlen, 0);
	if (p == NULL) {
		printf("\rDecompression failed.\n");
		return 0;
	}

	// check CRC32 and uncompressed size
	unsigned int crc32=buffer[buffersize-8]+buffer[buffersize-7]*256+buffer[buffersize-6]*256*256+buffer[buffersize-5]*256*256*256;
	unsigned int origsize=buffer[buffersize-4]+buffer[buffersize-3]*256+buffer[buffersize-2]*256*256+buffer[buffersize-1]*256*256*256;
	if (origsize != outlen) {
		printf("\rWARNING: output size does not match file\n");
	} else {
		if (crc32 != crc((unsigned char*)p, outlen)) {
			printf("\rWARNING: GZIP CRC does not match output data\n");
		}
	}

	if (outlen > sizeof(buffer)) {
		printf("\rResulting file larger than buffer size (1MB), truncating.\n");
		outlen=sizeof(buffer);
	}
	myprintf("\rDecompressed to %d bytes\n", outlen);
	int ret = outlen - buffersize;
	memcpy(buffer, p, outlen);
	buffersize=outlen;
	free(p);
	return ret;
}

// return the 8-bit volume for channel 'ch'
int getVolume(int ch) {
    int chip = ch/5;
    int chanidx = ch%5;
    bool isWave = (chanidx == 4);

    if ((isWave) && (waveMode == 2)) return 0;     // user asked to ignore wave

    // square waves are muted when outside of valid frequency
    if ((chanidx == 0)||(chanidx == 1)) {
        if ((chan[ch].frequency < 8) || (chan[ch].frequency > 0x7ff)) {
            return 0;
        }
    }

    // DMC is muted when out of data (only in our case, not in real life, it holds level IRL)
    if (chanidx == 4) {
        // TODO: this might be 8 samples too early -- but in this
        // frame-based player maybe that won't matter
        if (chan[ch].dmcCnt <= 0) {
            return 0;
        }
        // otherwise whatever the volume is is good
        // DMC is from 0 to 127 - I don't want quite to 254... this gives to about 240
        int ret = int(chan[ch].volume * 1.89 + .5);
        return ret;
    }

    // special case for triangle channel
    if (chanidx == 2) {
        // triangle has no volume - it is either on or off
        if ((chan[ch].length > 0) && (chan[ch].linear > 0)) {
            return volumeTable[triangleVolume];
        } else {
            return 0;
        }
    } else {
        // for anyone else, check if the length generator has us muted
        if ((chan[ch].lengthEnable) && (chan[ch].length <= 0)) {
            return 0;
        }
    }

    // I think we're good
    int ret = 0;
    if (chan[ch].volume == 0) return 0;             // muted, no output
    ret = volumeTable[15-(chan[ch].volume&0xf)];

    // if noise or triangle, adjust with the DMC level
    if (!disabledmcvolhack) {
        if ((chanidx==2)||(chanidx==3)) {
            // Some math from the apu_ref.txt (there's another divide not shown here):
            // 1 / (15/8227 + 127/22638) = 134 -> 0.68
            // 1 / (15/8227 + 0/22638) = 548   -> 0.24  <---- triangle is worth 24
            // 1 / (0/8227 + 127/22638) = 178  -> 0.57  <---- triangle is worth 11
            // 1 / (15/12241 + 0/22638) = 816  -> 0.17  <---- noise is worth 17
            // 1 / (15/12241 + 127/22638) = 146-> 0.65  <---- noise is worth 8
            // So... the fake volume control on noise and triangle is worth about 50%
            // We'll just do a sloppy linear scale on that.
            // Apparently SMB3 actually USES this.
            // DMC has this affect whether it's clocking or not
            double ratio = 1.0 - (chan[chip*5+4].dmcDelta / 240);   // division selected for 57% volume as per docs
            ret = int(ret * ratio + 0.5);
        }
    }

    return ret;
}

// run the emulation - at this point we always assume 60hz
// This needs to get the nCurrentTone array up to date
void runEmulation() {
    // DMC volume emulation
    int voldmc[MAXCHIP];
    int cntdmc[MAXCHIP];
    for (int channel = 0; channel < MAXCHIP; ++channel) {
        voldmc[channel]=0;
        cntdmc[channel]=0;
    }

    // run frame sequencer for 1/60th of a second
    double time = (nClock/60.0) / SEQUENCECLK;
    seqClock += time;
    while (seqClock >= 1.0) {
        seqClock -= 1.0;

        bool runL;  // run length and sweep
        bool runE;  // run envelope and linear counter (not implemented, we don't need it)

        for (int channel = 0; channel < MAXCHIP; ++channel) {
            int chip = channel/5;

            if (chip-channel == 0) {
                // only update the sequencer once per chip...
                ++sequence[chip];
                if (seqlong[chip]) {
                    // 5 step
                    if (sequence[chip] > 4) sequence[chip]=0;
                    runL = (Seq5Step[sequence[chip]] & 1) ? true : false;
                    runE = (Seq5Step[sequence[chip]] & 2) ? true : false;
                } else {
                    // 4 step
                    if (sequence[chip] > 3) sequence[chip]=0;
                    runL = (Seq4Step[sequence[chip]] & 1) ? true : false;
                    runE = (Seq4Step[sequence[chip]] & 2) ? true : false;
                }
            }

            // Frequency sweep (square wave only)
            if (runL) {
                if (chan[channel].sweepEn) {
                    if (chan[channel].sweepPerWork) --chan[channel].sweepPerWork;
                    if (chan[channel].sweepPerWork <= 0) {
                        chan[channel].sweepPerWork = chan[channel].sweepPer;

                        // No shadow reg like the GB
                        int cnt = chan[channel].frequency >> chan[channel].sweepShift;
                        if (chan[channel].sweepNeg) cnt = -cnt;

                        int newFreq = chan[channel].frequency + cnt;
                        // special case for the second square wave only
                        if ((channel-chip == 1)&&(chan[channel].sweepNeg)) {
                            --newFreq;
                        }

                        // no update if we zero out or no length
                        if ((newFreq > 0) && (newFreq < 0x800) && (chan[channel].length > 0)) {
                            chan[channel].frequency = newFreq & 0x7ff;
                        }

                        // stop cases
                        if ((newFreq < 8) || (newFreq > 0x7ff)) {
                            chan[channel].sweepEn = false;
                        }
                    }
                }
            }

            // Envelope generator (square and noise)
            if (runE) {
                // decay counts even when not enabled
                if (chan[channel].volumePerWork > 0) --chan[channel].volumePerWork;
                if (chan[channel].volumePerWork <= 0) {
                    chan[channel].volumePerWork = chan[channel].volumePer;

                    if (chan[channel].volumeDecay >= 0) {
                        --chan[channel].volumeDecay;
                        if (chan[channel].volumeDecay < 0) {
                            if (chan[channel].volumeLoop) {
                                chan[channel].volumeDecay = 0xf;
                            } else {
                                chan[channel].volumeDecay = 0;
                            }
                        }
                    }

                    // apply it to the volume if enabled
                    if (chan[channel].volumeEn) {
                        chan[channel].volume = chan[channel].volumeDecay;
                    }
                }
            }

            // Length counter (all channels but dmc)
            if (runL) {
                if (chan[channel].lengthEnable) {
                    if (chan[channel].length > 0) {
                        chan[channel].length--;
                        // we shouldn't do anything here - getVolume will
                        // check if we're supposed to be muted
                    }
                }
            }

            // Linear counter (triangle channel)
            if (runE) {
                if (channel-chip == 2) {
                    // triangle, check linear counter reload
                    if (chan[channel].linearreloadflag) {
                        chan[channel].linear = chan[channel].linearreload;
                        if (chan[channel].clearreloadflag) {
                            chan[channel].clearreloadflag = false;
                            chan[channel].linearreloadflag = false;
                        }
                    } else {
                        // so I'm not sure, do I check the lengthenable or not?
                        if (chan[channel].linear > 0) {
                            chan[channel].linear--;
                            // we don't have to do anything here because
                            // getVolume will check linear and length to see
                            // if output is allowed
                        }
                    }
                }
            }

            // DMC channel
            if (channel-chip == 4) {
                // we actually need some shift rate calculation this time
                // to get an average for the played sample
                // this is run every seqclock (roughly 240hz), not at 60hz
                chan[channel].dmcPerWork += (nClock/60.0);    // 240hz of clocks
                while ((chan[channel].dmcPerWork > chan[channel].dmcPer) && (chan[channel].dmcCnt > 0)) {
                    chan[channel].dmcPerWork -= chan[channel].dmcPer;

                    // Sample RAM runs from 0x8000 to 0xFFFF, however, the
                    // start address is 0xC000. It will wrap around to 0x8000
                    // if necessary.
                    if (chan[channel].dmcBitsLeft <= 0) {
                        // TODO: probably a bug in the protocol, VGM doesn't support multiple
                        // RAM blocks with multiple sound chips
                        chan[channel].dmcBuffer = sampleRam[0][chan[channel].dmcAdr & 0x7fff];
                        chan[channel].dmcBitsLeft = 7;
                        ++chan[channel].dmcAdr;
                        if (chan[channel].dmcAdr > 0xffff) chan[channel].dmcAdr -= 0x8000;
                        --chan[channel].dmcCnt;
                        if (debug == 5) {
                            printf("  DEBUG: DMC byte: 0x%02X\n", chan[channel].dmcBuffer);
                        }
                    } else {
                        --chan[channel].dmcBitsLeft;
                    }

                    if (chan[channel].dmcCnt < 0) {
                        if (chan[channel].dmcLoop) {
                            chan[channel].dmcAdr = chan[channel].dmcBase;
                            chan[channel].dmcCnt = chan[channel].dmcCntRaw;
                        } else {
                            chan[channel].dmcCnt = 0;
                            break;
                        }
                    }

                    if (chan[channel].dmcBuffer & 0x01) {
                        chan[channel].dmcOutput++;
                        if (chan[channel].dmcOutput > 0x3f) chan[channel].dmcOutput = 0x3f;
                    } else {
                        chan[channel].dmcOutput--;
                        if (chan[channel].dmcOutput < 0) chan[channel].dmcOutput = 0;
                    }
                    chan[channel].dmcBuffer >>= 1;
#if 1
                    // flat output
                    voldmc[channel] += chan[channel].dmcOutput;
#else
                    // signed sample output to abs
                    if (chan[channel].dmcOutput > 32) voldmc[channel] += chan[channel].dmcOutput-32;
                    else voldmc[channel] += 32-chan[channel].dmcOutput;
#endif
                    ++cntdmc[channel];
                }
                if (chan[channel].dmcCnt <= 0) {
                    // if we aren't playing, don't accumulate time
                    chan[channel].dmcPerWork = 0;
                }
            }

            if (debug) {
                if (channel == debug-1) {
                    switch (channel) {
                        case 0:
                        case 1: // square
                            printf("DEBUG: SEQ: Freq: 0x%03X, length: %d(%d), sweep: (%d), vol: %d(%d,LP:%d)\n",
                                    chan[channel].frequency,
                                    chan[channel].length, chan[channel].lengthEnable,
                                    chan[channel].sweepEn,
                                    getVolume(channel), chan[channel].volumeEn, chan[channel].volumeLoop);
                            break;
                        case 2: // triangle
                            printf("DEBUG: SEQ: Freq: 0x%03X, length: %d(%d), linear: %d:%d(%d), vol: %d\n",
                                    chan[channel].frequency,
                                    chan[channel].length, chan[channel].lengthEnable,
                                    chan[channel].linear, chan[channel].linearreload, chan[channel].linearreloadflag,
                                    getVolume(channel));
                            break;
                        case 3: // noise
                            printf("DEBUG: SEQ: Freq: 0x%03X(%d), length: %d(%d), vol: %d(%d,LP:%d)\n",
                                    chan[channel].frequency&0xfff, chan[channel].frequency&NOISE_PERIODIC?1:0,
                                    chan[channel].length, chan[channel].lengthEnable,
                                    getVolume(channel), chan[channel].volumeEn, chan[channel].volumeLoop);
                            break;
                        case 4: // dmc
                            printf("DEBUG: SEQ: Freq: 0x%03X, Adr: 0x%04X, Cnt: 0x%04X, Loop:(%d), Output: %d, Delta: %d, vol: %d\n",
                                    chan[channel].frequency,
                                    chan[channel].dmcAdr, chan[channel].dmcCnt, chan[channel].dmcLoop?1:0,
                                    chan[channel].dmcOutput, chan[channel].dmcDelta, getVolume(channel));
                            break;
                    }
                }
            }

        }   // channel

    }   // seqClock

    // finish off the DMC sample munging
    for (int channel = 0; channel < MAXCHIP; ++channel) {
        // this will only be incremented on DMC channels
        if (cntdmc[channel] > 0) {
            // no change if we didn't process anything
            chan[channel].volume = (int(voldmc[channel]/cntdmc[channel]) << 1); // volume times 2
            if (chan[channel].volume > 0x7f) chan[channel].volume = 0x7f;
            if (chan[channel].volume < 0) chan[channel].volume = 0;
            chan[channel].dmcDelta = chan[channel].volume;  // overwrite it if valid
        }
    }

    // update nCurrentTone[]'s
    for (int ch = 0; ch < MAXCHIP; ++ch) {
        int outch = ch*2;
        switch (ch%5) {
            case 0:
            case 1:     // square waves
            case 3:     // noise
            case 4:     // DMC
                nCurrentTone[outch] = chan[ch].frequency;
                break;

            case 2:     // triangle wave runs roughly 50% lower frequency
                nCurrentTone[outch] = chan[ch].frequency*2;
                break;
        }
        nCurrentTone[outch+1] = getVolume(ch);
    }
}

// output one current row of audio data
// return false if a fatal error occurs
bool outputData() {
    // get our working values
    int nWork[MAXCHANNELS];

    // number of rows out is 1 or 2
    int rowsOut = 1;
    // This causes padding to adapt 50hz to 60hz playback
    // it's the same output code just repeated
	if ((nRate==50)&&(nTicks%5==0)) {   // every 5 frames, gives us 10 extra frames to get 60
		// output an extra frame delay
        rowsOut++;
    }

    // now we can manipulate these, if needed for special cases

    // now write the result into the current line of data
    for (int rows = 0; rows < rowsOut; ++rows) {
        // reload the work regs
        for (int idx=0; idx<MAXCHANNELS; ++idx) {
            nWork[idx] = nCurrentTone[idx];
        }

        for (int idx=0; idx<MAXCHANNELS; idx++) {
		    if (nWork[idx] == -1) {         // no entry yet
                switch(idx%10) {
                    case 0:
                    case 2:
                    case 4: // tones
                    case 6: // noise
                    case 8: // dmc
                        nWork[idx] = 1;     // very high pitched
                        break;

                    case 1:
                    case 3:
                    case 5:
                    case 7: // volumes
                    case 9:
                        nWork[idx] = 0;     // mute
                        break;
                }
            }
            if (((idx&1)==0) && (nWork[idx] == 0)) {
                // this is a flat line on a sega console, but on the TI version
                // it's a low pitched tone. Convert it to high frequency mute
                nWork[idx] = 1;
            }
		    VGMStream[idx][nTicks]=nWork[idx];
	    }
    	nTicks++;
	    if (nTicks > MAXTICKS) {
		    printf("\rtoo many ticks (%d), can not process. Need a shorter song.\n", MAXTICKS);
		    return false;
	    }

        // and run the sound chip emulator
        runEmulation();

        // now that it's output, clear any noise triggers
        for (int idx=0; idx<MAXCHIP; ++idx) {
            if (chan[idx].frequency != -1) {
                chan[idx].frequency &= ~NOISE_TRIGGER;
            }
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
	printf("Import VGM NES - v20200328\n");

	if (argc < 2) {
		printf("vgm_nes2psg [-q] [-d <n>] [-o <n>] [-add <n>] [-triangle <n>] [-enableperiodic] [-disabledmcvolhack] [-dmcnoise|-dmcnone] [-ignoreweird] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d <n> - enable parser debug output for channel n (1-5) (use only with single chip)\n");
        printf(" -o <n> - output only channel <n> (1-5)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -triangle <n> - set triangle volume to <n> (0-15, default is 8)\n");
        printf(" -enableperiodic - enable periodic noise when short mode set (usually not helpful)\n");
        printf(" -disabledmcvolhack - don't reduce volume of triangle and noise as DMC volume increases\n");
        printf(" -dmcnoise - treat the DMC channel as noise\n");
        printf(" -dmcnone - ignore the DMC channel (if neither, treat as tone)\n");
        printf(" -ignoreweird - ignore anything else unexpected and treat as default\n");
		printf(" <filename> - VGM file to read.\n");
		return -1;
	}
    verbose = true;

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-q")) {
			verbose=false;
        } else if (0 == strcmp(argv[arg], "-d")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -d parameter.\n");
                return -1;
            }
            ++arg;
            debug = atoi(argv[arg]);
            if ((debug > 5) || (debug < 1)) {
                printf("debug channel must be 1 (sq), 2 (sq), 3 (tri), 4 (noise) or 5 (dmc)\n");
                return -1;
            }
            printf("Debug for channel %d: ", debug);
            switch(debug) {
                case 1: printf("Square 1\n"); break;
                case 2: printf("Square 2\n"); break;
                case 3: printf("Triangle\n"); break;
                case 4: printf("Noise\n"); break;
                case 5: printf("DMC\n"); break;
            }
        } else if (0 == strcmp(argv[arg], "-add")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -add parameter.\n");
                return -1;
            }
            ++arg;
            addout = atoi(argv[arg]);
            printf("Output channel index offset is now %d\n", addout);
        } else if (0 == strcmp(argv[arg], "-o")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -o parameter.\n");
                return -1;
            }
            ++arg;
            output = atoi(argv[arg]);
            if ((output > 10) || (output < 1)) {
                printf("output channel must be 1 (sq), 2 (sq), 3 (tri), 4 (noise) or 5 (dmc) (6-10 for second chip)\n");
                return -1;
            }
            printf("Output ONLY channel %d: ", debug);
            switch(output) {
                case 1: printf("Square 1\n"); break;
                case 2: printf("Square 2\n"); break;
                case 3: printf("Triangle\n"); break;
                case 4: printf("Noise\n"); break;
                case 5: printf("DMC\n"); break;
            }
		} else if (0 == strcmp(argv[arg], "-triangle")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -triangle parameter.\n");
                return -1;
            }
            ++arg;
            triangleVolume = atoi(argv[arg]);
            if ((triangleVolume > 15) || (triangleVolume < 0)) {
                printf("Triangle volume must be from 0 (mute) to 15 (loudest). Default is 8.\n");
                return -1;
            }
            if (triangleVolume == 0) {
                printf("Warning: Triangle channel muted at command line.\n");
            }
		} else if (0 == strcmp(argv[arg], "-enableperiodic")) {
			enableperiodic=true;
		} else if (0 == strcmp(argv[arg], "-disabledmcvolhack")) {
			disabledmcvolhack=true;
		} else if (0 == strcmp(argv[arg], "-dmcnoise")) {
			waveMode=1;
		} else if (0 == strcmp(argv[arg], "-dmcnone")) {
			waveMode=2;
		} else if (0 == strcmp(argv[arg], "-ignoreweird")) {
			ignoreWeird=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    // global variable init
    memset(sampleRam, 0, sizeof(sampleRam));
    sequence[0] = 0;
    sequence[1] = 0;
    seqlong[0] = false;
    seqlong[1] = false;
    seqClock = 0;

	{
		FILE *fp=fopen(argv[arg], "rb");
		if (NULL == fp) {
			printf("\rfailed to open file '%s'\n", argv[arg]);
			return -1;
		}
		myprintf("\rReading %s - ", argv[arg]);
		buffersize=fread(buffer, 1, sizeof(buffer), fp);
		fclose(fp);
		myprintf("%d bytes\n", buffersize);

		// -Split a VGM file into multiple channels (8 per chip - 4 audio, 3 tone and 1 noise)
        //  (although the wave channel can be tone or noise)
        //  We allow up to 2 chips.
		//  For the first pass, emit for every frame (that should be easier to parse down later)

		// The format starts with a 256 byte header (1.70):
		//
		//      00  01  02  03   04  05  06  07   08  09  0A  0B  0C  0D  0E  0F
		// 0x00 ["Vgm " ident   ][EoF offset     ][Version        ][SN76489 clock  ]
		// 0x10 [YM2413 clock   ][GD3 offset     ][Total # samples][Loop offset    ]
		// 0x20 [Loop # samples ][Rate           ][SN FB ][SNW][SF][YM2612 clock   ]
		// 0x30 [YM2151 clock   ][VGM data offset][Sega PCM clock ][SPCM Interface ]
		// 0x40 [RF5C68 clock   ][YM2203 clock   ][YM2608 clock   ][YM2610/B clock ]
		// 0x50 [YM3812 clock   ][YM3526 clock   ][Y8950 clock    ][YMF262 clock   ]
		// 0x60 [YMF278B clock  ][YMF271 clock   ][YMZ280B clock  ][RF5C164 clock  ]
		// 0x70 [PWM clock      ][AY8910 clock   ][AYT][AY Flags  ][VM] *** [LB][LM]
		// 0x80 [GB DMG clock   ][NES APU clock  ][MultiPCM clock ][uPD7759 clock  ]
		// 0x90 [OKIM6258 clock ][OF][KF][CF] *** [OKIM6295 clock ][K051649 clock  ]
		// 0xA0 [K054539 clock  ][HuC6280 clock  ][C140 clock     ][K053260 clock  ]
		// 0xB0 [Pokey clock    ][QSound clock   ] *** *** *** *** [Extra Hdr ofs  ]
        // 0xC0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
        // 0xD0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
        // 0xE0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***
        // 0xF0  *** *** *** ***  *** *** *** ***  *** *** *** ***  *** *** *** ***	

        // 56 67 6D 20 0B 1F 00 00 01 01 00 00 94 9E 36 00 
		// 00 00 00 00 C7 1D 00 00 91 D4 12 00 40 02 00 00 
		// 00 3A 11 00 3C 00 00 00 00 00 00 00 00 00 00 00 
		// 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
		// 50 80 50 00 50 A0 50 30 50 C0 50 00 50 E4 50 92 
		// 50 B3 50 D5 50 F0 62 50 A0 50 00 50 CF 50 1A 50 
		// E4 50 94 50 B4 50 F4 62 62 50 C9 50 1B 50 92 50 
		// B3 50 D6 50 F5 62 62 50 C3 50 1C 50 94 50 D7 50 
		// F7 62 62 50 C0 50 00 50 95 50 B4 50 D5 50 F4 62 
		// 62 50 96 50 BF 50 D6 50 F5 62 62 50 D7 50 F4 62 
		// 62 50 D8 50 FF 62 62 50 CF 50 1A 50 D5 50 F4 62 
		// 62 50 C9 50 1B 50 D6 50 F5 62 62 50 C3 50 1C 50

		// verify VGM
		if (0x206d6756 != *((unsigned int*)buffer)) {
			int cnt = tryvgz(); // returns difference between bufferlen and new data
			if (0x206d6756 != *((unsigned int*)buffer)) {
				printf("\rFailed to find VGM tag.\n");
				return -1;
			}
		}

		unsigned int nEOF=4+*((unsigned int*)&buffer[4]);
		if (nEOF > buffersize) {
			printf("\rWarning: EOF in header past end of file! Truncating.\n");
			nEOF = buffersize;
		}

		unsigned int nVersion = *((unsigned int*)&buffer[8]);
		myprintf("Reading version 0x%X\n", nVersion);
        if (nVersion < 0x161) {
            printf("Failure - NES not supported earlier than version 1.61\n");
            return 1;
        }

		nClock = *((unsigned int*)&buffer[0x84]);
        if (nClock&0x40000000) {
            // bit indicates dual NES chips - though I don't need to do anything here
            // the format assumes that all NESes are identical
            myprintf("Dual NES output specified (we shall see!)\n");
        }
		nClock&=0x0FFFFFFF;
        if (nClock == 0) {
            printf("\rThis VGM does not appear to contain a NES APU. This tool extracts only NES data.\n");
            return -1;
        }
        if (nClock != 0) {
		    if ((nClock < 1789772-50) || (nClock > 1789772+50)) {
			    freqClockScale = 1789772/nClock;
			    printf("\rUnusual NES clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }
            // now adapt for the ratio between NES and PSG, which is faster at 3579545.0 Hz
            // The NES clock is pre-divided by two, so we'll do the same. This seems to be
            // pretty close to correct.
            freqClockScale *= (3579545.0/2) / 1789772.0;
            if (debug) {
                myprintf("\rPSG clock scale factor %f\n", freqClockScale);
            }
        }
		if (nVersion > 0x100) {
			nRate = *((unsigned int*)&buffer[0x24]);
            if (nRate == 0) {
                printf("Warning: rate set to zero - treating as 60hz\n");
                nRate = 60;     // spec doesn't define a default...
            }
			if ((nRate!=50)&&(nRate!=60)) {
				printf("\rweird refresh rate %d - only 50hz or 60hz supported\n", nRate);
                if (ignoreWeird) {
                    printf("  .. ignoring as requested - treating as 60hz\n");
                    nRate = 60;
                } else {
    				return -1;
                }
			}
		}
		myprintf("Refresh rate %d Hz\n", nRate);

        // shift register width is irrelevant

        // find the start of data
		unsigned int nOffset=0x40;
		if (nVersion >= 0x150) {
			nOffset=*((unsigned int*)&buffer[0x34])+0x34;
			if (nOffset==0x34) nOffset=0x40;		// if it was 0
		}

        // prepare the decode
		for (int idx=0; idx<MAXCHANNELS; idx+=2) {
			nCurrentTone[idx]=-1;		// never set
			nCurrentTone[idx+1]=-1;		// never set
		}
		nTicks= 0;
		bool delaywarn = false;			// warn about imprecise delay conversion

        // start up the chip emulation
        runEmulation();

		// Use nRate - if it's 50, add one tick delay every 5 frames
		// Use nOffset for the pointer
		// Stop parsing at nEOF
		// 1 'sample' is intended to be at 44.1kHz
		while (nOffset < nEOF) {
			static int nRunningOffset = 0;

			// parse data for a tick
			switch (buffer[nOffset]) {		// what is it? what is it?
			case 0x50:		// PSG data byte
            case 0x30:      // second PSG data byte
                {
				    static bool warn = false;
				    if (!warn) {
					    printf("\rUnsupported chip PSG skipped\n");
					    warn = true;
				    }
                }
				nOffset+=2;
				break;

			case 0x61:		// 16-bit wait value
				{
					unsigned int nTmp=buffer[nOffset+1] | (buffer[nOffset+2]<<8);

                    if (debug) printf("  DEBUG: delay %f frames\n", (float)nTmp/735.0);

					// divide down from samples to ticks (either 735 for 60hz or 882 for 50hz)
					if (nTmp % ((nRate==60)?735:882)) {
						if ((nRunningOffset == 0) && (!delaywarn)) {
							printf("\rWarning: Delay time loses precision (total %d, remainder %d samples).\n", nTmp, nTmp % ((nRate==60)?735:882));
							delaywarn=true;
						}
					}
					{
						// this is a crude way to do it - but if the VGM is consistent in its usage, it works
						// (ie: Space Harrier Main BGM uses this for a faster playback rate, converts nicely)
						int x = (nTmp+nRunningOffset)%((nRate==60)?735:882);
						nTmp=(nTmp+nRunningOffset)/((nRate==60)?735:882);
						nRunningOffset = x;
					}
					while (nTmp-- > 0) {
                        if (!outputData()) return -1;
					}
					nOffset+=3;
				}
				break;

			case 0x62:		// wait 735 samples (60th second)
			case 0x63:		// wait 882 samples (50th second)
				// going to treat both of these the same. My output intends to run at 60Hz
				// and so this counts as a tick
                if (debug) printf("  DEBUG: delay 1 frame\n");

                if (!outputData()) return -1;
				nOffset++;
				break;

			case 0x66:		// end of data
				nOffset=nEOF+1;
				break;

			case 0x70:		// wait 1 sample
			case 0x71:		// wait 2 samples
			case 0x72:
			case 0x73:
			case 0x74:
			case 0x75:
			case 0x76:
			case 0x77:
			case 0x78:
			case 0x79:
			case 0x7a:
			case 0x7b:
			case 0x7c:
			case 0x7d:
			case 0x7e:
			case 0x7f:		// wait 16 samples
				// try the same hack as above
                {
                    int cnt = buffer[nOffset]-0x70+1;
			        if (nRunningOffset == 0) {
				        printf("\rWarning: fine timing (%d samples) lost.\n", cnt);
			        }
                    if (debug) printf("  DEBUG: fine delay %d samples\n", cnt);
			        nRunningOffset+=cnt;
			        if (nRunningOffset > ((nRate==60)?735:882)) {
				        nRunningOffset -= ((nRate==60)?735:882);
                        if (!outputData()) return -1;				
			        }
			        nOffset++;
                }
				break;

			// skipped opcodes - entered only as I encounter them - there are whole ranges I /could/ add, but I want to know.
			case 0x4f:		// game gear stereo (ignore)
				nOffset+=2;
				break;

			// unsupported sound chips
			case 0x51:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2413 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x52:
			case 0x53:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2612 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x54:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2151 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x55:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2203 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x56:
			case 0x57:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2608 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x58:
			case 0x59:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM2610 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x5A:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM3812 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x5B:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM3526 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x5C:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YM8950 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x5D:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YMZ280B skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

			case 0x5E:
			case 0x5F:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YMF262 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0x67:
                {
                    // data block: 0x67 0x66 tt ss ss ss ss /data/
                    int type = buffer[nOffset+2];
                    unsigned int sz = *((unsigned int *)&buffer[nOffset+3]);
                    if (type != 0xc2) {
                        static bool warn = false;
                        if (!warn) {
                            printf("\rIgnoring unsupported data block type 0x%02X\n", type);
                            warn=true;
                        }
                    } else {
                        // populate NES DMA memory
                        // first type bytes are the start offset
                        int start = *((unsigned short*)&buffer[nOffset+7]);
                        // this is an address from 0x8000
                        start -= 0x8000;
                        sz-=2;
                        if ((start+sz > 0x8000) || (sz < 1)) {
                            printf("\rNES DMC offset out of range: Start 0x%04X, length 0x%04X\n", start+0x8000, sz);
                        } else {
                            // TODO: probably a bug in the protocol, VGM doesn't support multiple
                            // RAM blocks with multiple sound chips
                            memcpy(&sampleRam[0][start], &buffer[nOffset+9], sz);
                            printf("Copy DMC data to 0x%04X, length 0x%04X\n", start+0x8000, sz);
                            if (debug == 5) {
                                printf("  DEBUG: ");
                                for (unsigned int idx=0; idx<MIN(8,sz); ++idx) {
                                    printf("%02X ", buffer[nOffset+9+idx]);
                                }
                                printf("...\n");
                            }
                        }
                    }
                
                    nOffset+=(*((unsigned int*)&buffer[nOffset+3]))+7;
				}
				break;

            case 0x68:
                {
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported PCM RAM skipped\n");
						warn = true;
					}
					nOffset+=((*((unsigned int*)&buffer[nOffset+8]))&0xffffff)+12;
				}
				break;

            case 0x80:
            case 0x81:
            case 0x82:
            case 0x83:
            case 0x84:
            case 0x85:
            case 0x86:
            case 0x87:
            case 0x88:
            case 0x89:
            case 0x8a:
            case 0x8b:
            case 0x8c:
            case 0x8d:
            case 0x8e:
            case 0x8f:
                // YM2612 port data - skip quietly
				nOffset++;
				break;

            case 0x90:
            case 0x91:
            case 0x95:
                {
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported DAC Stream skipped\n");
						warn = true;
					}
                    nOffset+=5;
				}
				break;

                // more stream commands, different sizes
            case 0x92:
                nOffset+=6;
                break;
            case 0x93:
                nOffset+=11;
                break;
            case 0x94:
                nOffset+=2;
                break;

            case 0xa0:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip AY-3-8910 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb0:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip RF5C68 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb1:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip RF5C164 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb2:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip PWM skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb3:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip GB skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb4:
				{
                    // NES APU: 0xb4 AA DD
                    // AA is the register, +0x80 for chip 2
                    // DD is the byte to write
                    int chipoff = 0;
                    if (buffer[nOffset+1]>0x7f) chipoff = 5;
                    unsigned char ad = (buffer[nOffset+1]&0x7f);
                    unsigned char da = buffer[nOffset+2];

                    switch (ad) {
                        case 0x00:  // square duty/volume/envelope decay (0)
                        case 0x04:  // square duty/volume/envelope decay (1)
                        case 0x0c:  // noise volume/envelope decay  (3)
                        {
                            // ddle nnnn   duty, loop env/disable length, env disable, vol/env period
                            // 0-3	volume / envelope decay rate
                            // 4	envelope decay disable
                            // 5	envelope decay looping enable
                            // 6-7  duty on the square waves (which we ignore)
                            int ch = ad/4+chipoff;
                            chan[ch].volumeEn = (da&0x10) ? false : true;   // bit is volume decay DISABLE
                            chan[ch].volumeLoop = (da&0x20) ? true : false;
                            chan[ch].lengthEnable = !chan[ch].volumeLoop;   // you get looping, or length
                            if (chan[ch].volumeEn) {
                                chan[ch].volume = 0;
                                chan[ch].volumePer = chan[ch].volumePerWork = (da&0x0f);
                            } else {
                                chan[ch].volume = (da&0x0f);
                                chan[ch].volumePer = chan[ch].volumePerWork = 0;
                            }
                            if ((da&0xc0) != 0x80) {
                                static bool warn = false;
                                if (!warn) {
                                    warn = true;
                                    printf("\rUnsupported duty cycle ignored.\n");
                                }
                            }

                            if ((debug)&&(ad == (debug-1)*4)) {
                                printf("  DEBUG: Volume:%d(%d), LengthEn: (%d)\n", chan[ch].volume, chan[ch].volumeEn,
                                       chan[ch].lengthEnable);
                            }
                        }
                        break;

                        case 0x01:  // square sweep/period/negative/shift (0)
                        case 0x05:  // square sweep/period/negative/shift (1)
                        {
                            // eppp nsss   enable sweep, period, negative, shift
                            int ch = (ad-1)/4+chipoff;

                            bool en = (da&0x80) ? true : false;
                            int per = (da&0x70)>>4;
                            bool neg = (da&0x08) ? true : false;
                            int shift = (da&0x07);

                            chan[ch].sweepEn = en;
                            chan[ch].sweepPer = chan[ch].sweepPerWork = per;
                            chan[ch].sweepNeg = neg;
                            chan[ch].sweepShift = shift;

                            if ((debug)&&(ad-1 == (debug-1)*4)) {
                                printf("  DEBUG:  Sweep:%d(%d), Neg: %d, Shift: %d\n", chan[ch].sweepPer, chan[ch].sweepEn,
                                       chan[ch].sweepNeg, chan[ch].sweepShift);
                            }
                        }
                        break;

                        case 0x02:  // square period low
                        case 0x06:  // square period low
                        case 0x0a:  // triangle period low
                        {
                            // pppp pppp   period low
                            int ch = (ad-2)/4+chipoff;
                            chan[ch].frequency &= 0xf00;
                            chan[ch].frequency |= da;

                            if ((debug)&&(ad-2 == (debug-1)*4)) {
                                printf("  DEBUG:  Freq: 0x%03X (low)\n", chan[ch].frequency);
                            }

                        }
                        break;

                        case 0x03:  // square length/period high
                        case 0x07:  // square length/period high
                        case 0x0b:  // triangle length/period high
                        case 0x0f:  // noise length
                        {
                            // llll lppp   length index, period high (not used on noise)
                            int ch = (ad-3)/4+chipoff;
                            chan[ch].volumeDecay = 0xf;
                            if (ad != 0x0f) {
                                // noise doesn't have frequency bits here
                                chan[ch].frequency &= 0xff;
                                chan[ch].frequency |= (da&0x07)<<8;
                            }
                            int bit3=(da&0x08) ? 1 : 0;
                            int idx = ((da&0xf0)>>4)*2 + bit3;
                            chan[ch].length = lengthData[idx];

                            if (ad == 0x0b) {
                                // triangle sets the reload flag
                                chan[ch].linearreloadflag = true;
                            }

                            if ((debug)&&(ad-3 == (debug-1)*4)) {
                                printf("  DEBUG: Frequency: 0x%03X  Length: %d\n", chan[ch].frequency, chan[ch].length);
                            }
                        }
                        break;

                        case 0x08:  // triangle length control/counter
                        {
                            // clll llll   control, linear counter load
                            int ch = chipoff+2;
                            chan[ch].lengthEnable = (da&0x80) ? false : true;   // bit is a disable, uses linear counter then
                            chan[ch].linearreload = da&0x7f;
                            if (chan[ch].lengthEnable) {
                                chan[ch].clearreloadflag = true;
                            }
                            if ((debug)&&(debug==3)) {
                                printf("  DEBUG: Linear: %d(%d)\n", chan[ch].linearreload, chan[ch].lengthEnable?0:1);
                            }
                        }
                        break;

                        case 0x0e:  // noise short mode/period index
                        {
                            // s--- pppp   short mode, period index
                            int ch = chipoff+3;
                            chan[ch].frequency = noiseData[da&0x0f]/2;  // do we need to divide by 2 for 'every other clock'?
                            if ((enableperiodic)&&(da&0x80)) {
                                // short mode switches between 32768 bits long and 93 bits long noise
                                chan[ch].frequency |= NOISE_PERIODIC;
                            }
                            // TODO: do we have to trigger? docs only say on reset
                            //chan[ch].frequency |= NOISE_TRIGGER;
                            
                            if ((debug)&&(debug==4)) {
                                printf("  DEBUG: Frequency: 0x%03X  Short: %d\n", chan[ch].frequency, (da&0x80)?1:0);
                            }
                        }
                        break;

                        case 0x10:  // DMC play mode and frequency
                        {
                            // il-- ffff   IRQ enable (ignored), loop, frequency index
                            int ch = chipoff+4;
                            chan[ch].dmcLoop = (da&0x40) ? true : false;
                            chan[ch].dmcPer = chan[ch].dmcPerWork = dmcData[da&0x0f];
                            chan[ch].frequency = dmcData[da&0x0f];      // also copy to frequency for conversion

                            if ((debug)&&(debug==5)) {
                                printf("  DEBUG: DMC Period: %f  Loop: %d\n", chan[ch].dmcPer, chan[ch].dmcLoop ? 1: 0);
                            }
                        }
                        break;

                        case 0x11:  // dmc delta channel
                        {
                            // the only side effect we bother with here is the fakey volume
                            // control when the channel is disabled
                            int ch = chipoff+4;
                            chan[ch].dmcDelta = da&0x7f;

                            if ((debug)&&(debug==5)) {
                                printf("  DEBUG: DMC Delta: %d\n", chan[ch].dmcDelta);
                            }
                        }
                        break;

                        case 0x12:  // dmc address load
                        {
                            int ch = chipoff+4;
                            chan[ch].dmcAdr = chan[ch].dmcBase = (da<<6)|0xc000;
                            chan[ch].dmcBitsLeft = 0;   // IRL the bits play out, then reload

                            if ((debug)&&(debug==5)) {
                                printf("  DEBUG: DMC Address: 0x%04X\n", chan[ch].dmcAdr);
                            }
                        }
                        break;

                        case 0x13:  // dmc length
                        {
                            int ch = chipoff+4;
                            //chan[ch].dmcCnt =     TODO: I think we don't load dmcCnt here..
                            chan[ch].dmcCntRaw = (da << 4) + 1;

                            if ((debug)&&(debug==5)) {
                                printf("  DEBUG: DMC Length: 0x%04X\n", chan[ch].dmcCntRaw);
                            }
                        }
                        break;

                        case 0x15:  // DMC/IRQ/Length counter/channel enable
                        {
                            // ---d nt21   length ctr enable: DMC, noise, triangle, pulse 2, 1
                            chan[4+chipoff].lengthEnable = (da&0x10) ? true : false;
                            chan[3+chipoff].lengthEnable = (da&0x08) ? true : false;
                            chan[2+chipoff].lengthEnable = (da&0x04) ? true : false;
                            chan[1+chipoff].lengthEnable = (da&0x02) ? true : false;
                            chan[0+chipoff].lengthEnable = (da&0x01) ? true : false;

                            // noise
                            if (!chan[3+chipoff].lengthEnable) {
                                chan[3+chipoff].length = 0;
                            }

                            // triangle
                            if (!chan[2+chipoff].lengthEnable) {
                                chan[2+chipoff].length = 0;
                            }
                            // squares
                            if (!chan[1+chipoff].lengthEnable) {
                                chan[1+chipoff].length = 0;
                            }
                            if (!chan[0+chipoff].lengthEnable) {
                                chan[0+chipoff].length = 0;
                            }

                            // special cases for DMC
                            if (!chan[4+chipoff].lengthEnable) {
                                chan[4+chipoff].length = 0;
                                chan[4+chipoff].dmcCnt = 0;
                                chan[4+chipoff].dmcLoop = 0;
                            } else {
                                if (chan[4+chipoff].dmcCnt == 0) {
                                    chan[4+chipoff].dmcCnt = chan[4+chipoff].dmcCntRaw;
                                }
                            }

                            if (debug) {
                                printf("  DEBUG: Length Counter Enable: %d\n", chan[chipoff+debug-1].lengthEnable ? 1:0);
                            }
                        }
                        break;

                        case 0x17:  // timer control
                        {
                            // fd-- ----   5-frame cycle, disable frame interrupt (ignored)
                            sequence[chipoff/5] = 0;          // sequence step for each chip
                            seqlong[chipoff/5] = (da&0x80) ? true : false;

                            if (debug) {
                                printf("  DEBUG: Frame length: %d\n", seqlong[chipoff/5] ? 5 : 4);
                            }
                        }
                        break;

                        default:
                            // note: 0D and 09 are unused in every doc I've seen, but some
                            // songs write to them...?
                            printf("Unknown NES register write: $40%02X = 0x%02X\n", ad, da);
                            break;
                    }
                
                    nOffset+=3;
				}
				break;

            case 0xb5:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip MultiPCM skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb6:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip uPD7759 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb7:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip OKIM6258 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb8:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip OKIM6295 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xb9:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip Hu6280 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xba:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip K053260 skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xbb:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip Pokey skipped\n");
						warn = true;
					}
					nOffset+=3;
				}
				break;

            case 0xc0:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip Sega PCM skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xc1:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip RF5C68 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xc2:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip RF5C164 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xc3:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip MultiPCM skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xc4:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip QSound skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xd0:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YMF278B skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xd1:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip YMF271 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xd2:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip SCC1 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xd3:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip K054539 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xd4:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip C140 skipped\n");
						warn = true;
					}
					nOffset+=4;
				}
				break;

            case 0xe0:
                // pcm seek, ignore
                nOffset += 5;
                break;

            case 0xff:		// reserved, skip 4 bytes
				nOffset+=5;
				break;

			default:
				printf("\rUnsupported command byte 0x%02X at offset 0x%04X\n", buffer[nOffset], nOffset);
				return -1;
			}
		}

		myprintf("File %d parsed! Processed %d ticks (%f seconds)\n", 1, nTicks, (float)nTicks/60.0);

		if (scaleFreqClock) {
			int clip = 0;
			if (freqClockScale != 1.0) {
				myprintf("Adapting tones for PSG clock rate... ");
				// scale every tone - clip if needed (note further clipping for PSG may happen at output)
                for (int chip = 0; chip < MAXCHANNELS; chip += 10) {
				    for (int idx=0; idx<nTicks; idx++) {
                        if (VGMStream[0+chip][idx] > 1) {
					        VGMStream[0+chip][idx] = (int)(VGMStream[0+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[0+chip][idx] == 0)    { VGMStream[0+chip][idx]=1;     clip++; }
					        if (VGMStream[0+chip][idx] > 0xfff) { VGMStream[0+chip][idx]=0xfff; clip++; }
                        }

                        if (VGMStream[2+chip][idx] > 1) {
					        VGMStream[2+chip][idx] = (int)(VGMStream[2+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[2+chip][idx] == 0)    { VGMStream[2+chip][idx]=1;     clip++; }
					        if (VGMStream[2+chip][idx] > 0xfff) { VGMStream[2+chip][idx]=0xfff; clip++; }
                        }

                        if (VGMStream[4+chip][idx] > 1) {
                            // this might have triggers in it, as it might be noise
                            bool trig = (VGMStream[4+chip][idx]&NOISE_TRIGGER) ? true : false;
                            VGMStream[4+chip][idx] &= NOISE_MASK;
					        VGMStream[4+chip][idx] = (int)(VGMStream[4+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[4+chip][idx] == 0)    { VGMStream[4+chip][idx]=1;     clip++; }
					        if (VGMStream[4+chip][idx] > 0xfff) { VGMStream[4+chip][idx]=0xfff; clip++; }
                            if (trig) VGMStream[4+chip][idx] |= NOISE_TRIGGER;
                        }

                        if (VGMStream[6+chip][idx] > 1) {
                            // noise too on this one!
                            bool trig = (VGMStream[6+chip][idx]&NOISE_TRIGGER) ? true : false;
                            VGMStream[6+chip][idx] &= NOISE_MASK;
					        VGMStream[6+chip][idx] = (int)(VGMStream[6+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[6+chip][idx] == 0)    { VGMStream[6+chip][idx]=1;     clip++; }
					        if (VGMStream[6+chip][idx] > 0xfff) { VGMStream[6+chip][idx]=0xfff; clip++; }
                            if (trig) VGMStream[6+chip][idx] |= NOISE_TRIGGER;
                        }

                        if (VGMStream[8+chip][idx] > 1) {
					        VGMStream[8+chip][idx] = (int)(VGMStream[8+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[8+chip][idx] == 0)    { VGMStream[8+chip][idx]=1;     clip++; }
					        if (VGMStream[8+chip][idx] > 0xfff) { VGMStream[8+chip][idx]=0xfff; clip++; }
                        }
                    }
                }
				if (clip > 0) myprintf("%d tones clipped", clip);
				myprintf("\n");
			}
		} else {
            myprintf("Skipping frequency scale...");
        }
	}

    // data is entirely stored in VGMStream[ch][tick]
    // [ch] the channel, and is tone/vol/tone/vol/tone/vol/noise/vol
    // we just write out tone channels as <name>_tonX.60hz
    // the noise channel is named <name>_noiX.60hz
    // Volume is written alongside every tone
    // simple ASCII format, values stored as hex (but import should support decimal if no 0x), row number is implied frame
    {
        int outChan = addout;
        for (int ch=0; ch<MAXCHANNELS; ch+=2) {
            char strout[1024];
            char num[32];   // just a string buffer for the index number

            // first, determine if there's any data in this stream to output
            // we check both volume and frequency. High frequencies are inaudible
            // so meaningless (they can happen on voice 3 for noise, but we have
            // moved that data into the noise channel). Muted volumes are likewise
            // unimportant and have the same caveat. We cut off at a frequency
            // count of 7, which is roughly 16khz. That said, noise channels are
            // allowed all the way down to zero.
            bool process = false;
            for (int r=0; r<nTicks; ++r) {
                if ((VGMStream[ch][r] > 7) && (VGMStream[ch+1][r] > 0)) {
                    process = true;
                    break;
                }
                if (((ch&7)==6) && (VGMStream[ch+1][r] > 0)) {
                    // noise channel with volume, even if frequency is high
                    process = true;
                    break;
                }
            }
            if (!process) {
                myprintf("Skipping channel %d - no data\n", ch/2+1);
                continue;
            }
            if ((output)&&(ch/2 != output-1)) {
                myprintf("Skipping channel %d - output not requested\n", ch/2+1);
                continue;
            }

            // create a filename
            strcpy(strout, argv[arg]);
            // every channel 6 is a noise channel, every channel 8 might be
            // 01 - square
            // 23 - square
            // 45 - triangle
            // 67 - noise
            // 89 - dmc
            bool isNoise = false;
            if ((ch%10)==6) isNoise = true;
            if (((ch%10)==8) && (waveMode == 1)) isNoise = true;
            if (isNoise) {
                strcat(strout, "_noi");     // noise (same format as tone, except noise flags are valid)
            } else {
                strcat(strout, "_ton");     // tone (0-4095)
            }
            sprintf(num, "%02d", outChan++);
            strcat(strout, num);
            strcat(strout, ".60hz");

            // open the file
		    FILE *fp=fopen(strout, "wb");
		    if (NULL == fp) {
			    printf("failed to open file '%s'\n", strout);
			    return -1;
		    }
		    myprintf("-Writing channel %d as %s...\n", ch/2+1, strout);

            for (int r=0; r<nTicks; ++r) {
                fprintf(fp, "0x%08X,0x%02X\n", VGMStream[ch][r], VGMStream[ch+1][r]);
            }

            fclose(fp);
        }
    }

    myprintf("done vgm_nes2psg.\n");
    return 0;
}

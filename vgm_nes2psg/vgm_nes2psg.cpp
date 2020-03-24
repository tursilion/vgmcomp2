// vgm_nes2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input NES APU input streams, and more
// or less emulate the NES hardware to get PSG data.

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

// 0-1 are square, 2 is triangle, 3 is noise, 4 is DMC, then repeat for chip 2
// Need defines, the GB stuff was a pain to code...
// these are offsets into VGMStream from Chipoffset - volume is plus 1
// nCurrentTone uses the same indexes.
#define CH_SQUARE1 0
#define CH_SQUARE2 2
#define CH_TRIANGLE 4
#define CH_NOISE 6
#define CH_DMC 8

unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
int nCurrentTone[MAXCHANNELS];				// current value for each channel in case it's not updated
double freqClockScale = 1.0;                // doesn't affect noise channel, even though it would in reality
                                            // all chips assumed on same clock
int nTicks;                                 // this MUST be a 32-bit int
bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data

// codes for noise processing (NES periodic noise is a little random... PSG's is not)
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

// options
bool enableperiodic = false;            // enable the periodic noise mode - probably won't work well without freq scaling
bool disabledmcvolhack = false;         // the DMC reduces the volume of noise and triangle - this will disable that adjust
int waveMode = 2;                       // 0 - treat as tone, 1 - treat as noise, 2 - ignore
bool scaleFreqClock = true;			    // apply scaling from APU clock rates (not an option here)
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;

// DMG emulation

// the APU divides the input clock by 12 to get the VGM clock of 1789772
// there is then a 240hz feature clock (roughly), which divides that again
// by 7457.5 (but why that number? Probably a reverse engineering bug). We'll
// use the integer value which should be correct
#define SEQUENCECLK 7457

// max number of chip channels (5 each)
#define MAXCHIP (5*2)

// length table
unsigned char lengthData[2][16] = {
 // table from apt_ref.txt - pre-multiplied by 2 for the inbuilt shifter
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

class Channel {
public:
    Channel() { reset(); }

    void reset() {
        // set values to default?
        enabled = false;
        sweepEn = false;
        sweepPer = 0;
        sweepPerWork = 0;
        sweepNeg = false;
        sweepShift = 0;

        lengthEnable = false;
        length = 0;

        volumeEn = false;
        volumeLoop = false;
        volume = 0;
        volumeDecay = 0xf;
        volumePer = 0;
        volumePerWork = 0;

        sampleLoop = false;
        sampleBase = 0;
        sampleAdr = 0;
        sampleCnt = 0;
        sampleCntRaw = 0;
        dmcPer = 0;
        dmcPerWork = 0;
        dmcOutput = 0;

        frequency = 0xfff;
    }

    // internal enable flag
    bool enabled;

    // sweep frequency generator
    bool sweepEn;
    int sweepPer, sweepPerWork;
    bool sweepNeg;
    int sweepShift;

    // length doesn't need a work register
    bool lengthEnable;
    int length;

    // volume envelope generator
    bool volumeEn;
    bool volumeLoop;
    int volume, volumeDecay;
    int volumePer, volumePerWork;

    // DMC
    bool sampleLoop;
    int sampleAdr, sampleBase;
    int sampleCnt, sampleCntRaw;
    int dmcOutput;
    double dmcPer, dmcPerWork;

    // Frequency
    // we don't emulate the frequency playback, so that's all we need
    int frequency;

};

// control registers
unsigned char sampleRam[2][32*1024];  // 32k of DMA sample RAM for 2 chips
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

    // check for mute cases
    if (!chan[ch].enabled) return 0;                // not enabled, no output

    // I think we're good
    int ret = 0;
    if (chan[ch].volume == 0) return 0;             // muted, no output
    if (chanidx == 4) {
        // DMC is from 0 to 127 - I don't want quite to 254... this gives to about 240
        ret = int(chan[ch].volume * 1.89 + .5);
    } else {
        ret = volumeTable[15-(chan[ch].volume&0xf)];
    }

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
            double ratio = 1.0 - (chan[chip*5+4].volume / 240);   // division selected for 57% volume as per docs
            ret = int(ret * ratio + 0.5);
        }
    }

    return ret;
}

// run the emulation - at this point we always assume 60hz
// This needs to get the nCurrentTone array up to date
void runEmulation() {
    // run frame sequencer for 1/60th of a second
    double time = (nClock/60.0) / SEQUENCECLK;
    seqClock += time;
    while (seqClock >= 1.0) {
        for (int channel = 0; channel < MAXCHIP; ++channel) {
            int chip = channel/5;

            bool runL;  // run length and sweep
            bool runE;  // run envelope and linear counter (not implemented, we don't need it)

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
                        if ((newFreq > 0) && (chan[channel].length > 0)) {
                            chan[channel].frequency = newFreq & 0x7ff;
                        }

                        // stop cases
                        if ((newFreq < 8) || (newFreq > 0x7ff)) {
                            chan[channel].sweepEn = false;
                            chan[channel].enabled = false;
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

                    if (chan[channel].volumeDecay > 0) {
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
                        // TODO: so.. what, do we mute at zero?
                        if (0 == chan[channel].length) {
                            chan[channel].volume = 0;
                            chan[channel].enabled = false;
                        }
                    }
                }
            }

            // DMC channel
            if (channel-chip == 4) {
                // we actually need some shift rate calculation this time
                // to get an average for the played sample
                if (chan[channel].enabled) {
                    int vol = 0;
                    int cnt = 0;
                    chan[channel].dmcPerWork += (nClock/60.0);    // 60hz of clocks
                    while (chan[channel].dmcPerWork > chan[channel].dmcPer) {
                        // we'll always process a full byte at a time
                        chan[channel].dmcPerWork -= chan[channel].dmcPer;

                        int byte = sampleRam[chip][chan[channel].sampleAdr&0x7fff];
                        ++chan[channel].sampleAdr;
                        --chan[channel].sampleCnt;

                        if (chan[channel].sampleCnt < 0) {
                            if (chan[channel].sampleLoop) {
                                chan[channel].sampleAdr = chan[channel].sampleBase;
                                chan[channel].sampleCnt = chan[channel].sampleCntRaw;
                            } else {
                                chan[channel].enabled = false;
                                break;
                            }
                        }

                        int bit = 0x01;
                        for (int idx=0; idx<8; ++idx) {
                            // real NES is LSB first, but we don't care here
                            if (byte&bit) {
                                chan[channel].dmcOutput++;
                                if (chan[channel].dmcOutput > 0x3f) chan[channel].dmcOutput = 0x3f;
                            } else {
                                chan[channel].dmcOutput--;
                                if (chan[channel].dmcOutput < 0) chan[channel].dmcOutput = 0;
                            }
                            ++cnt;
                            vol += chan[channel].dmcOutput;
                        }
                    }
                    chan[channel].volume = (int(vol/cnt) << 1);
                    if (chan[channel].volume > 0x7f) chan[channel].volume = 0x7f;
                    if (chan[channel].volume < 0) chan[channel].volume = 0;
                }
            }
        }   // channel
    }   // seqClock

    // update nCurrentTone[]'s
    for (int ch = 0; ch < MAXCHIP; ++ch) {
        int outch = ch*2;
        switch (ch%5) {
            case 0:
            case 1:     // square waves
            case 2:     // triangle wave
            case 3:     // noise
            case 4:     // DMC
                nCurrentTone[outch] = chan[ch].frequency;
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
        if (debug) printf("tick\n");

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
	printf("Import VGM NES - v20200324\n");

	if (argc < 2) {
		printf("vgm_nes2psg [-q] [-d] [enableperiodic] [disabledmcvolhack] [-dmcnoise|-dmcnone] [-ignoreweird] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
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
			debug = true;
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
            printf("Failure - Gameboy not supported earlier than version 1.61\n");
            return 1;
        }

		nClock = *((unsigned int*)&buffer[0x80]);
        if (nClock&0x40000000) {
            // bit indicates dual GB chips - though I don't need to do anything here
            // the format assumes that all GBs are identical
            myprintf("Dual GB output specified (we shall see!)\n");
        }
		nClock&=0x0FFFFFFF;
        if (nClock == 0) {
            printf("\rThis VGM does not appear to contain a DMG (Gameboy). This tool extracts only DMG (Gameboy) data.\n");
            return -1;
        }
        if (nClock != 0) {
		    if ((nClock < 4194304-50) || (nClock > 4194304+50)) {
			    freqClockScale = 4194304/nClock;
			    printf("\rUnusual GB clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }
            // now adapt for the ratio between GB and PSG, which is slower at 3579545.0 Hz
            // fewer ticks for the same tone
            // Confirmed - both of these clock the audio through a divide-by-32, giving the
            // PSG counter at 111860.8Hz and the GB counter at 131072.0Hz. So this should be right.
            freqClockScale *= 3579545.0 / 4194304.0;
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

        // shift register is 15 bits wide, but can be set in software to 7,
        // we accomodate by tweaking the frequency instead

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
				if (nRunningOffset == 0) {
					printf("\rWarning: fine timing lost.\n");
				}
				nRunningOffset+=buffer[nOffset]-0x70;
				if (nRunningOffset > ((nRate==60)?735:882)) {
					nRunningOffset -= ((nRate==60)?735:882);
                    if (!outputData()) return -1;				
				}
				nOffset++;
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
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported data block skipped\n");
						warn = true;
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
                    // gameboy DMG: 0xB3 AA DD
                    // AA is the register, +0x80 for chip 2
                    // DD is the byte to write

                    // so.. is AA the 0xFFaa address or an index? Will assume address
                    int chipoff = 0;
                    if (buffer[nOffset+1]>0x7f) chipoff = 4;

                    // the VGM puts NR10 at 0, I've mapped to the register address, so add 0x10
                    unsigned char ad = (buffer[nOffset+1]&0x7f) + 0x10;
                    unsigned char da = buffer[nOffset+2];

                    switch (ad) {
                        case 0x10:  // NR10     -PPP NSSS	Sweep period, negate, shift
                            {
                                chan[0+chipoff].sweepPer = (da&0x70)>>4;
                                chan[0+chipoff].sweepShift = (da&0x7);
                                bool neg = (da&0x08) ? true : false;
                                if ((!neg)&&(chan[0+chipoff].sweepNeg)&&(chan[0+chipoff].sweeping)) {
                                    chan[0+chipoff].enabled = false;
                                }
                                chan[0+chipoff].sweepNeg = neg;
                            }
                            break;
                            
                        case 0x11:  // NR11     DDLL LLLL	Duty, Length load (64-L)
                        case 0x16:  // NR21
                            {
                                static bool warn = false;
                                if ((!warn) && ((da&0xc0) != 80)) {
                                    warn = true;
                                    myprintf("Warning: ignoring unsupported duty cycle\n");
                                }

                                int ch = (ad == 0x11) ? chipoff : chipoff+1;
                                // length is also inverted
                                // TODO: I might be 1 clock off, but I think it balances out
                                // with the late processing...
                                chan[ch].length = 63 - (da&0x3f);
                            }
                            break;

                        case 0x12:  // NR12     VVVV APPP	Starting volume, Envelope add mode, period
                        case 0x17:  // NR22
                        case 0x21:  // NR42
                            {
                                int ch = (ad-0x12)/5 + chipoff;
                                if ((chan[ch].volumeDone) && (chan[ch].enabled)) {
                                    // "zombie" mode reacts oddly to changes, incrementing the volume
                                    int diff = 0;
                                    // increment if any bits change
                                    if (chan[ch].volumePer != (da&0x07)) {
                                        ++diff;
                                    }
                                    if (chan[ch].volumeReg != ((da&0xf0)>>4)) {
                                        ++diff;
                                    }
                                    // if working register was zero, double it
                                    // clear that if a non-zero is written, but
                                    // zeros written later do NOT unclear it
                                    if (chan[ch].volumePerWork == 0) {
                                        diff *= 2;
                                        if (da&0x07) {
                                            chan[ch].volumePerWork = da&0x07;
                                        }
                                    }
                                    // and if the direction changed, double it again
                                    if (chan[ch].volumeAdd != ((da&0x08) ? true : false)) {
                                        diff *= 2;
                                    }
                                    // add to the volume, wrapping around
                                    chan[ch].volume = (chan[ch].volume+diff) & 0x0f;
                                } else {
                                    // stop everything if we were sweeping
                                    // increment if any bits change
                                    if (chan[ch].volumePer != (da&0x07)) {
                                        chan[ch].volumeDone=true;
                                    }
                                    if (chan[ch].volumeReg != ((da&0xf0)>>4)) {
                                        chan[ch].volumeDone=true;
                                    }
                                    if (chan[ch].volumeAdd != ((da&0x08) ? true : false)) {
                                        chan[ch].volumeDone=true;
                                    }
                                }
                                // do the set anyway
                                chan[ch].volumeReg = (da&0xf0)>>4;
                                if (chan[ch].volumeReg == 0) {
                                    chan[ch].enabled = false;
                                } else {
                                    // is this right? Seems so! Helps Guile's theme a lot.
                                    chan[ch].enabled = true;
                                }
                                chan[ch].volumeAdd = (da&0x08) ? true : false;
                                chan[ch].volumePer = da&0x07;
                            }
                            break;

                        case 0x13:  // NR13     FFFF FFFF	Frequency LSB
                        case 0x18:  // NR23
                        case 0x1d:  // NR33
                            {
                                // the doc says it's (2048-freq)*4 -- probably for the
                                // 4 phase duty cycle. This might mean the we should translate
                                // to half the count (and I don't know if we need to invert it).
                                // For now, though, try it raw.
                                int ch = (ad-0x13)/5 + chipoff;
                                chan[ch].frequency &= 0xFFff00;
                                chan[ch].frequency |= da;
                            }
                            break;

                        case 0x14:  // NR14     TL-- -FFF	Trigger, Length enable, Frequency MSB
                        case 0x19:  // NR24
                        case 0x1e:  // NR34
                        {
                            int ch = (ad-0x14)/5 + chipoff;

                            // get the frequency byte
                            chan[ch].frequency &= 0xFF00ff;
                            chan[ch].frequency |= ((da&0x07)<<8);

                            // we should process length first, and we need to count it down immediately
                            if ((chan[ch].enabled) && (!chan[ch].lengthEnable) && (da&0x40)) {
                                // rising edge on the length counter
                                if (chan[ch].length != 0) {
                                    if (--chan[ch].length == 0) {
                                        chan[ch].volume = 0;
                                        chan[ch].enabled = false;
                                    }
                                }
                            }
                            chan[ch].lengthEnable = (da&0x40) ? true : false;

                            // then the trigger - any positive write retriggers the channel
                            if (da & 0x80) {
                                // set the trigger flag on noise channels
                                // can't be the noise channel here, but check wave
                                if ((waveMode==1)&&((ch==2)||(ch==6))) chan[ch].frequency |= NOISE_TRIGGER;

                                // set up all the various flags
                                triggerchan(ch);
                            }
                        }
                        break;

                        case 0x1a:  // NR30     E--- ----	DAC power
                            // this is just the enable made explicit
                            if (da & 0x80) {
                                chan[2+chipoff].enabled = true;
                            } else {
                                chan[2+chipoff].enabled = false;
                            }
                            break;

                        case 0x1b:  // NR31     LLLL LLLL	Length load (256-L)
                            {
                                int ch = chipoff+2;
                                // length is also inverted
                                // TODO: I might be 1 clock off, but I think it balances out
                                // with the late processing...
                                chan[ch].length = 255 - (da&0xff);
                            }
                            break;

                        case 0x1c:  // NR32     -VV- ----	Volume code (00=0%, 01=100%, 10=50%, 11=25%)
                            {
                                int ch = chipoff+2;
                                chan[ch].volumeCode = (da&0x60)>>5;
                            }
                            break;

                        case 0x20:  // NR41     --LL LLLL	Length load (64-L)
                            {
                                int ch = chipoff+3;
                                chan[ch].length = da&0x3f;
                            }
                            break;

                        case 0x22:  // NR43     SSSS WDDD	Clock shift, Width mode of LFSR, Divisor code
                            {
                                int shiftrate = (da&0xf0)>>4;
                                bool shift7bit = (da&0x08) ? true : false;
                                int shiftdiv = (da&0x07);

                                if (shiftrate > 13) {
                                    chan[chipoff+3].frequency = 0;
                                } else {

#if 0
                                    // I'm not entirely sure how these apply. Just try just multiplying it out...
                                    if (shiftdiv == 0) {
                                        shiftrate *= 8;
                                    } else {
                                        shiftrate *= (16*shiftdiv);
                                    }
#elif 0
                                    // per http://www.devrs.com/gb/files/hosted/GBSOUND.txt, the
                                    // final frequency(?) is 524288 / shiftdiv / (2^(shiftrate+1))
                                    // with shiftdiv being 0.5 if it's zero.
                                    // 524288 is the clock divided by 8 for the default
                                    // what the heck, we have the CPU...
                                    // but I'd like the understand this...
                                    if (shiftdiv == 0) {
                                        shiftrate = (nClock/8) * 2 / (1 << (shiftrate+1));
                                    } else {
                                        shiftrate = (nClock/8) / shiftdiv / (1 << (shiftrate+1));
                                    }

                                    // that gives us a rate in HZ, make it a rate in clocks...
                                    shiftrate = nClock / shiftrate;
#else
                                    // back to my own interpretation - the initial clock divider is only
                                    // divide-by-two, then we get the configurable divider in shiftdiv,
                                    // literally at shiftdiv+1. The period is twice the shift rate, so
                                    // the maximum is (8*2*2 = 32) same as the tones.
                                    // The shiftdiv clock outputs to the 16-bit shiftrate
                                    // So, the ultimate countdown is (shiftdiv+1)*shiftrate, but this
                                    // is based on a clock 16 times faster than the main clock, so /16.
                                    // This will result in a large range of unreproducable frequencies,
                                    // especially once we add in the the 7-bit rate... perhaps we should
                                    // ignore that. It's not periodic noise, after all.
                                    shiftrate = ((shiftdiv+1)*shiftrate) / 16;
                                    if ((shiftrate == 0)&&(shiftrate != 0)) {
                                        static bool warn = false;
                                        if (!warn) {
                                            warn = true;
                                            printf("Warning: noise frequency exceeds PSG ability.\n");
                                        }
                                    }
#endif

                                    // check for the shorter shift rate
                                    if ((enable7bitnoise) && (shift7bit)) {
                                        // the existing shift rate is for 15 bit.
                                        // we need to make it higher pitched for 7 bit...
                                        // try this...
                                        int oldrate = shiftrate;
                                        shiftrate = (int)((double)shiftrate * (7/15) + 0.5);
                                        if ((shiftrate == 0)&&(oldrate != 0)) {
                                            static bool warn = false;
                                            if (!warn) {
                                                warn = true;
                                                printf("Warning: 7-bit shift caused noise frequency to exceed PSG ability.\n");
                                            }
                                        }
                                    }

                                    chan[chipoff+3].frequency = shiftrate;
                                }
                            }
                            break;

                        case 0x23:  // NR44     TL-- ----	Trigger, Length enable
                        {
                            int ch = chipoff + 3;

                            // we should process length first, and we need to count it down immediately
                            if ((chan[ch].enabled) && (!chan[ch].lengthEnable) && (da&0x40)) {
                                // rising edge on the length counter
                                if (chan[ch].length != 0) {
                                    if (--chan[ch].length == 0) {
                                        chan[ch].volume = 0;
                                        chan[ch].enabled = false;
                                    }
                                }
                            }
                            chan[ch].lengthEnable = (da&0x40) ? true : false;

                            // then the trigger - any positive write retriggers the channel
                            if (da & 0x80) {
                                // set the trigger flag on noise channels
                                // can't be wave channel here, so check noise
                                if ((ch==3)||(ch==7)) chan[ch].frequency |= NOISE_TRIGGER;

                                // set up all the various flags
                                triggerchan(ch);
                            }
                        }
                        break;

                        case 0x24:  // NR50     ALLL BRRR	Vin L enable, Left vol, Vin R enable, Right vol
                            NR50 = da;
                            break;

                        case 0x25:  // NR51     NW21 NW21	Left enables, Right enables
                            NR51 = da;
                            break;

                        case 0x26:  // NR52     P--- NW21	Power control/status, Channel length statuses
                            NR52 = da;
                            break;

                            // wave table
                        case 0x30:  sampleRam[chipoff/4][0]=(da&0xf0)>>4; sampleRam[chipoff/4][1]=(da&0x0f); break;
                        case 0x31:  sampleRam[chipoff/4][2]=(da&0xf0)>>4; sampleRam[chipoff/4][3]=(da&0x0f); break;
                        case 0x32:  sampleRam[chipoff/4][4]=(da&0xf0)>>4; sampleRam[chipoff/4][5]=(da&0x0f); break;
                        case 0x33:  sampleRam[chipoff/4][6]=(da&0xf0)>>4; sampleRam[chipoff/4][7]=(da&0x0f); break;
                        case 0x34:  sampleRam[chipoff/4][8]=(da&0xf0)>>4; sampleRam[chipoff/4][9]=(da&0x0f); break;
                        case 0x35:  sampleRam[chipoff/4][10]=(da&0xf0)>>4; sampleRam[chipoff/4][11]=(da&0x0f); break;
                        case 0x36:  sampleRam[chipoff/4][12]=(da&0xf0)>>4; sampleRam[chipoff/4][13]=(da&0x0f); break;
                        case 0x37:  sampleRam[chipoff/4][14]=(da&0xf0)>>4; sampleRam[chipoff/4][15]=(da&0x0f); break;
                        case 0x38:  sampleRam[chipoff/4][16]=(da&0xf0)>>4; sampleRam[chipoff/4][17]=(da&0x0f); break;
                        case 0x39:  sampleRam[chipoff/4][18]=(da&0xf0)>>4; sampleRam[chipoff/4][19]=(da&0x0f); break;
                        case 0x3a:  sampleRam[chipoff/4][20]=(da&0xf0)>>4; sampleRam[chipoff/4][21]=(da&0x0f); break;
                        case 0x3b:  sampleRam[chipoff/4][22]=(da&0xf0)>>4; sampleRam[chipoff/4][23]=(da&0x0f); break;
                        case 0x3c:  sampleRam[chipoff/4][24]=(da&0xf0)>>4; sampleRam[chipoff/4][25]=(da&0x0f); break;
                        case 0x3d:  sampleRam[chipoff/4][26]=(da&0xf0)>>4; sampleRam[chipoff/4][27]=(da&0x0f); break;
                        case 0x3e:  sampleRam[chipoff/4][28]=(da&0xf0)>>4; sampleRam[chipoff/4][29]=(da&0x0f); break;
                        case 0x3f:  sampleRam[chipoff/4][30]=(da&0xf0)>>4; sampleRam[chipoff/4][31]=(da&0x0f); break;

                        default:
                        {
                            static bool warn = false;
                            if (!debug) {
                                if (!warn) {
                                    warn = true;
                                    myprintf("Warning: ignoring writes to unsupported registers.\n");
                                }
                            } else {
                                printf("Warning: ignoring write to unsupported register: 0x%2X = 0x%02X\n", ad, da);
                            }
                        }
                        break;
                    }
                
                    nOffset+=3;
				}
				break;

            case 0xb4:
				{
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip NES APU skipped\n");
						warn = true;
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
                for (int chip = 0; chip < MAXCHANNELS; chip += 8) {
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
        int outChan = 0;
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
                myprintf("Skipping channel %d - no data\n", ch);
                continue;
            }

            // create a filename
            strcpy(strout, argv[arg]);
            // every channel 6 is a noise channel, every channel 4 might be
            bool isNoise = false;
            if ((ch&7)==6) isNoise = true;
            if (((ch&7)==4) && (waveMode == 1)) isNoise = true;
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
		    myprintf("Writing channel %d as %s...\n", ch, strout);

            for (int r=0; r<nTicks; ++r) {
                fprintf(fp, "0x%08X,0x%02X\n", VGMStream[ch][r], VGMStream[ch+1][r]);
            }

            fclose(fp);
        }
    }

    myprintf("done vgm_gb2psg.\n");
    return 0;
}

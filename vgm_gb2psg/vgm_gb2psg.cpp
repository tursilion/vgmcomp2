// vgm_gb2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input GB DMG input streams, and more
// or less emulate the GB hardware to get PSG data.

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
// each chip uses up 8 channels
#define MAXCHANNELS 16
int VGMStream[MAXCHANNELS][MAXTICKS];		// every frame the data is output

unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
int nCurrentTone[MAXCHANNELS];				// current value for each channel in case it's not updated
double freqClockScale = 1.0;                // doesn't affect noise channel, even though it would in reality
                                            // all chips assumed on same clock
int nTicks;                                 // this MUST be a 32-bit int
bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count

// codes for noise processing (if not periodic, it's white noise)
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000

// options
bool enable7bitnoise = false;           // scale noise frequencies higher when the 7 bit LFSR is active
int waveMode = 0;                       // 0 - treat as tone, 1 - treat as noise, 2 - ignore
bool scaleFreqClock = true;			    // apply scaling from GB clock rates (not an option here)
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;

// the DMG feature clocks use a 512Hz source clock and trigger at 256, 128, and 64 hz
// But, we run at 60hz or 50hz, so we need to convert that to get as close as possible.
// (Or, we can just fake it to 60, 120, and 240, and it'll probably be close enough.)
// But, we'll try to be good. Even though the ratio is irrational.
// So, we'll count in microseconds, cause we're awesome that way
// These defines give us number of microseconds each
#if 1
#define US60 16667
#define US64 15625
#define US128 7812
#define US256 3906
#else
// as an experiment, make everything fixed multiples rather than the 60/64 mismatch...
// it doesn't seem like this makes much difference
#define US60 1
#define US64 1
#define US128 2
#define US256 4
#endif

// max number of chip channels (4 each)
#define MAXCHIP (4*2)

// DMG emulation
class Channel {
public:
    Channel() { reset(); }

    void reset() {
        // set values as per GBSOUND
        enabled = false;
        sweepPer = 0;
        sweepPerWork = 0;
        sweepFreq = 0;
        sweepNeg = false;
        sweepShift = 0;
        sweepClock = 0;
        sweeping = false;
        length = 63;
        lengthClock = 0;
        volume = 0xf;
        volumeAdd = false;
        volumePer = 3;
        volumePerWork = 0;
        volumeClock = 0;
        volumeDone = false;
        frequency = 0xfff;
        lengthEnable = false;
        volumeCode = 1;
    }

    // internal enable flag
    bool enabled;

    // NRx0 - sweep frequency generator
    int sweepPer, sweepPerWork;
    int sweepFreq;
    bool sweepNeg;
    int sweepShift;
    int sweepClock;     // runs at 128Hz 
    bool sweeping;

    // NRx1 - duty and length
    // we don't care about duty, though, and length
    // doesn't need a work register
    int length;
    int lengthClock;    // runs at 256Hz

    // NRx2 - volume envelope generator
    int volume;
    int volumeReg;      // user volume register, needed for 'zombie'
    bool volumeAdd;
    int volumePer, volumePerWork;
    int volumeClock;    // runs at 64Hz
    int volumeCode;     // for the DAC only
    bool volumeDone;    // true after a sweep or if sweep was interrupted, causes odd behaviour

    // NRx3 - Frequency (also some in NRx4 in tones)
    // we don't emulate the frequency playback, so that's all we need
    // getting the shift count for noises is complex, but still this
    // is the only result we need.
    int frequency;

    // NRx4 - trigger, length enable
    // We don't need to store trigger, so just the length enable
    bool lengthEnable;
};

// control registers
int NR50;   // ALLL BRRR	Vin L enable, Left vol, Vin R enable, Right vol
int NR51;   // NW21 NW21	Left enables, Right enables
int NR52;   // P--- NW21	Power control/status, Channel length statuses
Channel chan[MAXCHIP];    // tone, tone, wave, noise, two chips
int sampleRam[2][32];     // sample RAM for 2 chips

// Gameboy is confirmed to meant to be linear (it's not quite on early models,
// but is not fully logarithmic either). We'll use linear.
// TRUE Logarithmic (no gameboy, inverted from TI)
// 0---------------1--------------2-------------3------------4-----------5----------6---------7--------8-------9------A-----B----C---D--E-F
//
// TRUE Linear (later gameboys)
// 0--------1--------2--------3--------4--------5--------6--------7--------8--------9--------A--------B--------C--------D--------E--------F
//
// Semi-Logarithmic (early gameboys)
//
//0-----------1-----------2-----------3----------4---------5---------6---------7--------8-------9-------A-------B------C-----D-----E-----F
// To account for the master volume multipliers, we run this from 0-31 instead
// of 0-255.
unsigned char volumeTable[16] = {
	30,28,26,24,22,20,18,
	16,
	14,12,10,8,6,4,2,0
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
    bool isWave = ((ch == 2) || (ch == 6));
    int chip = ch/4;

    // check master controls
    if ((NR52&0x80) == 0) return 0;                 // power off
    if ((NR51&(0x11<<ch))==0) return 0;             // not routed to either side
    if ((isWave) && (waveMode == 2)) return 0;     // user asked to ignore wave

    // check for mute cases
    if (!chan[ch].enabled) return 0;                // not enabled, no output

    // I think we're good
    int ret = 0;
    if (isWave) {
        // get the average of the wavetable sample
        for (int idx=0; idx<32; ++idx) {
            ret += volumeTable[15-sampleRam[chip][idx]];
        }
        ret /= 32;
        switch (chan[ch].volumeCode) {
            case 0: ret=0; break;       // 0%
            case 1: break;              // 100%
            case 2: ret >>= 1; break;   // 50%
            case 3: ret >>= 2; break;   // 25%
        }
    } else {
        if (chan[ch].volume == 0) return 0;             // muted, no output
        ret = volumeTable[15-(chan[ch].volume&0xf)];
    }

    // now apply the master volume:
    // NR50     ALLL BRRR	Vin L enable, Left vol, Vin R enable, Right
    // NR51     NW21 NW21	Left enables, Right enables
    // "These multiply the signal by (volume+1). The volume step
    // relative to the channel DAC is such that a single channel enabled via
    // NR51 playing at volume of 2 with a master volume of 7 is about as loud
    // as that channel playing at volume 15 with a master volume of 0."
    // I think it's trying to say that 2*7=14 which is about 15*1. Oi.
    // this multiplier is 0-7, for a multiple of 1-8. volumeTable is scaled
    // so that the end result will be 0-255.

    // we need to figure out whether to use the left or right volume -
    // we see which side the channel is on. If both, we will take the
    // louder. It's not efficient code, but just to be simple I'll precalculate
    // all the pieces of data I need to evaluable.
    int leftVol = ((NR50&0x70)>>4)+1;
    int rightVol = (NR50&0x07)+1;
    bool isLeft = (NR51&(0x10<<(ch&3))) ? true : false;
    bool isRight = (NR51&(0x1<<(ch&3))) ? true : false;
    int outVol = 0;
    if (isLeft) outVol = leftVol;
    if ((isRight) && (rightVol > outVol)) outVol = rightVol;
    ret *= outVol;

    return ret;
}

// run the emulation - at this point we always assume 60hz
// This needs to get the nCurrentTone array up to date
void runEmulation() {
    // check power is on
    if (NR52&0x80) {
        // run frequency generator - channel 0 only - 128Hz
        // TODO: untested - SMB doesn't use the sweep
        for (int ch = 0; ch < MAXCHIP; ++ch) {
            if ((ch != 0) && (ch != 4)) continue;   // voice 1 only
            if (!chan[ch].enabled) continue;        // nothing runs when disabled
            if ((chan[ch].sweepPer == 0) && (chan[ch].sweepShift == 0)) continue;
            if (chan[ch].sweepPer == 0) chan[ch].sweepPer = 8;

            chan[ch].sweepClock += US60;
            while (chan[ch].sweepClock >= US128) {
                chan[ch].sweepClock -= US128;

                if (--chan[ch].sweepPerWork < 0) {
                    chan[ch].sweepPerWork = chan[ch].sweepPer;
                    if (chan[ch].sweepShift == 0) {
                        // stop now
                        chan[ch].enabled = false;
                    } else {
                        int work = chan[ch].sweepFreq >> chan[ch].sweepShift;
                        if (chan[ch].sweepNeg) work = -work;
                        chan[ch].sweepFreq += work;
                        if (chan[ch].sweepFreq & ~0x7ff) {
                            chan[ch].enabled = false;
                            chan[ch].sweepFreq &= 0x7ff;
                        } else {
                            chan[ch].frequency = chan[ch].sweepFreq;
                            chan[ch].sweeping = true;    // only has to indicate that we started
                            // second test, not stored
                            work = chan[ch].sweepFreq >> chan[ch].sweepShift;
                            work += chan[ch].sweepFreq;
                            if (work & ~0x7ff) {
                                chan[ch].enabled = false;
                            }
                        }
                    }
                }
            }
        }

        // run length generator - all channels - 256Hz
        // so far no reason to doubt this...
        for (int ch = 0; ch < MAXCHIP; ++ch) {
//          if (!chan[ch].enabled) continue;        // length generator runs even if channel is disabled
            if (!chan[ch].lengthEnable) continue;   // length is not counting
            if (chan[ch].length == 0) continue;     // we've got nowhere else to count

            chan[ch].lengthClock += US60;
            while (chan[ch].lengthClock >= US256) {
                chan[ch].lengthClock -= US256;

                if (--chan[ch].length == 0) {
                    chan[ch].volume = 0;
                    chan[ch].enabled = false;
                }
            }
        }

        // run envelope generator - all channels - 64Hz
        // I think this is right, or at least close...
        for (int ch = 0; ch < MAXCHIP; ++ch) {
            if (!chan[ch].enabled) continue;        // nothing runs when disabled
            if ((ch==2)||(ch==6)) continue;         // not on wave channel
            if (chan[ch].volumePer == 0) chan[ch].volumePer = 8;

            if (!chan[ch].volumeDone) {
                chan[ch].volumeClock += US60;
                while (chan[ch].volumeClock >= US64) {
                    chan[ch].volumeClock -= US64;

                    // did the doc say while the period is NOT zero? that seems odd...
                    if (--chan[ch].volumePerWork < 0) {
                        chan[ch].volumePerWork = chan[ch].volumePer;

                        if (chan[ch].volumeAdd) {
                            chan[ch].volume++;
                            if (chan[ch].volume > 15) {
                                chan[ch].volume = 15;
                                chan[ch].volumeDone = true;
                            }
                        } else {
                            chan[ch].volume--;
                            if (chan[ch].volume < 0) {
                                chan[ch].volume = 0;
                                chan[ch].volumeDone = true;
                            }
                        }
                    }
                }
            }
        }
    } else {
        // no power
        for (int ch = 0; ch < MAXCHIP; ++ch) {
            chan[ch].reset();
        }
        NR50 = 0x77;
        NR51 = 0xf3;
        NR52 = 0x01;
    }

    // update nCurrentTone[]'s
    // the subtraction from 2048 is due to bitwise negation
    for (int ch = 0; ch < MAXCHIP; ++ch) {
        int outch = ch*2;
        if ((ch&3) < 2) {
            // tone frequencies are inverted
            nCurrentTone[outch] = (2048-chan[ch].frequency)&0x7ff;
        } else if ((ch&3) < 3) {
            // wave channel seems to be about the same for SMB at least.
            // for more complex instruments, external tuning can be used
            nCurrentTone[outch] = (2048-chan[ch].frequency)&0x7ff;
        } else {
            // we already did the math for noise...
            nCurrentTone[outch] = chan[ch].frequency;
        }
        nCurrentTone[outch+1] = getVolume(ch);
    }
}

// trigger a channel
void triggerchan(int ch) {
    chan[ch].enabled = true;
    chan[ch].sweepPerWork = chan[ch].sweepPer;
    chan[ch].sweepFreq = chan[ch].frequency&0xfff;
    chan[ch].sweepClock = 0;
    chan[ch].sweeping = false;
    if (chan[ch].length == 0) {
        chan[ch].length = 64;
        if ((ch==2)||(ch==5)) chan[ch].length = 256;
    }
    chan[ch].lengthClock = 0;
    chan[ch].volumePerWork = chan[ch].volumePer;
    chan[ch].volumeClock = 0;
    chan[ch].volume = chan[ch].volumeReg;
    chan[ch].volumeDone = false;
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
                switch(idx%8) {
                    case 0:
                    case 2:
                    case 4: // tones
                        nWork[idx] = 1;     // very high pitched
                        break;

                    case 6: // noise
                        // can't do much for noise, but we can mute it
                        nWork[idx] = 16;    // just a legit pitch
                        nWork[idx+1] = 0;   // mute
                        break;

                    case 1:
                    case 3:
                    case 5:
                    case 7: // volumes
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
	printf("Import VGM DMG (Gameboy) - v20200716\n");

	if (argc < 2) {
		printf("vgm_gb2psg [-q] [-d] [-o <n>] [-add <n>] [-wavenoise|-wavenone] [-enable7bitnoise] [-ignoreweird] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-5)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -wavenoise - treat the wave channel as noise\n");
        printf(" -wavenone - ignore the wave channel (if neither, treat as tone)\n");
        printf(" -enable7bitnoise - normally we don't try to retune 7-bit noise - enable this to try\n");
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
            if ((output > 8) || (output < 1)) {
                printf("output channel must be 1 (square), 2 (square), 3 (wave), or 4 (noise) (5-8 for second chip)\n");
                return -1;
            }
            printf("Output ONLY channel %d: ", output);
            switch(output) {
                case 1: printf("Tone 1\n"); break;
                case 2: printf("Tone 2\n"); break;
                case 3: printf("Wave\n"); break;
                case 4: printf("Noise\n"); break;
            }
		} else if (0 == strcmp(argv[arg], "-wavenoise")) {
			waveMode = 1;
		} else if (0 == strcmp(argv[arg], "-wavenone")) {
			waveMode = 2;
		} else if (0 == strcmp(argv[arg], "-enable7bitnoise")) {
			enable7bitnoise = true;
		} else if (0 == strcmp(argv[arg], "-ignoreweird")) {
			ignoreWeird=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    // global variable init - see also the runEmulation code
    NR50 = 0x77;
    NR51 = 0xf3;
    NR52 = 0xf1;

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

		unsigned int nClock = *((unsigned int*)&buffer[0x80]);
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
					printf("\rWarning: fine timing (%d samples) lost.\n", buffer[nOffset]-0x70+1);
				}
				nRunningOffset+=buffer[nOffset]-0x70+1;
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
    
    // delete all old output files
    {
        char strout[1024];

        // noises
        for (int idx=0; idx<100; ++idx) {
            // create a filename
            sprintf(strout, "%s_noi%02d.60hz", argv[arg], idx);
            // nuke it, if it exists
            remove(strout);
        }
        // tones
        for (int idx=0; idx<100; ++idx) {
            // create a filename
            sprintf(strout, "%s_ton%02d.60hz", argv[arg], idx);
            // nuke it, if it exists
            remove(strout);
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
		    myprintf("-Writing channel %d as %s...\n", ch/2+1, strout);

            for (int r=0; r<nTicks; ++r) {
                fprintf(fp, "0x%08X,0x%02X\n", VGMStream[ch][r], VGMStream[ch+1][r]);
            }

            fclose(fp);
        }
    }

    myprintf("done vgm_gb2psg.\n");
    return 0;
}

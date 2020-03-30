// vgm_pokey2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input PokeyG input streams, and more
// or less emulate the pokey hardware to get PSG data. Noise data can't
// even come close, but we'll try. There was not as much Pokey data
// in VGM form as I thought when I started this one.

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
// each chip uses up 16 channels (because tone and noise are split)
#define MAXCHANNELS 32
int VGMStream[MAXCHANNELS][MAXTICKS];		// every frame the data is output

// nCurrentTone tone,vol,noise,vol, for each of four channels
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
#define NOISE_PERIODIC 0x20000

// options
bool disablePeriodic = false;           // output only white noise when true
bool ignoreHighpass = false;            // ignore the high pass filter when set
bool scaleFreqClock = true;			    // apply scaling from Pokey clock rates (not an option here)
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;
int nReg[MAXCHANNELS];                  // pokey registers, 2 chips

// The Pokey can feed audio with the source clock, or
// the source clock divided by 28 (64Khz) or 114 (16khz).
#define CLKDIV_64K 28
#define CLKDIV_15K 114

// Pokey is apparently linear
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

void runEmulation() {
    // get the registers into nCurrentTone
    // first, from global control, work out our divider for each channel
    for (int chip = 0; chip < MAXCHANNELS; chip+=16) {
        int div[4] = { CLKDIV_64K, CLKDIV_64K, CLKDIV_64K, CLKDIV_64K };

        // AUDCTL:
	    // 1xxxxxxx --	17-bit noise becomes only 8-bit noise (PSG ignore)
	    // x1xxxxxx -- channel one clock is 1.79MHz (override main clock)
	    // xx1xxxxx -- channel three clock is 1.79MHz (override main clock)
	    // xxx1xxxx -- joins 2 to 1 for 16-bit resolution (little endian)
	    // xxxx1xxx -- joins channel 4 to 3 for 16-bit resolution (little endian)
	    // xxxxx1xx -- channel 1 high pass filter (only freqs higher than 3 play (more or less))
	    // xxxxxx1x -- channel 2 high pass filter (only freqs higher than 4 play (more or less))
	    // xxxxxxx1 -- main clock changes from 64Khz (default clock) to 15Khz

        if (nReg[8+chip]&0x01) {
            // clock is 15k, not 64
            for (int idx=0; idx<4; ++idx) {
                div[idx] = CLKDIV_15K;
            }
        }
        // check exceptions
        if (nReg[8+chip]&0x40) {
            // clock 1 is full rate
            div[0] = 1;
            if (nReg[8+chip]&0x10) {
                // clock 2 slaves to clock 1
                div[1] = 1;
            }
        }
        if (nReg[8+chip]&0x20) {
            // clock 3 is full rate
            div[2] = 1;
            if (nReg[8+chip]&0x08) {
                // clock 4 slaves to clock 3
                div[3] = 1;
            }
        }

        // just for simplicity, I'll do each channel manually. There are only 4
        int nClk[4], nVol[4];

        // Channel 1
        if (nReg[1+chip]&0x10) {
            // DAC mode - we mute that
            nVol[0] = 0;
        } else {
            if (nReg[8+chip] & 0x10) {
                // 16-bit mode - TODO: so does it mute automatically?
                nClk[0] = nReg[0+chip];
                nVol[0] = 0;
            } else {
                nClk[0] = nReg[0+chip];
                // get volume
                nVol[0] = nReg[1+chip]&0x0f;
            }
            // apply divider (inverted to get count at full clock)
            nClk[0] *= div[0];
        }

        // Channel 2
        if (nReg[3+chip]&0x10) {
            // DAC mode - we mute that
            nVol[1] = 0;
        } else {
            if (nReg[8+chip] & 0x10) {
                // 16-bit mode - LSB from chan 1
                nClk[1] = nReg[0+chip] | (nReg[2+chip] << 8);
                // get volume
                nVol[1] = nReg[3+chip]&0x0f;
            } else {
                nClk[1] = nReg[2+chip];
                // get volume
                nVol[1] = nReg[3+chip]&0x0f;
            }
            // apply divider (inverted to get count at full clock)
            nClk[1] *= div[1];
        }

        // Channel 3
        if (nReg[5+chip]&0x10) {
            // DAC mode - we mute that
            nVol[2] = 0;
        } else {
            if (nReg[8+chip] & 0x08) {
                nClk[2] = nReg[4+chip];
                // 16-bit mode - TODO: so does it mute automatically?
                nVol[2] = 0;
            } else {
                nClk[2] = nReg[4+chip];
                // get volume
                nVol[2] = nReg[5+chip]&0x0f;
            }
            // apply divider (inverted to get count at full clock)
            nClk[2] *= div[2];
        }

        // Channel 4
        if (nReg[7+chip]&0x10) {
            // DAC mode - we mute that
            nVol[3] = 0;
        } else {
            if (nReg[8+chip] & 0x08) {
                // 16-bit mode - LSB from chan 3
                nClk[3] = nReg[4+chip] | (nReg[6+chip] << 8);
                // get volume
                nVol[3] = nReg[7+chip]&0x0f;
            } else {
                nClk[3] = nReg[6+chip];
                // get volume
                nVol[3] = nReg[7+chip]&0x0f;
            }
            // apply divider (inverted to get count at full clock)
            nClk[3] *= div[3];
        }

        // apply the high pass filters, if they are enabled
        if (!ignoreHighpass) {
            if (nReg[8+chip]&0x04) {
                // channel 1 must be higher than 3
                if (nClk[0] < nClk[2]) {
                    nVol[0] = 0;
                }
            }
            if (nReg[8+chip]&0x02) {
                // channel 2 must be higher than 4
                if (nClk[1] < nClk[3]) {
                    nVol[1] = 0;
                }
            }
        }

        // apply each channel to tone or noise as needed
        for (int idx = 0; idx < 4; ++idx) {
            int offset = idx * 4;
            int regCtrl = nReg[idx*2+1+chip];

            if ((regCtrl&0xa0) == 0xa0) {
                // this is a pure tone, no need to preserve the trigger
                nCurrentTone[offset+chip] = nClk[idx];   // tone side
                nCurrentTone[offset+chip+1] = volumeTable[15-(nVol[idx]&0xf)];
                nCurrentTone[offset+chip+2] = -1;        // noise side
                nCurrentTone[offset+chip+3] = -1;
            } else {
                // this is a noise, maybe periodic
                nCurrentTone[offset+chip] = -1;         // tone side
                nCurrentTone[offset+chip+1] = -1;
                if (nCurrentTone[offset+chip+2] == -1) nCurrentTone[offset+chip+2]=0;
                nCurrentTone[offset+chip+2] &= ~NOISE_MASK;
                nCurrentTone[offset+chip+2] |= nClk[idx]&0xfff;
                nCurrentTone[offset+chip+3] = volumeTable[15-(nVol[idx]&0x0f)];
                if ((disablePeriodic) || ((regCtrl&0xe0)==0) || ((regCtrl&0xe0)==0x80)) {
                    // white noise
                } else {
                    // periodic noise
                    nCurrentTone[offset+chip+2] &= ~NOISE_MASK;
                    nCurrentTone[offset+2] |= NOISE_PERIODIC;
                    nCurrentTone[offset+chip+2] |= (nClk[idx]/16)&0xfff;
                }
            }
        }
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

        // update the emulation - need to run it early because of the
        // place that we set the noise trigger
        runEmulation();

        // reload the work regs
        for (int idx=0; idx<MAXCHANNELS; ++idx) {
            nWork[idx] = nCurrentTone[idx];
        }

        for (int idx=0; idx<MAXCHANNELS; idx++) {
		    if (nWork[idx] == -1) {         // no entry yet
                if (idx&1) {
                    // volume - so mute it
                    nWork[idx] = 0;     // mute
                } else {
                    // tone (or noise), so high pitch it
                    nWork[idx] = 1;     // very high pitched
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

        // now that it's output, clear any noise triggers
        for (int idx=0; idx<MAXCHANNELS; ++idx) {
            if (nCurrentTone[idx] != -1) {
                nCurrentTone[idx] &= ~NOISE_TRIGGER;
            }
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
	printf("Import VGM Pokey - v20200328\n");

	if (argc < 2) {
		printf("vgm_pokey2psg [-q] [-d] [-o <n>] [-disableperiodic] [-ignorehighpass] [-ignoreweird] <filename>\n");
        printf(" -ignorehighpass - ignores the high pass filter bits\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -disableperiodic - disables periodic noise, only white noise output\n");
        printf(" -ignorehighpass - ignore the highpass bit and output all frequencies\n");
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
            if ((output > 16) || (output < 1)) {
                printf("output channel must be 1-8. (9-16 for second chip).\nTones and noises are split: odd channels are tone, even channels are noise.\n");
                return -1;
            }
            printf("Output ONLY channel %d: %s%d", debug, output&1?"Tone":"Noise", output/2);
        } else if (0 == strcmp(argv[arg], "-disableperiodic")) {
            disablePeriodic = true;
        } else if (0 == strcmp(argv[arg], "-ignorehighpass")) {
            ignoreHighpass = true;
		} else if (0 == strcmp(argv[arg], "-ignoreweird")) {
			ignoreWeird=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

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
            printf("Failure - Pokey not supported earlier than version 1.61\n");
            return 1;
        }

		unsigned int nClock = *((unsigned int*)&buffer[0xB0]);
        if (nClock&0x40000000) {
            // bit indicates dual GB chips - though I don't need to do anything here
            // the format assumes that all GBs are identical
            myprintf("Dual Pokey output specified (we shall see!)\n");
        }
		nClock&=0x0FFFFFFF;
        if (nClock == 0) {
            printf("\rThis VGM does not appear to contain a Pokey. This tool extracts only Pokey data.\n");
            return -1;
        }
        if (nClock != 0) {
		    if ((nClock < 1789772-50) || (nClock > 1789772+50)) {
			    freqClockScale = 1789772/nClock;
			    printf("\rUnusual Pokey clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }
            // now adapt for the ratio between Pokey and PSG, which appears faster at 3579545.0 Hz.
            // The divider is more than that, though. The PSG counts at 111860.8Hz after a divide
            // by 32, while the Pokey is capable of counting at full clock rate (on two channels
            // anyway.) So, it's technically 111860.8/1789772. Hopefully we won't see too much
            // rounding error... multiply by 2 cause it's 2 clocks per cycle. ;)
            freqClockScale *= (111860.8*2) / 1789772;
            myprintf("\rPSG clock scale factor %f\n", freqClockScale);
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

        // The shift register for noise is quite complex on this chip, so... we
        // are just going to futz it quite a lot, I fear.

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
        // zero the registers
        for (int idx=0; idx<9*2; ++idx) {
            nReg[idx] = 0;
        }
        runEmulation();
		nTicks= 0;
		bool delaywarn = false;			// warn about imprecise delay conversion

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
					static bool warn = false;
					if (!warn) {
						printf("\rUnsupported chip PWM skipped\n");
						warn = true;
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
                    // Pokey: 0xBB AA DD
                    // AA is the register, +0x80 for chip 2
                    // DD is the byte to write

                    int chipoff = 0;
                    if (buffer[nOffset+1]>0x7f) chipoff = 16;

                    // Registers we care about are 0-8
                    unsigned char ad = (buffer[nOffset+1]&0x7f);
                    unsigned char da = buffer[nOffset+2];

                    switch (ad) {
                        case 0:     // audio frequency 1
                            nReg[ad+chipoff] = da;
                            break;

                        case 1:     // audio control 1 - nnnDvvvv
                            // trigger if the noise changes
                            if ((da&0xe0) != (nReg[ad+chipoff]&0xe0)) {
                                nCurrentTone[chipoff+2] = NOISE_TRIGGER;
                            }
                            nReg[ad+chipoff] = da;
                            break;

                        case 2:     // audio frequency 2
                            nReg[ad+chipoff] = da;
                            break;

                        case 3:     // audio control 2 - nnnDvvvv
                            // trigger if the noise changes
                            if ((da&0xe0) != (nReg[ad+chipoff]&0xe0)) {
                                nCurrentTone[chipoff+6] = NOISE_TRIGGER;
                            }
                            nReg[ad+chipoff] = da;
                            break;

                        case 4:     // audio frequency 3
                            nReg[ad+chipoff] = da;
                            break;

                        case 5:     // audio control 3 - nnnDvvvv
                            // trigger if the noise changes
                            if ((da&0xe0) != (nReg[ad+chipoff]&0xe0)) {
                                nCurrentTone[chipoff+10] = NOISE_TRIGGER;
                            }
                            nReg[ad+chipoff] = da;
                            break;

                        case 6:     // audio frequency 4
                            nReg[ad+chipoff] = da;
                            break;

                        case 7:     // audio control 4 - nnnDvvvv
                            // trigger if the noise changes
                            if ((da&0xe0) != (nReg[ad+chipoff]&0xe0)) {
                                nCurrentTone[chipoff+14] = NOISE_TRIGGER;
                            }
                            nReg[ad+chipoff] = da;
                            break;

                        case 8:     // Pokey control - 8b-c1-c3-21-43-f1-f2-64
                            nReg[ad+chipoff] = da;
                            break;

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
                for (int chip = 0; chip < MAXCHANNELS; chip += 4) {
                    // not really 'chip', but every 4 channels is one physical channel, and every
                    // channel 3 is the noise version of that channel
                    // tone, vol, noise, vol
				    for (int idx=0; idx<nTicks; idx++) {
                        if (VGMStream[0+chip][idx] > 1) {
					        VGMStream[0+chip][idx] = (int)(VGMStream[0+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[0+chip][idx] == 0)    { VGMStream[0+chip][idx]=1;     clip++; }
					        if (VGMStream[0+chip][idx] > 0xfff) { 
                                VGMStream[0+chip][idx]=0xfff; clip++; 
                            }
                        }

                        // this might have triggers in it, as it might be noise
                        if (VGMStream[2+chip][idx] > 1) {
                            bool trig = (VGMStream[2+chip][idx]&NOISE_TRIGGER) ? true : false;
                            bool per = (VGMStream[2+chip][idx]&NOISE_PERIODIC) ? true : false;
                            VGMStream[2+chip][idx] &= NOISE_MASK;
					        VGMStream[2+chip][idx] = (int)(VGMStream[2+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[2+chip][idx] == 0)    { VGMStream[2+chip][idx]=1;     clip++; }
					        if (VGMStream[2+chip][idx] > 0xfff) { VGMStream[2+chip][idx]=0xfff; clip++; }
                            if (trig) VGMStream[2+chip][idx] |= NOISE_TRIGGER;
                            if (per) VGMStream[2+chip][idx] |= NOISE_PERIODIC;
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
    // [ch] the channel, and is tone/vol/noise/vol x4
    // we just write out tone channels as <name>_tonX.60hz
    // the noise channels are named <name>_noiX.60hz
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
                if (((ch&3)==2) && (VGMStream[ch+1][r] > 0)) {
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
            // every channel 2 is a noise channel
            bool isNoise = false;
            if ((ch&3)==2) isNoise = true;
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

    myprintf("done vgm_pokey2psg.\n");
    return 0;
}

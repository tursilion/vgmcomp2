// vgm_md2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input MegaDrive (MD) YM2612 input streams, 
// and uses the excellent Nuked-OPN2 to get an overkill amount of accuracy,
// which we then decimate to the output here. ;)

// Currently, we set all channels (even noise) to countdown values
// compatible with the SN clock
// All volume is set to 8 bit values. This lets us take in a wider
// range of input datas, and gives us more flexibility on the output.

// It's a little difficult to determine what an output frequency
// on an FM chip actually is, so for now we are treating the system
// as having six channels with a master (maybe tuned) frequency
// each. We'll do our best - but a lot of the real sound chip data
// is necessarily thrown away. Hopefully we can keep some of the
// vibrato and other effects.

// Currently data is stored in nCurrentTone[] with even numbers being
// tones and odd numbers being volumes. DAC is always treated as noise
// since otherwise we have no way to get a frequency for it.

// Tones: scales to SN countdown rates, even on noise, 0 to 0xfff (4095) (other chips may go higher!)
// Volume: 8-bit linear (NOT logarithmic) with 0 silent and 0xff loudest
// Note that /either/ value can be -1 (integer), which means never set (these won't land in the stream though)

#include <stdio.h>
#include <stdarg.h>
#define MINIZ_HEADER_FILE_ONLY
#include "tinfl.c"						    // comes in as a header with the define above
										    // here to allow us to read vgz files as vgm
#include "ym3438.h"
#define MAXTICKS 432000					    // about 2 hrs, but arbitrary

// in general even channels are tone (or noise) and odd are volume
// They are mapped to PSG frequency counts and 8-bit volume (0=mute, 0xff=max)
// So that's 12 channels for the FM. In addition, the optional DAC channel, if
// it is active, will be channels 13/14 as noise.
// each chip (up to two) uses up 14 channels
#define MAXCHANNELS 28
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
unsigned char dataBank[65536];              // for PCM data
unsigned int dataBankPtr = 0;               // current pointer into it
unsigned int dataBankLen = 0;               // how much is stored
int dacAvg = 0;                             // fake the DAC level on this side

// codes for noise processing (if not periodic, it's white noise)
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000

// options
bool scaleFreqClock = true;			    // apply scaling from GB clock rates (not an option here)
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;
unsigned int nClock = 7670454;          // default Mega drive clock

// max number of chip channels (7 each - 6 FM and 1 DAC/noise)
#define MAXCHIP (7*2)

// YM emulation
ym3438_t soundChip[2];

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

// run one 60hz tick of emulation
void runEmulation() {
    // the OPN function runs one internal clock, which is 6 master clocks,
    // and we need 1/60s.
    // nClock contains the master clock in Hz. Divide by 6 for internal clocks,
    // then divide by 60 for fraction.
    Bit16s buffer[2];       // dummy output buffer for current levels
    int cycles = int(nClock/6.0/60.0+0.5);
    for (int idx=0; idx<cycles; ++idx) {
        OPN2_Clock(&soundChip[0], buffer);
        OPN2_Clock(&soundChip[1], buffer);
    }

    // now we are supposed to fill in the nCurrentTone[] fields based on the
    // state of the chip
    // noise is somewhat easy - we are faking it on this side. Only chip 0 DAC
    // is supported in the VGMs.
    if (somehow check if DAC 0 is active) {
        nCurrentTone[13] = dacAvg;
        if (dacAvg > 0) {
            nCurrentTone[12] = 16;     // just any noise pitch
        }
    }

    // then get every channel's frequency

    // and current volume


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
                switch(idx%14) {
                    case 0:
                    case 2:
                    case 4: // tones
                    case 6:
                    case 8:
                    case 10:
                        nWork[idx] = 1;     // very high pitched
                        break;

                    case 12: // noise (DAC)
                        // can't do much for noise, but we can mute it
                        nWork[idx] = 16;    // just a legit pitch
                        nWork[idx+1] = 0;   // mute
                        break;

                    case 1:
                    case 3:
                    case 5:
                    case 7: // volumes
                    case 9:
                    case 11:
                    case 13:
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
    }
    dacAvg = 0;     // zeroed here in case we need it twice

    return true;
}

int main(int argc, char* argv[])
{
	printf("Import VGM MD (MegaDrive/Genesis) - v20200926\n");

	if (argc < 2) {
		printf("vgm_gb2psg [-q] [-d] [-o <n>] [-add <n>] [-ignoreweird] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-5)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
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
            if ((output > 14) || (output < 1)) {
                printf("output channel must be 1-6 (FM) or 7 (DAC), or 8-14 for second chip.\n");
                return -1;
            }
            printf("Output ONLY channel %d\n", output);
		} else if (0 == strcmp(argv[arg], "-ignoreweird")) {
			ignoreWeird=true;
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    // global variable init - see also the runEmulation code
    OPN2_SetChipType(ym3438_mode_ym2612);   // global for all chips
    OPN2_Reset(&soundChip[0]);
    OPN2_Reset(&soundChip[1]);

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

		// -Split a VGM file into multiple channels
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
        if (nVersion < 0x110) {
            printf("Failure - MD not supported earlier than version 1.10\n");
            return 1;
        }

		nClock = *((unsigned int*)&buffer[0x2c]);
        if (nClock&0x40000000) {
            // bit indicates dual chips - though I don't need to do anything here
            myprintf("Dual MD output specified (we shall see!)\n");
        }
		nClock&=0x0FFFFFFF;
        if (nClock == 0) {
            printf("\rThis VGM does not appear to contain a YM2612 (MegaDrive). This tool extracts only YM2612 (MegaDrive) data.\n");
            return -1;
        }
        if (nClock != 0) {
		    if ((nClock < 7670454-50) || (nClock > 7670454+50)) {
			    freqClockScale = 7670454.0/nClock;
			    printf("\rUnusual YM2612 clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }
            // now adapt for the ratio between MD and PSG, which clocks at 3579545.0 Hz
            // The PSG counts through a divide-by-32, giving it a count of 111860.8Hz.
            // The YM2612 seems to have a divide by 6...??? I'm not entirely sure
            // this is even how I'll extract frequency yet...
            freqClockScale *= (3579545.0/32) / (7670454.0/6);
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
                // YM2612 port 0 (first 3 channels and globals)
                OPN2_Write(&soundChip[0], 0, buffer[nOffset+1]);    // set address
                OPN2_Write(&soundChip[0], 1, buffer[nOffset+2]);    // set data
                nOffset+=3;
                break;
			case 0x53:
                // YM2612 port 1 (second 3 channels)
                OPN2_Write(&soundChip[0], 2, buffer[nOffset+1]);    // set address
                OPN2_Write(&soundChip[0], 3, buffer[nOffset+2]);    // set data
                nOffset+=3;
                break;

            // second chip
            case 0xa2:
                // YM2612 port 0 (first 3 channels and globals)
                OPN2_Write(&soundChip[1], 0, buffer[nOffset+1]);    // set address
                OPN2_Write(&soundChip[1], 1, buffer[nOffset+2]);    // set data
                nOffset+=3;
                break;
			case 0xa3:
                // YM2612 port 1 (second 3 channels)
                OPN2_Write(&soundChip[1], 2, buffer[nOffset+1]);    // set address
                OPN2_Write(&soundChip[1], 3, buffer[nOffset+2]);    // set data
                nOffset+=3;
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
                    // data block - copy into dataBank
                    int type = buffer[nOffset+2];
                    unsigned int size = (*((unsigned int*)&buffer[nOffset+3]));
                    if (type == 0) {
                        // YM2612 PCM data - not supporting 0x40, which is compressed
                        if (dataBankLen+size > sizeof(dataBank)) {
                            printf("Too much PCM data, unable to parse.\n");
                            return -1;
                        }
                        memcpy(&dataBank[dataBankLen], &buffer[nOffset+7], size);
                        dataBankLen += size;
                    }
					nOffset += size+7;
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
                // YM2612 DAC - write from data bank, then wait n samples
                // first chip only!
                // I'm not convinced DAC will do much any good with this...
                OPN2_Write(&soundChip[0], 0, 0x2a); // set register address
                // fake the dac level
                dacAvg = (dacAvg + dataBank[dataBankPtr]) / 2;
                OPN2_Write(&soundChip[0], 1, dataBank[dataBankPtr++]);  // set data
                if (dataBankPtr >= sizeof(dataBank)) {
                    printf("WARNING: PCM buffer exceeded\n");
                    dataBankPtr=0;
                }
				// try the same hack as above
				if (nRunningOffset == 0) {
					printf("\rWarning: fine timing (%d samples) lost.\n", buffer[nOffset]-0x70+1);
				}
				nRunningOffset+=buffer[nOffset]-0x80;   // note: NOT +1 this time, docs call out explicitly
				if (nRunningOffset > ((nRate==60)?735:882)) {
					nRunningOffset -= ((nRate==60)?735:882);
                    if (!outputData()) return -1;				
				}
				nOffset++;
				break;

            // YM2612 CAN use this DAC stream
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
						printf("\rUnsupported gameboy DMG skipped\n");
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
                // pcm seek
                dataBankPtr = (*((unsigned int*)&buffer[nOffset+1]));
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

        // TODO: not certain this needs to be done here... depends on how I get the frequency...
		if (scaleFreqClock) {
			int clip = 0;
			if (freqClockScale != 1.0) {
				myprintf("Adapting tones for PSG clock rate... ");
				// scale every tone - clip if needed (note further clipping for PSG may happen at output)
                // this will include the noise channel
                for (int chip = 0; chip < MAXCHANNELS; chip += 2) {
				    for (int idx=0; idx<nTicks; idx++) {
                        if (VGMStream[0+chip][idx] > 1) {
					        VGMStream[0+chip][idx] = (int)(VGMStream[0+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[0+chip][idx] == 0)    { VGMStream[0+chip][idx]=1;     clip++; }
					        if (VGMStream[0+chip][idx] > 0xfff) { VGMStream[0+chip][idx]=0xfff; clip++; }
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
    // [ch] the channel, and is tone/vol/tone/vol/tone/vol/tone/vol/tone/vol/tone/vol/noise/vol
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
                if (((ch%14)==12) && (VGMStream[ch+1][r] > 0)) {
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
            // every channel 12 is a noise channel
            bool isNoise = false;
            if ((ch%14)==12) isNoise = true;
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

    myprintf("done vgm_md2psg.\n");
    return 0;
}

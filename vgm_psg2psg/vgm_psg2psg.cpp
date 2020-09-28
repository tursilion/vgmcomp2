// vgm_psg2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input PSG input streams.

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
#define FIRSTPSG 0
#define SECONDPSG 8

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

// codes for noise processing (if not periodic (types 0-3), it's white noise (types 4-7))
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000
#define NOISE_MAPCHAN3 0x40000

// options
bool noTuneNoise = false;               // don't retune channel three for noises
bool scaleFreqClock = true;			    // apply scaling for unexpected clock rates
bool ignoreWeird = false;               // ignore any other weirdness (like shift register)
unsigned int nRate = 60;

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
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

        for (int base = 0; base < MAXCHANNELS; base += 8) {
            // don't process if it's never been set
            if (nWork[base+6] == -1) continue;
            // if the noise channel needs to use channel 3 for frequency...
            if (nWork[base+6]&NOISE_MAPCHAN3) {
                // remember the flag - we do final tuning at the end if needed
                nWork[base+6]=(nWork[base+6]&(~NOISE_MASK)) | (nWork[base+4]&NOISE_MASK) | NOISE_MAPCHAN3;   // gives a frequency for outputs that need it
            }
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
	printf("Import VGM PSG - v20200928\n");

	if (argc < 2) {
		printf("vgm_psg2psg [-q] [-d] [-o <n>] [-add <n>] [-notunenoise] [-noscalefreq] [-ignoreweird] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-5)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
		printf(" -notunenoise - Do not retune for noise (normally autodetected)\n");
		printf(" -noscalefreq - do not apply frequency scaling if non-NTSC (normally automatic)\n");
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
                printf("output channel must be 1-3 (tone), or 4 (noise) (5-8 for second chip)\n");
                return -1;
            }
            printf("Output ONLY channel %d: ", output);
            switch(output) {
                case 1: printf("Tone 1\n"); break;
                case 2: printf("Tone 2\n"); break;
                case 3: printf("Tone 3\n"); break;
                case 4: printf("Noise\n"); break;
            }
		} else if (0 == strcmp(argv[arg], "-notunenoise")) {
			noTuneNoise=true;
		} else if (0 == strcmp(argv[arg], "-noscalefreq")) {
			scaleFreqClock=false;
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

		unsigned int nClock = *((unsigned int*)&buffer[12]);
        if (nClock&0x80000000) {
            // high bit indicates NeoGeo Pocket version?
            printf("Warning: ignoring T6W28 (NGP) specific PSG settings.\n");
        }
        if (nClock&0x40000000) {
            // bit indicates dual PSG chips - though I don't need to do anything here
            // the format assumes that all PSGs are identical
            myprintf("Dual PSG output specified (we shall see!)\n");
        }
		nClock&=0x0FFFFFFF;
		double nNoiseRatio = 1.0;
        if (nClock == 0) {
            printf("\rThis VGM does not appear to contain a PSG. This tool extracts only PSG data.\n");
            return -1;
        }
        if (nClock != 0) {
		    if ((nClock < 3579545-50) || (nClock > 3579545+50)) {
			    freqClockScale = 3579545.0/nClock;
			    printf("\rUnusual PSG clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
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
		unsigned int nShiftRegister=16;     // default if not specified
		if (nVersion >= 0x110) {
			nShiftRegister = buffer[0x2a];
			if ((nShiftRegister != 0)&&(nShiftRegister!=16)&&(nShiftRegister!=15)) {
				printf("\rweird shift register %d - only 15 or 16 bit supported\n", nShiftRegister);
                if (ignoreWeird) {
                    printf("  .. ignoring as requested - treating as 16 bit\n");
                    nShiftRegister = 16;
                } else {
    				return -1;
                }
			}
		}
		// TI/Coleco has a 15 bit shift register, so if it's 16 (default), scale it down
        // again, both PSGs are assumed the same, and the AY only has one possibility
		if (nShiftRegister == 16) {
			nNoiseRatio *= 15.0/16.0;   // shift it down to the TI version 15-bit shift register
			myprintf("Selecting 16-bit shift register.\n");
		}

        // check the extra flags. The only one we might care about is bit 0 - frequency 0 is 0x400
        // but, we just assume we can't rely on it anyway
        if (nVersion >= 0x151) {
            if (buffer[0x2b] != 0) {
                printf("Warning: ignoring SN76489 flags\n");
            }
        }

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
		int nCmd=0;
        int lastCmd[2] = { 0, 0 };
		bool delaywarn = false;			// warn about imprecise delay conversion

		// Use nRate - if it's 50, add one tick delay every 5 frames
		// If user-defined noise is used, multiply voice 3 frequency by nNoiseRatio
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
                    // second chip check...
                    int chipoff = 0;
                    if (buffer[nOffset] == 0x30) chipoff = 8;

					// here's the good stuff!
					unsigned char c = buffer[nOffset+1];

                    if (debug) printf("[%d]=%02x ", chipoff, c);

					if (c&0x80) {
						// it's a command byte - update the byte data
						nCmd=(c&0x70)>>4;
						c&=0x0f;

                        if (chipoff) {
                            lastCmd[1] = nCmd;
                        } else {
                            lastCmd[0] = nCmd;
                        }

						// save off the data into the appropriate stream
						switch (nCmd) {
						case 1:		// vol0
						case 3:		// vol1
						case 5:		// vol2
						case 7:		// vol3
							// single byte (nibble) values
  							nCurrentTone[nCmd+chipoff]=volumeTable[c];
							break;

						case 6:		// noise
							// single byte (nibble) values, masked to indicate a retrigger
                            // put in the actual countdown clock
							nCurrentTone[nCmd+chipoff]=NOISE_TRIGGER;
                            if ((c&0x4)==0) nCurrentTone[nCmd+chipoff]|=NOISE_PERIODIC;
                            switch (c&0x3) {
                                case 0: // 6991Hz
                                    nCurrentTone[nCmd+chipoff]|=(int)(111860.9/6991+.5);   // 16
                                    break;
                                case 1: // 3496Hz
                                    nCurrentTone[nCmd+chipoff]|=(int)(111860.9/3496+.5);   // 32
                                    break;
                                case 2: // 1748Hz
                                    nCurrentTone[nCmd+chipoff]|=(int)(111860.9/1748+.5);   // 64
                                    break;
                                case 3: // map to channel 3
                                    nCurrentTone[nCmd+chipoff]|=NOISE_MAPCHAN3;
                                    break;
                            }

							break;

						default:	
							// tone command, least significant nibble
                            if (nCurrentTone[nCmd+chipoff] == -1) nCurrentTone[nCmd+chipoff]=0;
							nCurrentTone[nCmd+chipoff]&=0xff0;
							nCurrentTone[nCmd+chipoff]|=c;
							break;
						}
					} else {
						// non-command byte, use the previous command (from the right chip) and update
                        if (chipoff) {
                            nCmd = lastCmd[1];
                        } else {
                            nCmd = lastCmd[0];
                        }

                        if (nCurrentTone[nCmd+chipoff] == -1) nCurrentTone[nCmd+chipoff]=0;
                        if (nCmd & 0x01) {
                            // volume
                            nCurrentTone[nCmd+chipoff]=volumeTable[c&0x0f];
                        } else {
                            // tone
    						nCurrentTone[nCmd+chipoff]&=0x00f;
	    					nCurrentTone[nCmd+chipoff]|=(c&0xff)<<4;
                        }
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
						printf("\rUnsupported chip Gameboy DMG skipped\n");
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

		if ((nShiftRegister != 15) && (!noTuneNoise)) {
			int cnt=0;
			myprintf("Adapting user-defined shift rates...");
			// go through every frame -- if the noise channel is user defined (PSG only),
			// adapt the tone by nRatio
            // Note we don't NEED to retune channel 3 now, so we don't, just the noise channel.
            // Reconciling this is the output tool's job.
            for (int chip = 0; chip < MAXCHANNELS; chip += 8) {
			    for (int idx=0; idx<nTicks; idx++) {
				    if (VGMStream[6+chip][idx]&NOISE_MAPCHAN3) {
					    if (VGMStream[5+chip][idx] != 0) {
						    // we can hear it! Will still tune, warn the user
						    static int warned = false;
						    if (!warned) {
							    if (VGMStream[7+chip][idx] != 0) {
								    warned=true;
								    printf("\nSong employs user-tuned noise channel, but both noise and voice are audible.\n");
								    printf("Noise and voice will be at different shift rates and one may sound off. Use\n");
								    printf("a 15-bit noise channel if your tracker supports it!\n");
							    }
						    }
					    }
                        // tune it anyway on the noise channel only - another tool's job to sort this out now
                        // this scale is verified on hardware!
                        // We finally remove the map flag, we're done remapping it. That flag doesn't escape the tool.
                        // But, we do need to preserve the trigger and periodic flags
  						VGMStream[6+chip][idx]=((int)(((VGMStream[6+chip][idx]&NOISE_MASK)/nNoiseRatio)+0.5)) |
                                                (VGMStream[6+chip][idx]&(NOISE_PERIODIC|NOISE_TRIGGER));
				    }
			    }
            }
			myprintf("%d notes tuned.\n", cnt);
		}

		if (scaleFreqClock) {
			int clip = 0;
			// debug is emitted earlier - a programmable sound chip rate, like the F18A gives,
			// could playback at the correct frequencies, otherwise we'll just live with the
			// detune. The only one I have is 680 Rock (which /I/ made) and it's 2:1
			if (freqClockScale != 1.0) {
				myprintf("Adapting tones for unusual clock rate... ");
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
					        VGMStream[4+chip][idx] = (int)(VGMStream[4+chip][idx] * freqClockScale + 0.5);
                            if (VGMStream[4+chip][idx] == 0)    { VGMStream[4+chip][idx]=1;     clip++; }
					        if (VGMStream[4+chip][idx] > 0xfff) { VGMStream[4+chip][idx]=0xfff; clip++; }
                        }
				    }
                }
				if (clip > 0) myprintf("%d tones clipped");
				myprintf("\n");
			}
		} else {
            myprintf("Skipping frequency scale...");
        }
	}

    // delete all old output files, unless -add was specified
    if (addout == 0) {
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
            // every channel 6 is a noise channel
            if ((ch&7)==6) {
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

    myprintf("done vgm_psg2psg.\n");
    return 0;
}

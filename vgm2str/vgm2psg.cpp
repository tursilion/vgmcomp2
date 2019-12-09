// vgm2psg.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and outputs raw 60hz streams for the
// TI PSG sound chip. It will input PSG or AY/YM input streams.

// vgmcomp2.cpp : Defines the entry point for the console application.

// TODO: the AY import is kind of weak, no envelopes and no testing,
// mostly because I don't think there are a lot of AY/YM tunes in
// VGM format out there...

// Currently, we set all channels (even noise) to countdown values
// compatible with the SN clock (we assume the same countdowns work
// on the default AY, even though it has a different clock, due to
// the different input stages. That's not yet proven.)
// All volume is set to 8 bit values. This lets us take in a wider
// range of input datas, and gives us more flexibility on the output.

// Currently data is stored in nCurrentTone[] with even numbers being
// tones and odd numbers being volumes, and the fourth tone is assumed
// to be the noise channel.

// Tones: scales to SN countdown rates, even on noise, 0 to 0xfff (4095)
// Volume: 8-bit linear (NOT logarithmic) with 0 silent and 0xff loudest
// Note that /either/ value can be -1 (integer), which means never set (these won't land in the stream though)

#include <stdio.h>
#include <stdarg.h>
#define MINIZ_HEADER_FILE_ONLY
#include "tinfl.c"						    // comes in as a header with the define above
										    // here to allow us to read vgz files as vgm

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary

// 8 huge channels
int VGMStream[8][MAXTICKS];		            // every frame the data is output
unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
int nCurrentTone[8];						// current value for each channel in case it's not updated
double freqClockScale = 1.0;			    // doesn't affect noise channel, even though it would in reality. Not much we can do!
int nTicks;                                 // this MUST be a 32-bit int
bool verbose = false;
int nDesiredSong = -1;

// codes for noise processing (if not periodic, it's white noise)
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000
#define NOISE_MAPCHAN3 0x40000

// ay specific stuff
bool useAyMixer = false;
int ayMixer = 0;                        // enable low

// options
bool scaleFreqClock = false;			// apply scaling for unexpected clock rates
bool halfvolrange = false;				// use only 8 levels of volume instead of 16 for better compression
bool code30hz = false;					// code for 30hz instead of 60 hz
unsigned int nRate = 60;

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
// mapVolume assumes the order of this table for mute suppression
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
};

#define ABS(x) ((x)<0?-(x):x)

// crc function for checking the gunzip
unsigned long crc(unsigned char *buf, int len);

// input: 8 bit unsigned audio (centers on 128)
// output: 15 (silent) to 0 (max)
int mapVolume(int nTmp) {
	int nBest = -1;
	int nDistance = INT_MAX;
	int idx;

	// testing says this is marginally better than just (nTmp>>4)
	for (idx=0; idx < 16; idx++) {
		if (ABS(volumeTable[idx]-nTmp) < nDistance) {
			nBest = idx;
			nDistance = abs(volumeTable[idx]-nTmp);
		}
	}

    // don't return mute unless the input was mute
    if ((nBest == 15) && (nTmp != 0)) --nBest;

	// return the index of the best match
	return nBest;
}

// maps the input noise frequency and flags to the PSG noise type
int mapNoisePSG(int nTmp) {
    static const int noiseTable[3] = { 16, 32, 64 };
	int nBest = -1;
	int nDistance = INT_MAX;
	int idx;
	int nLast = 0x0f;

    if (nTmp & NOISE_MAPCHAN3) {
        nBest = 3;
    } else {
	    for (idx=0; idx < 3; idx++) {
		    if (ABS(noiseTable[idx]-(nTmp&NOISE_MASK)) < nDistance) {
			    nBest = idx;
			    nDistance = abs(volumeTable[idx]-(nTmp&NOISE_MASK));
		    }
	    }
    }
    if (0 == (nTmp & NOISE_PERIODIC)) nBest += 4;   // white noise 4-7

	// return the formatted code
	return nBest;
}

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
	printf("\rDecompressed to %d bytes\n", outlen);
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
    int nWork[8];
    for (int idx=0; idx<8; ++idx) {
        nWork[idx] = nCurrentTone[idx];
    }

    // now we can manipulate these, if needed for special cases

    // if the noise channel needs to use channel 3 for frequency...
    if (nWork[6]&NOISE_MAPCHAN3) {
        nWork[6]=(nWork[6]&(~NOISE_MASK)) | (nWork[4]&NOISE_MASK);   // gives a frequency for outputs that need it
    }
    // if the AY mapping is active, apply that
    if (useAyMixer) {
        // noise control - we'll take the loudest available
        int vol = 0;
        if (!(ayMixer&0x20)) {
            // mix with vol C
            if (nWork[5] > vol) vol = nWork[5];
        }
        if (!(ayMixer&0x10)) {
            // mix with vol B
            if (nWork[3] > vol) vol = nWork[3];
        }
        if (!(ayMixer&0x08)) {
            // mix with vol A
            if (nWork[1] > vol) vol = nWork[1];
        }
        nWork[7] = vol;

        // tone control, mute if not enabled (have to do this AFTER the noise above)
        if (ayMixer&0x04) {
            // mute tone C
            nWork[5]=0;
        }
        if (ayMixer&0x02) {
            // mute tone B
            nWork[3]=0;
        }
        if (ayMixer&0x01) {
            // mute tone A
            nWork[1]=0;
        }
    }

    // number of rows out is 1 or 2
    int rowsOut = 1;
    // This causes padding to adapt 50hz to 60hz playback
    // it's the same output code as above just repeated
	if ((nRate==50)&&(nTicks%6==0)) {
		// output an extra frame delay
        rowsOut++;
    }

    // now write the result into the current line of data
    for (int rows = 0; rows < rowsOut; ++rows) {
	    for (int idx=0; idx<8; idx++) {
		    if (nWork[idx] == -1) {
                switch(idx) {
                    case 0:
                    case 2:
                    case 4: // tones
                    case 6: // noise
                        nWork[idx] = 1;     // very high pitched
                        break;

                    case 1:
                    case 3:
                    case 5:
                    case 7: // volumes
                        nWork[idx] = 0;     // mute
                        break;
                }
            }
		    VGMStream[idx][nTicks]=nWork[idx];
	    }
    	nTicks++;
	    if (nTicks > MAXTICKS) {
		    printf("\rtoo many ticks (%d), can not process. Need a shorter song.\n", MAXTICKS);
		    return false;
	    }
    }

    return true;
}

int main(int argc, char* argv[])
{
	printf("v12092019\n");

	if (argc < 2) {
		printf("vgm2psg [-v] [-scalefreq] [-halfvolrange] [-30hz] <filename>\n");
		printf(" -v - output verbose data\n");
		printf(" -scalefreq - apply frequency scaling if the frequency clock is not the NTSC clock\n");
		printf(" -halfvolrange - use 8 levels of volume instead of all 16 (lossy compression, usually sounds ok)\n");
		printf(" -30hz - code for 30hz instead of 60hz (may affect timing, may break arpeggio)\n");
		printf(" -testout - output song as a VGM for testing playback.\n");
		printf(" <filename> - VGM file to compress. Output will be <filename>.spf\n");
		return -1;
	}

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-v")) {
			verbose=true;
		} else if (0 == strcmp(argv[arg], "-scalefreq")) {
			scaleFreqClock=true;
		} else if (0 == strcmp(argv[arg], "-halfvolrange")) {
			halfvolrange=true;
		} else if (0 == strcmp(argv[arg], "-30hz")) {
			code30hz = true;
		} else if (0 == strcmp(argv[arg], "-testout")) {
        	nDesiredSong = 0;
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
		printf("\rReading %s - ", argv[arg]);
		buffersize=fread(buffer, 1, sizeof(buffer), fp);
		fclose(fp);
		printf("%d bytes\n", buffersize);

		// -Split a VGM file into multiple channels (8 total - 4 audio, 3 tone and 1 noise)
		//  For the first pass, emit for every frame (that should be easier to parse down later)

		// The format starts with a 192 byte header:
        // TODO: header is up to 256 bytes in the current version... (1.71?)
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
		// 0xB0 [Pokey clock    ][QSound clock   ] *** *** *** ***  *** *** *** ***
	
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
            // TODO: bit indicates dual PSG chips - eventually I want this
            printf("Warning: ignoring dual PSG output.\n");
        }
		nClock&=0x0FFFFFFF;
		double nNoiseRatio = 1.0;
        if (nClock != 0) {  // 0 might be an AY only file...
		    if ((nClock < 3579545-10) || (nClock > 3579545+10)) {
			    freqClockScale = 3579545.0/nClock;
			    printf("\rUnusual clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }
        }
		if (nVersion > 0x100) {
			nRate = *((unsigned int*)&buffer[0x24]);
			if ((nRate!=50)&&(nRate!=60)) {
				printf("\rweird refresh rate %d\n", nRate);
				return -1;
			}
		}
		myprintf("Refresh rate %d Hz\n", nRate);
		unsigned int nShiftRegister=16;
		if (nVersion >= 0x110) {
			nShiftRegister = buffer[0x2a];
			if ((nShiftRegister!=16)&&(nShiftRegister!=15)) {
				printf("\rweird shift register %d\n", nShiftRegister);
				return -1;
			}
		}
		// TI/Coleco has a 15 bit shift register, so if it's 16 (default), scale it down
		if (nShiftRegister == 16) {
			nNoiseRatio *= 15.0/16.0;
			myprintf("Selecting 16-bit shift register.\n");
		}

		unsigned int nOffset=0x40;
		if (nVersion >= 0x150) {
			nOffset=*((unsigned int*)&buffer[0x34])+0x34;
			if (nOffset==0x34) nOffset=0x40;		// if it was 0
		}

		for (int idx=0; idx<8; idx+=2) {
			nCurrentTone[idx]=-1;		// never set
			nCurrentTone[idx+1]=-1;		// never set
		}
		nTicks= 0;
		int nCmd=0;
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
				{
                    // TODO: second chip is on 0x30
					// here's the good stuff!
					unsigned char c = buffer[nOffset+1];
					if (c&0x80) {
						// it's a command byte - update the byte data
						nCmd=(c&0x70)>>4;
						c&=0x0f;

						// save off the data into the appropriate stream
						switch (nCmd) {
						case 1:		// vol0
						case 3:		// vol1
						case 5:		// vol2
						case 7:		// vol3
							// single byte (nibble) values
                            if (halfvolrange) {
    							nCurrentTone[nCmd]=volumeTable[c|1];    // |1 ensures mute is always available
                            } else {
    							nCurrentTone[nCmd]=volumeTable[c];
                            }
							break;

						case 6:		// noise
							// single byte (nibble) values, masked to indicate a retrigger
                            // put in the actual countdown clock
							nCurrentTone[nCmd]=NOISE_TRIGGER;
                            if ((c&0x4)==0) nCurrentTone[nCmd]|=NOISE_PERIODIC;
                            switch (c&0x3) {
                                case 0: // 6991Hz
                                    nCurrentTone[nCmd]|=(int)(111860.9/6991+.5);   // 16
                                    break;
                                case 1: // 3496Hz
                                    nCurrentTone[nCmd]|=(int)(111860.9/3496+.5);   // 32
                                    break;
                                case 2: // 1748Hz
                                    nCurrentTone[nCmd]|=(int)(111860.9/1748+.5);   // 64
                                    break;
                                case 3: // map to channel 3
                                    nCurrentTone[nCmd]|=NOISE_MAPCHAN3;
                                    break;
                            }

							break;

						default:	
							// tone command, least significant nibble
                            if (nCurrentTone[nCmd] == -1) nCurrentTone[nCmd]=0;
							nCurrentTone[nCmd]&=0xff0;
							nCurrentTone[nCmd]|=c;
							break;
						}
					} else {
						// non-command byte, use the previous command and update
                        if (nCurrentTone[nCmd] == -1) nCurrentTone[nCmd]=0;
						nCurrentTone[nCmd]&=0x00f;
						nCurrentTone[nCmd]|=(c&0xff)<<4;
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

            // we do want support for the AY-3-8910 / YM2149 - clock is at 0x74 in the header, 0x78 specifies the variant (we want 0x00 or 0x10)
            // we assume a single file has AY /or/ SN audio, not both. :)
            case 0xa0:
                {
                    static bool warn = false;
                    if (!warn) {
                        // check the type
                        warn = true;
                        if (nVersion < 0x151) printf("Warning: AY commands in file older than 1.51 not supported.\n");
                        if ((buffer[0x78]!=0x00)&&(buffer[0x78]!=0x10)) printf("AY type 0x%02X may not be parsed correctly.\n", buffer[0x78]);
                        if ((buffer[0x79]!=0)||(buffer[0x7a]!=0)||(buffer[0x7b]!=0)) printf("Non-zero AY flags ignored\n");
                        int clk = buffer[0x74]+buffer[0x75]*256+buffer[0x76]*65536+buffer[0x77]*16777216;
                        if (clk&0x40000000) {
                            // TODO: bit indicates dual AY chips - eventually I want this
                            printf("Warning: ignoring dual AY output.\n");
                        }
                        clk &= 0x0fffffff;
                        if (clk != 1789750) {
			                freqClockScale = 1789750.0/clk;
                            printf("\rUnusual AY clock of %dHz Scale factor %f.\n", clk, freqClockScale);
                        }
                    }
                    // 0xa0 aa dd - write value dd to register aa
                    // TODO: we need some envelope emulation to do this right...
                    // not 100% sure how the envelope works yet, despite my docs
                    unsigned char r = buffer[nOffset+1];    // TODO: bit 0x80 means go to second chip
                    unsigned char c = buffer[nOffset+2];
                    switch (r&0x9f) {
                        case 0: 
                        case 2:
                        case 4: // fine tune (lower 8 bits)
                        {
                            int idx = (r&0xf)>>1;
                            if (nCurrentTone[idx]==-1) nCurrentTone[idx]=0;
                            nCurrentTone[idx]&=0xf00;
                            nCurrentTone[idx]|=c;
                            break;
                        }

                        case 1:
                        case 3:
                        case 5: // coarse tune (upper 4 bits)
                        {
                            int idx = ((r&0xf)-1)>>1;
                            if (nCurrentTone[idx]==-1) nCurrentTone[idx]=0;
                            nCurrentTone[idx]&=0x0ff;
                            nCurrentTone[idx]|=(c&0xf)<<8;
                            break;
                        }

                        case 6: // noise frequency (5 bits)
                            // extra bit to indicate a retrigger
                            useAyMixer = true;  // just in case the user never sets the mixer itself, we need it for noise
                            nCurrentTone[6] = (c&0x1f) | NOISE_TRIGGER;
                            break; 

                        case 7: // mixer control
                        {
                            // there are 3 bits for noise control, and 3 for tones
                            // if more than one noise bit is set, we're just going to
                            // throw a warning for now.
                            static bool warn = false;
                            if (((c&0x38)!=0x20)&&((c&0x38)!=0x10)&&((c&0x38)!=0x08)) {
                                if (!warn) printf("Warning: AY noise mapped to multiple channels.\n");
                            }
                            useAyMixer = true;
                            ayMixer = c;
                            // we need to apply this during decode, since it shares the volume levels,
                            // and/or might be muted without changing the volume register
                            break;
                        }

                        case 8:
                        case 9:
                            // unused
                            break;

                        case 10:
                        case 11:
                        case 12:    // volumes
                            if (halfvolrange) {
                                nCurrentTone[((r&0x1f)-10)*2+1] = volumeTable[(c&0xf)|1];   // |1 ensures mute is always available
                            } else {
                                nCurrentTone[((r&0x1f)-10)*2+1] = volumeTable[c&0xf];
                            }
                            // todo: the envelope use bit is 0x10
                            break;

                        case 13:    // TODO envelope period fine (8 bits)
                        case 14:    // TODO envelope period coarse (8 bits)
                        case 15:    // TODO envelope shape control (08-continue, 04-attack, 02-alternate, 01-hold)
                            break;

                        default:    // TODO: ignoring second chip support
                            break;
                    }

                    nOffset+=3;
				}
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

		if (nShiftRegister != 15) {
			int cnt=0;
			myprintf("Adapting channel 3 for user-defined shift rates...");
			// go through every frame -- if the noise channel is user defined (PSG only),
			// adapt the tone by nRatio
			for (int idx=0; idx<nTicks; idx++) {
				if (VGMStream[6][idx]&NOISE_MAPCHAN3) {
					if (VGMStream[5][idx] != 0) {
						// we can hear it! Don't tune it! But warn the user
						static int warned = false;
						if (!warned) {
							if (VGMStream[7][idx] != 0) {
								warned=true;
								printf("\nSong employs user-tuned noise channel, but both noise and voice are audible.\n");
								printf("Not tuning the audible voice, but this may cause noise to be detuned. Use\n");
								printf("a 15-bit noise channel if your tracker supports it!\n");
							}
						}
					} else {
						VGMStream[4][idx]=(int)((VGMStream[4][idx]/nNoiseRatio)+0.5);		// this is verified on hardware!
						cnt++;
					}
				}
			}
			myprintf("%d notes tuned.\nChannel 3 will be preserved even if silent.\n", cnt);
		} else {
			// we don't need to tune, but we DO need to check whether channel 3 might be undead.
			myprintf("Checking if channel 3 is tuning noises...");
			// go through every frame -- if the noise channel is user defined, then we are set
			for (int idx=0; idx<nTicks; idx++) {
				if (VGMStream[6][idx]&NOISE_MAPCHAN3) {
					myprintf("Noise control detected, channel 3 will be preserved even if silent.\n");
					break;
				}
			}
		}

		if (scaleFreqClock) {
			int clip = 0;
			// debug is emitted earlier - a programmable sound chip rate, like the F18A gives,
			// could playback at the correct frequencies, otherwise we'll just live with the
			// detune. The only one I have is 680 Rock (which /I/ made) and it's 2:1
			if (freqClockScale != 1.0) {
				myprintf("Adapting tones for unusual clock rate... ");
				// scale every tone - clip if needed (note further clipping for PSG may happen at output)
				for (int idx=0; idx<nTicks; idx++) {
					VGMStream[0][idx] = (int)(VGMStream[0][idx] * freqClockScale + 0.5);
                    if (VGMStream[0][idx] == 0)    { VGMStream[0][idx]=1;     clip++; }
					if (VGMStream[0][idx] > 0xfff) { VGMStream[0][idx]=0xfff; clip++; }

					VGMStream[2][idx] = (int)(VGMStream[2][idx] * freqClockScale + 0.5);
                    if (VGMStream[2][idx] == 0)    { VGMStream[0][idx]=1;     clip++; }
					if (VGMStream[2][idx] > 0xfff) { VGMStream[2][idx]=0xfff; clip++; }

					VGMStream[4][idx] = (int)(VGMStream[4][idx] * freqClockScale + 0.5);
                    if (VGMStream[4][idx] == 0)    { VGMStream[0][idx]=1;     clip++; }
					if (VGMStream[4][idx] > 0xfff) { VGMStream[4][idx]=0xfff; clip++; }
				}
				if (clip > 0) myprintf("%d tones clipped");
				myprintf("\n");
			}
		}

		if (code30hz) {
			// cut the number of entries in half - the savings are obvious, but the output of
			// the song can be damaged, too. (Even with compression, at least the time streams
			// are halved). Arpeggios would be completely destroyed (we try). But this may be useful
			// for being able to split processing between music and sound effects on separate ticks
			printf("\rReducing to 30hz playback (lossy)....\n");
			for (int chan=0; chan<8; chan++) {
				// noise trigger MUST be preserved, so check for that
				for (int idx=0; idx<nTicks; idx+=2) {
					// to try and improve arpeggios, check the two words and take the first
					// one that's different from current. But always take retrigger flags
					// This works acceptably well.
					if ((idx==0)||(VGMStream[chan][idx/2-1]!=VGMStream[chan][idx])) {
						VGMStream[chan][idx/2]=VGMStream[chan][idx] | (VGMStream[chan][idx+1] & 0x0000f000);
					} else {
						VGMStream[chan][idx/2]=VGMStream[chan][idx+1] | (VGMStream[chan][idx] & 0x0000f000);
					}
				}
			}
			nTicks/=2;
		}
	}

    if (nDesiredSong > -1) {
		int nDesiredSong = 0;		// desired song to test as PSG VGM only
		unsigned char out[]={
			0x56,0x67,0x6D,0x20,0x0B,0x1F,0x00,0x00,0x10,0x01,0x00,0x00,0x94,0x9E,0x36,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x91,0xD4,0x12,0x00,0x40,0x02,0x00,0x00,
			0x00,0x3A,0x11,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
		};
		*((unsigned int*)&out[4])=nTicks*11-4;		// set length (not actually correct!)
		*((unsigned int*)&out[0x18])=nTicks;		// set number of waits (this should be right!)
        printf("Writing test.output.vgm\n");
		FILE *fp=fopen("test.output.vgm", "wb");
		fwrite(out, 64, 1, fp);
		for (int idx=0; idx<nTicks; idx++) {
			for (int i2=nDesiredSong*8; i2<(nDesiredSong*8)+8; i2++) {
				// skip unchanged notes - mask is to detect noise triggers without outputting noises twice
				if ((idx>0)&&(VGMStream[i2][idx] == VGMStream[i2][idx-1])) continue;

				fputc(0x50,fp);

				switch (i2) {
				case 1:
				case 3:
				case 5:
				case 7:
                    // volumes need to be remapped back
					fputc(mapVolume(VGMStream[i2][idx]) | (i2<<4) | 0x80, fp);
					break;
                
                case 6:
					// single nibble
                    // noises need to be mapped too
					fputc(mapNoisePSG(VGMStream[i2][idx]) | (i2<<4) | 0x80, fp);
					break;

				default:
					// dual byte
					fputc((VGMStream[i2][idx]&0xf) | (i2<<4) | 0x80, fp);
					fputc(0x50, fp);
					fputc((VGMStream[i2][idx] & 0x3f0)>>4, fp); // 0x3f deliberate here, VGM test export
					break;
				}
			}
			// all channels out, delay 1/60th second
			fputc(0x62, fp);
			if (code30hz) fputc(0x62,fp);	// second delay for 30hz
		}
		// output end of data
		fputc(0x66, fp);
		fclose(fp);
	}

    // data is entirely stored in VGMStream[ch][tick]
    // [ch] the channel, and is tone/vol/tone/vol/tone/vol/noise/vol
    // we just write out 8 channels as <name>_tonX.60hz and <name>_volX.60hz
    // the noise channel is named <name>_noiX.60hz
    // simple ASCII format, values stored as hex (but import should support decimal!), row number is implied frame
    {
        for (int ch=0; ch<8; ch++) {
            char strout[1024];
            char num[32];

            // create a filename
            strcpy(strout, argv[arg]);
            if (ch == 6) {
                strcat(strout, "_noi");         // noise (same format as tone, except noise flags are valid)
            } else {
                if (ch&1) {
                    strcat(strout, "_vol");     // volume (0-255 linear, 0=mute)
                } else {
                    strcat(strout, "_ton");     // tone (0-4095)
                }
            }
            sprintf(num, "%d", ch/2);
            strcat(strout, num);
            strcat(strout, ".60hz");

            // open the file
		    FILE *fp=fopen(strout, "wb");
		    if (NULL == fp) {
			    printf("failed to open file '%s'\n", strout);
			    return -1;
		    }
		    printf("Writing %s...\n", strout);

            for (int r=0; r<nTicks; ++r) {
                fprintf(fp, "0x%08X\n", VGMStream[ch][r]);
            }

            fclose(fp);
        }
    }

    printf("done vgm2psg.\n");
    return 0;
}

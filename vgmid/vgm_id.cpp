// vgm_id.cpp : Defines the entry point for the console application.
// This reads in a VGM file, and lists the recognized sound chips and
// other basic data.

// TODO: gameboy can have notes too low AND noises with too fast a shift rate,
// we should warn on both. (Actually, not sure if gb2psg warns on the too fast...)

#include <stdio.h>
#include <stdarg.h>
#define MINIZ_HEADER_FILE_ONLY
#include "tinfl.c"						    // comes in as a header with the define above
										    // here to allow us to read vgz files as vgm

unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
bool verbose = false;

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

int main(int argc, char* argv[])
{
	printf("VGM_ID - v20201001\n\n");

	if (argc < 2) {
		printf("vgm_id <filename>\n");
		printf(" <filename> - VGM file to read.\n");
		return -1;
	}
    verbose = true;

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-q")) {
			verbose=false;
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
			printf("Warning: EOF in header past end of file! Truncating.\n");
			nEOF = buffersize;
		}

		unsigned int nVersion = *((unsigned int*)&buffer[8]);
		myprintf("\nReading version 0x%X\n", nVersion);

        int nRate = 60;
        int samplesPerTick = 735;
		if (nVersion > 0x100) {
			nRate = *((unsigned int*)&buffer[0x24]);
            if (nRate == 0) {
                printf("Warning: rate set to zero - treating as 60hz\n");
                nRate = 60;     // spec doesn't define a default...
            }
			if ((nRate!=50)&&(nRate!=60)) {
				printf("Weird refresh rate %d - only 50hz or 60hz supported\n", nRate);
			}
		}
        if (nRate != 60) {
            samplesPerTick = int((double)samplesPerTick * ((double)60/nRate));
        }
		myprintf("Refresh rate %d Hz (%d samples per tick)\n", nRate, samplesPerTick);

        // detect PSG
		unsigned int nClock = *((unsigned int*)&buffer[12]);
        if (nClock != 0) {
            printf("\n* Detected PSG with clock of %dHz\n", nClock&0x0fffffff);
            if (nClock&0x40000000) {
                // bit indicates dual PSG chips - though I don't need to do anything here
                // the format assumes that all PSGs are identical
                printf("  Dual PSG output specified.\n");
            }
    		nClock&=0x0FFFFFFF;
		    if ((nClock < 3579545-10) || (nClock > 3579545+10)) {
			    double freqClockScale = 3579545.0/nClock;
			    printf("  Unusual PSG clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		    }

            unsigned int nShiftRegister=16;     // default if not specified
		    if (nVersion >= 0x110) {
			    nShiftRegister = buffer[0x2a];
			    if ((nShiftRegister != 0)&&(nShiftRegister!=16)&&(nShiftRegister!=15)) {
				    printf("  Weird shift register %d - only 15 or 16 bit supported\n", nShiftRegister);
				    return -1;
			    }
		    }
            printf("  Shift register size: %d\n", nShiftRegister);
        }

        // detect AY
        if (nVersion >= 0x151) {
		    nClock = *((unsigned int*)&buffer[0x74]);
            if (nClock != 0) {
                printf("\n* Detected AY with clock of %dHz\n", nClock&0x0fffffff);

                if (nClock&0x40000000) {
                    // bit indicates dual chips - though I don't need to do anything here
                    printf("  Dual AY output specified.\n");
                }
		        nClock&=0x0FFFFFFF;
                if (nClock != 0) {
                    // SMS Power says usually 1789750, but that is an odd value
                    // actually saw this one in the wild on Time Pilot, and seems
                    // to be right for MSX too (will test more)
		            if ((nClock < 1789772-10) || (nClock > 1789772+10)) {
			            double freqClockScale = 1789772.0/nClock;
			            printf("  Unusual AY clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		            }
                    // emit some debug of type
                    switch (buffer[0x78]) {
                        case 0:   printf ("  Subtype AY8910...\n"); break;
                        case 1:   printf ("  Subtype AY8912 (untested)...\n"); break;
                        case 2:   printf ("  Subtype AY8913 (untested)...\n"); break;
                        case 3:   printf ("  Subtype AY8930 (untested)...\n"); break;
                        case 0x10: printf("  Subtype YM2149 (untested)...\n"); break;
                        case 0x11: printf("  Subtype YM3439 (untested)...\n"); break;
                        case 0x12: printf("  Subtype YMZ284 (untested)...\n"); break;
                        case 0x13: printf("  Subtype YMZ294 (untested)...\n"); break;
                        default:   printf("  Subtype unknown AY variant (untested)\n");
                    }
                }
            }
        }

        // detect Gameboy DMG
        if (nVersion >= 0x161) {
		    nClock = *((unsigned int*)&buffer[0x80]);
            if (nClock != 0) {
                printf("\n* Detected DMG (Gameboy) with clock of %dHz\n", nClock&0x0fffffff);

                if (nClock&0x40000000) {
                    // bit indicates dual chips - though I don't need to do anything here
                    printf("  Dual DMG output specified.\n");
                }
		        nClock&=0x0FFFFFFF;
                if (nClock != 0) {
		            if ((nClock < 4194304-10) || (nClock > 4194304+10)) {
			            double freqClockScale = 4194304.0/nClock;
			            printf("  Unusual DMG clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		            }
                }
            }
        }

        // detect NES APU
        if (nVersion >= 0x161) {
		    nClock = *((unsigned int*)&buffer[0x84]);
            if (nClock != 0) {
                printf("\n* Detected APU (NES) with clock of %dHz\n", nClock&0x0fffffff);

                if (nClock&0x40000000) {
                    // bit indicates dual chips - though I don't need to do anything here
                    printf("  Dual APU output specified.\n");
                }
		        nClock&=0x0FFFFFFF;
                if (nClock != 0) {
		            if ((nClock < 1789772-10) || (nClock > 1789772+10)) {
			            double freqClockScale = 1789772.0/nClock;
			            printf("  Unusual APU clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		            }
                }
            }
        }

        // detect Pokey
        if (nVersion >= 0x161) {
		    nClock = *((unsigned int*)&buffer[0xb0]);
            if (nClock != 0) {
                printf("\n* Detected POKEY with clock of %dHz\n", nClock&0x0fffffff);

                if (nClock&0x40000000) {
                    // bit indicates dual chips - though I don't need to do anything here
                    printf("  Dual POKEY output specified.\n");
                }
		        nClock&=0x0FFFFFFF;
                if (nClock != 0) {
		            if ((nClock < 1789772-10) || (nClock > 1789772+10)) {
			            double freqClockScale = 1789772.0/nClock;
			            printf("  Unusual POKEY clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		            }
                }
            }
        }

        // detect YM2612 (Genesis/MegaDrive)
        if (nVersion >= 0x110) {
            // Note that version 1.01 files can have YM2612 data with the clock
            // stored on the YM2413 slot at 0x10. But I think we will not support
            // those older files - if anyone has one that can't be found elsewhere,
            // we can easily convert it by hand.
		    nClock = *((unsigned int*)&buffer[0x2c]);
            if (nClock != 0) {
                printf("\n* Detected YM2612 (MD) with clock of %dHz\n", nClock&0x0fffffff);

                if (nClock&0x40000000) {
                    // bit indicates dual chips - though I don't need to do anything here
                    // I'm not sure I'm going to support this, though... unless it pops up.
                    printf("  Dual YM2612 output specified.\n");
                }
		        nClock&=0x0FFFFFFF;
                if (nClock != 0) {
		            if ((nClock < 7670454-10) || (nClock > 7670454+10)) {
			            double freqClockScale = 7670454.0/nClock;
			            printf("  Unusual YM2612 clock rate %dHz. Scale factor %f.\n", nClock, freqClockScale);
		            }
                }
            }
        }

        printf("\n");

        // look for anything in the data that might throw us off...
        // find the start of data
		unsigned int nOffset=0x40;
		if (nVersion >= 0x150) {
			nOffset=*((unsigned int*)&buffer[0x34])+0x34;
			if (nOffset==0x34) nOffset=0x40;		// if it was 0
		}

		while (nOffset < nEOF) {
			// parse data for a tick
			switch (buffer[nOffset]) {		// what is it? what is it?
			case 0x50:		// PSG data byte
            case 0x30:      // second PSG data byte
                {
                    int val = buffer[nOffset+1];
                    int chip = (buffer[nOffset]==0x30)?1:0;

                    // check for low noises - AY can't do them
                    {
                        static bool warned = false;
                        static int lastidx = -1;

                        if (!warned) {
                            // just track channel 3 and 4
                            static int tone[2][2] = {0,0, 0,0};
                            static int writes = 0;  // writes to channel 3 only
                            if ((val&0x80)==0) {
                                // not a command, update last reg
                                if (lastidx >= 0) {
                                    if (lastidx==3) {
                                        tone[chip][0] = (tone[chip][0]&0xf)|(val<<4);
                                        ++writes;
                                    } else if (lastidx==4) {
                                        tone[chip][1] = val&0x0f;
                                    }
                                }
                            } else {
                                if ((val&0x10)==0) {
                                    // tone command
                                    lastidx = ((val&0x60)>>5);
                                    if ((lastidx==3)||(lastidx==4)) {
                                        tone[chip][lastidx-3] = val&0x0f;
                                    }
                                    if (lastidx == 3) {
                                        ++writes;
                                    }
                                }
                            }
                            if (writes >= 2) {
                                writes = 0;
                                if ((tone[chip][1]&3)==3) {
                                    if (tone[chip][0] > 0x1f) {
                                        printf("Warning: user-defined noise rate too low for accurate AY replay (%03X)\n", tone[chip][1]);
                                        warned = true;
                                    }
                                }
                            }
                        }
                    }

                    // check for lowest fixed shift noise
                    {
                        static bool warned = false;
                        if ((!warned)&&((val&0xf3)==0xe2)) {
                            printf("Warning: low frequency fixed noise too low for accurate AY replay (%02X)\n", val);
                            warned = true;
                        }
                    }

                    // check for periodic noise
                    {
                        static bool warned = false;
                        if ((!warned)&&((val&0xf4)==0xe0)) {
                            printf("Warning: periodic noise not available for accurate AY replay (%02X)\n", val);
                            warned = true;
                        }
                    }
                }

				nOffset+=2;
				break;

			case 0x61:		// 16-bit wait value
				{
                    static bool delaywarn = false;
					unsigned int nTmp=buffer[nOffset+1] | (buffer[nOffset+2]<<8);
					// divide down from samples to ticks (either 735 for 60hz or 882 for 50hz)
					if (nTmp % samplesPerTick) {
						if (!delaywarn) {
							printf("Warning: Uses non-frame sized wait values, timing may be off.\n");
							delaywarn=true;
						}
					}
					nOffset+=3;
				}
				break;

			case 0x62:		// wait 735 samples (60th second)
			case 0x63:		// wait 882 samples (50th second)
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
                {
                    static bool warn = false;
                    if (!warn) {
    				    warn = true;
                        printf("\rWarning: Using fine timing which can't be tracked.\n");
                    }
				}
				nOffset++;
				break;

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
                // YM2612
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

            case 0xa0:  // AY8910
				{
                    int reg = buffer[nOffset+1];
                    int val = buffer[nOffset+2];
                    int chip = (reg&0x80)?1:0;
                    reg &= 0x7f;

                    // check for fast envelopes
                    {
                        static bool warned = false;
                        if (!warned) {
                            static int env[2] = {0,0};
                            if (reg == 11) {
                                // envelope 8-bit fine tune
                                env[chip] = (env[chip]&0xf00) | val;
                            }
                            if (reg == 12) {
                                // envelope 4 bit coarse tune
                                env[chip] = (env[chip]&0xff) | ((val&0xf)<<8);
                            }
                            // only need to check when the envelope is triggered
                            if (reg == 13) {
                                if (env[chip] < 1000) {
                                    // this could be a false warning during transitions...
                                    printf("Warning: AY Envelope may be too fast for accurate PSG replay (%03X)\n", env[chip]);
                                    warned = true;
                                }
                            }
                        }
                    }

                    // check for low pitches - similar to the envelope check above
                    {
                        static bool warned = false;
                        if (!warned) {
                            static int tone[2][3] = {0,0,0, 0,0,0};
                            static int writes = 0;
                            if ((reg==0)||(reg==2)||(reg==4)) {
                                tone[chip][reg/2]=(tone[chip][reg/2]&0xf00)|val;
                            } else if ((reg==1)||(reg==3)||(reg==5)) {
                                tone[chip][reg/2]=(tone[chip][reg/2]&0x0ff)|((val&0xf)<<8);
                            }
                            // only check every other write to give the code a chance
                            // to write both registers
                            if (reg < 6) {
                                ++writes;
                                if (((writes&1) == 0)&&((tone[chip][0] > 0x3ff)||(tone[chip][1] > 0x3ff)||(tone[chip][2] > 0x3ff))) {
                                    // this could be a false warning during transitions...
                                    printf("Warning: AY frequency may be too low for accurate PSG replay ");
                                    printf("(%03X %03X %03X)\n", tone[chip][0], tone[chip][1], tone[chip][2]);
                                    warned = true;
                                }
                            }
                        }
                    }

					nOffset+=3;
				}
				break;

            case 0xa2:
            case 0xa3:  // second YM2612
                nOffset+=3;
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

            case 0xb3:  // gameboy DMG
				{
					nOffset+=3;
				}
				break;

            case 0xb4:  // NES APU
				{
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

            case 0xbb:  // POKEY
				{
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
                printf("Will not be able to process file.\n");
				return -1;
			}
		}

        printf("\n** DONE **\n");
    }

    return 0;
}

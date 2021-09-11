// soundlist2psg.cpp : Defines the entry point for the console application.
// This reads in a binary file containing TI SoundList data only

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
#include <string.h>
#include <stdlib.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary

// in general even channels are tone (or noise) and odd are volume
// They are mapped to PSG frequency counts and 8-bit volume (0=mute, 0xff=max)
// each chip uses up 8 channels
#define MAXCHANNELS 8
int VGMStream[MAXCHANNELS][MAXTICKS];		// every frame the data is output
#define FIRSTPSG 0

unsigned char buffer[1024*1024];			// 1MB read buffer
unsigned int buffersize;					// number of bytes in the buffer
int nCurrentTone[MAXCHANNELS];				// current value for each channel in case it's not updated
int nTicks;                                 // this MUST be a 32-bit int
bool verbose = false;                       // emit more information
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count

// codes for noise processing (if not periodic (types 0-3), it's white noise (types 4-7))
#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000
#define NOISE_MAPCHAN3 0x40000

// options
unsigned int nRate = 60;

// lookup table to map PSG volume to linear 8-bit. AY is assumed close enough.
unsigned char volumeTable[16] = {
	254,202,160,128,100,80,64,
	50,
	40,32,24,20,16,12,10,0
};

#define ABS(x) ((x)<0?-(x):x)

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
	printf("Import TI PlayList - v20210911\n");

	if (argc < 2) {
		printf("playlist2psg [-50] [-q] [-d] [-o <n>] [-add <n>] <filename>\n");
		printf(" -50 - input data is 50hz instead of 60hz\n");
		printf(" -q - quieter verbose data\n");
        printf(" -o <n> - output only channel <n> (1-5)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
		printf(" <filename> - VGM file to read.\n");
		return -1;
	}
    verbose = true;

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-q")) {
			verbose=false;
        } else if (0 == strcmp(argv[arg], "-50")) {
			nRate = 50;
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

		// split a TI playlist into multiple channels
		// <num bytes><byte data...><num frames>
		// ends when num frames is 0

		// since this is TI specific, we can assume SN chip with 15-bit noise register
		// and standard clocking. 

		// Since a binary file can be anything and a sound list can't, we will validate as we go

        // find the start of data
		unsigned int nOffset=0;

		// find the end of data
		unsigned int nEOF = buffersize;

        // prepare the decode
		for (int idx=0; idx<MAXCHANNELS; idx+=2) {
			nCurrentTone[idx]=-1;		// never set
			nCurrentTone[idx+1]=-1;		// never set
		}
		nTicks= 0;
		int nCmd = 0;
        int lastCmd = 0;
		double nNoiseRatio = 1.0;

		// Use nRate - if it's 50, add one tick delay every 5 frames
		// If user-defined noise is used, multiply voice 3 frequency by nNoiseRatio
		// Use nOffset for the pointer
		// Stop parsing at nEOF
		while (nOffset < nEOF) {
			// parse data for a tick
			int cnt = buffer[nOffset++];
			if (cnt > 31) {
				printf("Fatal: unreasonable count value of %d at offset %d\n", cnt, nOffset-1);
				return -1;
			}
			if (cnt > 11) {
				printf("Warning: unlikely count value of %d at offset %d\n", cnt, nOffset-1);
			}
			for (int idx=0; idx<cnt; ++idx) {
				// unfortunately, the actual bytes /can/ be anything
				unsigned char c = buffer[nOffset++];

				if (c&0x80) {
					// it's a command byte - update the byte data
					nCmd=(c&0x70)>>4;
					c&=0x0f;

                    lastCmd = nCmd;

					// save off the data into the appropriate stream
					switch (nCmd) {
					case 1:		// vol0
					case 3:		// vol1
					case 5:		// vol2
					case 7:		// vol3
						// single byte (nibble) values
  						nCurrentTone[nCmd]=volumeTable[c];
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
					// non-command byte, use the previous command (from the right chip) and update
                    nCmd = lastCmd;

                    if (nCurrentTone[nCmd] == -1) nCurrentTone[nCmd]=0;
                    if (nCmd & 0x01) {
                        // volume
                        nCurrentTone[nCmd]=volumeTable[c&0x0f];
                    } else {
                        // tone
    					nCurrentTone[nCmd]&=0x00f;
	    				nCurrentTone[nCmd]|=(c&0xff)<<4;
                    }
				}
			}

			int delay = buffer[nOffset++];
			if (delay == 0) {
				nOffset = nEOF + 1;
				if (!outputData()) return -1;
				break;
			} else {
				for (int idx=0; idx<delay; ++idx) {
					if (!outputData()) return -1;
				}
			}
		}

		if (buffer[nOffset-1] != 0) {
			printf("Warning: did not get end frame\n");
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

    myprintf("done playlist2psg.\n");
    return 0;
}

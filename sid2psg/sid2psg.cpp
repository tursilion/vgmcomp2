// sid2psg.cpp : Play out a SID file and export PSG data files
// Part of the vgmcomp2 package.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAX_CHANNELS 18                     // up to 3 SID chips, 3 channels each, split tone and noise. 3*3*2
#define NOISESTART (MAX_CHANNELS/2)         // first noise channel

bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data
bool debug2 = false;                        // dump per row data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count
int channels = 0;                           // number of channels
int outTone[MAX_CHANNELS][MAXTICKS];        // tone and volume for the song - first half are tones, second noise
int outVol[MAX_CHANNELS][MAXTICKS];
double songSpeedScale = 1.0;                // changes the samples per frame to adjust speed
int duration = 0;                           // duration to play in seconds (mandatory)
double freqScale = 1.0;                     // frequency scale

// CSID stuff
int mainSID(char *fn, int st);              // prepare the SID
void play(unsigned char *stream, int len);  // plays for len bytes worth of sample data
int getFreq();                              // gets the CPU clock
extern int SIDamount;                       // number of SID chips
extern unsigned int SID_address[3];         // addresses of SID chips (0,1,2 - 0 if not present)
extern unsigned char memory[65536];         // C64 memory array
extern unsigned char envcnt[9];             // per channel envelope level (0-256)

int main(int argc, char *argv[]) {
	printf("Import C64 SID files - v20200730\n\n");

	if (argc < 2) {
		printf("sid2psg [-q] [-d] [-o <n>] [-add <n>] [-speedscale <n>] [-subtune <n>] <-len <n>> <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -speedscale <float> - scale song rate by float (1.0 = no change, 1.1=10%% faster, 0.9=10%% slower)\n");
        printf(" -subtune <n> - play subtune 'n', from 1-64 (default 1)\n");
        printf(" -len <n> - play duration in seconds (MUST be specified!)\n");
		printf(" <filename> - SID file to read.\n");
        printf("\nsid2psg uses csid by Hermit.\n");
		return -1;
	}
    verbose = true;
    int subtune = 1;
    int wavelen = 0;

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-q")) {
			verbose=false;
        } else if (0 == strcmp(argv[arg], "-d")) {
			debug = true;
        } else if (0 == strcmp(argv[arg], "-dd")) {
            // hidden command for even more debug output
			debug2 = true;
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
            // we don't know how many channels there will be - check after loading the MOD
            output = atoi(argv[arg]);
            printf("Output ONLY channel %d\n", output);
        } else if (0 == strcmp(argv[arg], "-speedscale")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'speedscale' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%lf", &songSpeedScale)) {
                printf("Failed to parse speedscale\n");
                return -1;
            }
            printf("Setting song speed scale to %lf\n", songSpeedScale);
        } else if (0 == strcmp(argv[arg], "-subtune")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'subtune' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%d", &subtune)) {
                printf("Failed to parse subtune\n");
                return -1;
            }
            if ((subtune < 1) || (subtune > 64)) {
                printf("Illegal subtune (must be 1-64)\n");
                return -1;
            }
        } else if (0 == strcmp(argv[arg], "-len")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'len' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%d", &duration)) {
                printf("Failed to parse len\n");
                return -1;
            }
            if (duration < 1) {
                printf("Illegal -len value\n");
                return -1;
            }
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    if (arg >= argc) {
        printf("Not enough arguments for filename.\n");
        return 1;
    }

    // prepare the SID file
    if (mainSID(argv[arg], subtune)) {
        printf("SID init failed.\n");
        return 1;
    }

    // check output
    channels = SIDamount*6;
    if (output-addout > channels) {
        printf("Output channel %d does not exist in module.\n", output);
        return 1;
    }
    if (duration == 0) {
        printf("You must specify a -len parameter for how long to play.\n");
        return 1;
    }

    // with a sample rate of 44100, a 60hz sample happens exactly every 735 samples,
    // so that is what we'll run the SID at. This way (hopefully) the original rate
    // (which is usually 50hz) comes through, giving us a free resample
    memset(outTone, 0, sizeof(outTone));
    memset(outVol, 0, sizeof(outVol));

    // we need to adapt the SID shift rates to the SN default
    // CONSTANT = 256^3 / CLOCK
    // SID hz = code / CONSTANT
    // SN code = 111860.8/hz
    // newcode = 111860.8 / (code / (16777216.0 / CLOCK))
    // newcode = (111860.8 * (16777216.0 / CLOCK)) / code
    // so - newcode = freqScale / code
    freqScale = (111860.8 * (16777216.0 / getFreq()));

    // duration in jiffies
    duration *= 60;
    if (verbose) printf("Playing...\n");

    int rows = 0;
    while (duration--) {
        static unsigned char *audioBuf = NULL;
        static int audioBufSize = 0;
        if (NULL == audioBuf) {
            audioBufSize = int(735*songSpeedScale)*2;
            audioBuf = (unsigned char*)malloc(audioBufSize);
            if (debug2) {
                // reset the file
                FILE *fp = fopen("sidsound.wav", "wb");
                if (NULL != fp) {
                    fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x1\0\x1\0\x44\xac\0\0\x44\xac\0\0\x10\0\x10\0data\0\0\0\0",
                        1,44,fp);
                    fclose(fp);
                }
            }
        }
        play(audioBuf, audioBufSize);

        // dump the SID data
        if (debug2) {
            // append the file
            FILE *fp = fopen("sidsound.wav", "ab");
            if (NULL != fp) {
                fwrite(audioBuf, 1, audioBufSize, fp);
                fclose(fp);
                wavelen += audioBufSize;
            }
        }

        // but what we want is the tracker information inside the sid player...
        for (int idx=0; idx<SIDamount*3; ++idx) {
            int period, vol;

            // first get the period of the channel - there are three channels on the SID
            // freqScale / SIDCODE = SNCODE
            int base = SID_address[idx/3];  // SID register base
            if (base == 0) {
                printf("Warning - bad SID address on channel index %d.\n", idx);
                // shouldn't happen...
                continue;
            }
            base+= (idx%3)*7;   // channel register base
            period = int(freqScale / (memory[base]+memory[base+1]*256) + 0.5);

            // next, get the current volume including ADSR. Scale by master
            // volume, then back to 8 bit
            vol = envcnt[idx] * (memory[SID_address[idx/3]+0x18]&0x0f) / 16;

            // now write it to the appropriate channel, depending on whether it's noise
            if (memory[base+4] & 0x80) {
                // it's a noise all right!
                outTone[idx+NOISESTART][rows] = period;
                outVol[idx+NOISESTART][rows] = vol;
                outTone[idx][rows] = 0;
                outVol[idx][rows] = 0;
                // debug row if requested
                if (debug2) printf("P:%5d V:%5d N - ", period, vol);
            } else {
                outTone[idx][rows] = period;
                outVol[idx][rows] = vol;
                outTone[idx+NOISESTART][rows] = 0;
                outVol[idx+NOISESTART][rows] = 0;
                // debug row if requested
                if (debug2) printf("P:%5d V:%5d   - ", period, vol);
            }
        }
        if (debug2) printf("\n");
        ++rows;
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

    // now write the data out
    int channum = addout;
    for (int idx=0; idx<MAX_CHANNELS; ++idx) {
        // first check for any data in a row
        bool data = false;
        for (int r=0; r<rows; ++r) {
            if (outVol[idx][r] > 0) {
                data=true;
                break;
            }
        }
        if (!data) continue;

        char buf[128];
        if (idx < NOISESTART) {
            sprintf(buf, "%s_ton%02d.60hz", argv[arg], channum++);
        } else {
            sprintf(buf, "%s_noi%02d.60hz", argv[arg], channum++);
        }

        if ((output != 0) && (channum-1 != output)) continue;

        FILE *fp = fopen(buf, "w");
        if (NULL == fp) {
            printf("Failed to open output file %s, code %d\n", buf, errno);
            return -1;
        }
        if (verbose) printf("-Writing channel %d as %s...\n", channum-1, buf);
        for (int r=0; r<rows; ++r) {
            fprintf(fp, "0x%08X,0x%02X\n", outTone[idx][r], outVol[idx][r]);
        }
        fclose(fp);
    }

    printf("Wrote %d rows...\n", rows);

    if ((debug2)&&(wavelen > 0)) {
        // patch the output wave file
        FILE *fp = fopen("sidsound.wav", "rb+");
        if (NULL != fp) {
            fseek(fp, 40, SEEK_SET);
            fputc(wavelen%0xff, fp);
            fputc((wavelen>>8)&0xff, fp);
            fputc((wavelen>>16)&0xff, fp);
            fputc((wavelen>>24)&0xff, fp);

            fseek(fp, 4, SEEK_SET);
            wavelen+=44;    // add size of header
            fputc(wavelen%0xff, fp);
            fputc((wavelen>>8)&0xff, fp);
            fputc((wavelen>>16)&0xff, fp);
            fputc((wavelen>>24)&0xff, fp);
            fclose(fp);
        }
    }

    printf("\n** DONE **\n");
    return 0;
}

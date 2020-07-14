// mod2psg.cpp : Import a MOD file and export PSG data files
// Part of the vgmcomp2 package.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "modplug\modplug.h"
#include "modplug\libmodplug\sndfile.h"
#include "kissfft\kiss_fft.h"
#include "kissfft\kiss_fftr.h"

struct _ModPlugFile
{
	CSoundFile mSoundFile;
};

bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data
bool debug2 = false;                        // dump per row data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count
int channels = 0;                           // number of channels
ModPlug_Settings settings;                  // MODPlug settings
ModPlugFile *pMod;                          // the module object itself
bool bAutoVol = true;                       // whether to automatically auto-volume
double VolScale = 1.0;                      // song volume scale
double FreqScale = 1.0;                     // song frequency scale
int snrcutoff = 70;                         // SNR cutoff for noise - less than this is considered noise
bool useDrums = false;                      // whether to look for DRUMS$ in the sample names
const char *forceNoise = NULL;              // comma-separated list of samples to make noise
const char *forceTone = NULL;               // comma-separated list of samples to make tone

// Guesstimate the SNR of a sample from 0-100 (percent)
// pSample is a pointer to sample data, uFlags describes it. len is number of samples
// The flags we care about are CHN_16BIT and CHN_STEREO
// uses a shared buffer, so not thread safe
// input timedata has nfft scalar points
// output freqdata has nfft/2+1 complex points
// using the real-only FFT mode
int guessSNR(int sampnum, void *pSample, int len, int nFlags) {
    // we should be able to tell after a couple seconds so a maximum is fine
    // needs to match the bucket count, oddly...?
    const int numFft = 65536;
    const int numSam = numFft;
    static float samp[numSam] = {0};
    static kiss_fft_cpx kout[numFft]={0,0}; // only numFft/2+1 are valid
    static kiss_fftr_cfg cfg	= NULL;

    // convert to float samples for ease of use
    if (len > numSam) len = numSam;
    if (len&1) --len;           // must be even
    if (len == 0) return 100;   // no sample as default, tone

    if (nFlags&CHN_16BIT) {
        if (nFlags&CHN_STEREO) {
            short *p = (short*)pSample;
            for (int idx=0; idx<len; ++idx) {
                char x = ((*p) + (*(p+1))) / 2;     // average to mono
                samp[idx] = (float)(x/32768.0);
                p+=2;
            }
        } else {
            short *p = (short*)pSample;
            for (int idx=0; idx<len; ++idx) {
                samp[idx] = (float)(*(p++)/32768.0);
            }
        }
    } else {
        if (nFlags&CHN_STEREO) {
            char *p = (char*)pSample;
            for (int idx=0; idx<len; ++idx) {
                char x = ((*p) + (*(p+1))) / 2;     // average to mono
                samp[idx] = (float)(x/128.0);
                p+=2;
            }
        } else {
            char *p = (char*)pSample;
            for (int idx=0; idx<len; ++idx) {
                samp[idx] = (float)(*(p++)/128.0);
            }
        }
    }
    // zero the rest of the buffer
    for (int idx=len; idx<numSam; ++idx) {
        samp[idx]=0;
    }

    if (NULL == cfg) {
        cfg = kiss_fftr_alloc(numFft, 0, NULL, NULL);
    }
    kiss_fftr(cfg, samp, kout);

    // so first, get the maximum
    double maximum = 0;
    int maxpos = 0;
    for (int idx=0; idx<numFft/2+1; ++idx) {
        double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
        if ((idx != 0) && (idx != numFft/2)) {
            mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
        }
        if (mag2 > maximum) {
            maximum=mag2;
            maxpos = idx;
        }
    }

    // write a bitmap representation
    if (debug2) {
        char buf[128];
        sprintf(buf, "spectrum%02d.bmp", sampnum);
        FILE *fp = fopen(buf, "wb");
        if (fp) {
            // write a 2048x1024x8 BMP (only colors 0 and 1 defined)
            // size is 2048*1024+54+4*256 = 0x00200436
            // offset is 54+4*256 = 0x00000436
            fwrite("BM\x36\x4\x20\0\0\0\0\0\x36\x4\0\0", 1, 14, fp);
            // bitmapinfo header
            fwrite("\x28\0\0\0\0\x8\0\0\0\x4\0\0\x1\0\x8\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 1, 40, fp);
            // color 0 black
            fwrite("\0\0\0\0", 1, 4, fp);
            // color 1 white
            fwrite("\xff\xff\xff\0", 1, 4, fp);
            // color 2 red
            fwrite("\0\0\xff\0", 1, 4, fp);
            for (int idx=3; idx<256; ++idx) {
                // no color
                fwrite("\0\0\0\0", 1, 4, fp);
            }
            // we could be clever about the pixel data, but screw it, just draw it
            // remember it's upside down
            static unsigned char buf[2048*1024] = {0};
            memset(buf, 0, sizeof(buf));
            double scale = 2048.0 / (numFft/2+1);
            double yscale = 1024 / maximum;
            // draw some red lines for the maximum harmonics
            if (maxpos > 10) {
                int harm = maxpos / 2;
                while (harm < numFft/2+1) {
                    int adr = (int)(harm*scale+.5);
                    for (int y=0; y<1024; ++y) {
                        buf[adr]=2;
                        adr+=2048;
                    }
                    harm+=maxpos/2;
                }
            }
            for (int idx=0; idx<numFft/2+1; ++idx) {
                double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
                if ((idx != 0) && (idx != numFft/2)) {
                    mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
                }
                int x = int(idx*scale+.5);
                int adr=x;
                for (int y=0; y<mag2*yscale; ++y) {
                    buf[adr]=1;
                    adr+=2048;
                    if (adr > sizeof(buf)) break;   // safety
                }
            }
            fwrite(buf, 1024, 2048, fp);
            fclose(fp);
        }
    }

    // now analyze the result - a pure tone should only
    // fill one bucket. An impure tone will add harmonics.
    // Noise will fill a block of consecutive buckets
    //
    // Observation of real samples shows that tones, even
    // noisy ones like guitars, generate nice curves around
    // their frequencies and show clear harmonics.
    // However, chords have multiple main frequencies.
    //
    // Noise, however, tends to cluster together in an area,
    // not necessarily touching, and doesn't show much for
    // harmonics. There is energy in most buckets though.
    //
    // Maybe it works to count the number of LOW energy buckets,
    // as even twangy tones like electric guitar are pretty clean
    // away from their main harmonics...

    // count the number of buckets with low but present energy
    int count = 0;
    for (int idx=0; idx<numFft/2+1; ++idx) {
        double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
        if ((idx != 0) && (idx != numFft/2)) {
            mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
        }
        // hand tuned values...
        if ((mag2 > maximum*0.001)&&(mag2 < maximum*0.07)) ++count;
    }
    return 100-(count*100/(numFft/2+1));
}

int main(int argc, char *argv[]) {
	printf("Import MOD tracker-style files - v20200714\n\n");

    // TODO: set up noise channels and output noises on them

    // TODO options:
    // - insvol (volume multiplier for instrument/sample, defaults to 1.0)
    // - instune (frequency multiplier for instrument/sample, defaults to 1.0)
    // - drumfreq (set a sample to a fixed drum frequency)

	if (argc < 2) {
		printf("mod2psg [-q] [-d] [-o <n>] [-add <n>] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -vol <float> - scale song volume by float (1.0=no change, disables automatic volume scale)\n");
        printf(" -tune <float> - scale song frequency by float (1.0=no change, 2.0=octave DOWN, 0.5=octave UP)\n");
        printf(" -snr <int> - SNR ratio from 1-100 - SNR less than this will be treated as noise (use -d to see estimates)\n");
        printf(" -usedrums - read the old DRUMS$ tag from the sample name from the old mod2psg - no autodetection\n");
        printf(" -forcenoise <str> - 'str' is a comma-separated list of sample indexes to make noise, no spaces!\n");
        printf(" -forcetone <str> - 'str' is a comma-separated list of sample indexes to make tone, no spaces!\n");
		printf(" <filename> - MOD file to read.\n");
        printf("\nmod2psg uses the ModPlug library and should be able to read anything it can.\n");
        printf("Supported types should be: ABC, MOD, S3M, XM, IT, 669, AMF, AMS, DBM, DMF, DSM, FAR,\n");
        printf("MDL, MED, MID, MTM, OKT, PTM, STM, ULT, UMX, MT2 and PSM.\n");
        printf("If you have Timidity .pat patches for MIDI, you can set the MMPAT_PATH_TO_CFG\n");
        printf("environment variable and it should be able to use them.\n");
		return -1;
	}
    verbose = true;

	int arg=1;
	while ((arg < argc-1) && (argv[arg][0]=='-')) {
		if (0 == strcmp(argv[arg], "-q")) {
			verbose=false;
        } else if (0 == strcmp(argv[arg], "-d")) {
			debug = true;
        } else if (0 == strcmp(argv[arg], "-dd")) {
            // hidden command for even more debug output
			debug2 = true;
        } else if (0 == strcmp(argv[arg], "-usedrums")) {
            useDrums = true;
        } else if (0 == strcmp(argv[arg], "-snr")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -snr parameter.\n");
                return -1;
            }
            ++arg;
            snrcutoff = atoi(argv[arg]);
            if ((snrcutoff < 0) || (snrcutoff > 101)) {
                printf("snr cutoff out of range - must be 0-101, got %d\n", snrcutoff);
                return -1;
            }
            printf("SNR cutoff is now %d\n", snrcutoff);
            if (snrcutoff == 0) {
                printf("All samples will be tone\n");
            } else if (snrcutoff==101) {
                printf("All samples will be noise.\n");
            }
        } else if (0 == strcmp(argv[arg], "-add")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -add parameter.\n");
                return -1;
            }
            ++arg;
            addout = atoi(argv[arg]);
            printf("Output channel index offset is now %d\n", addout);
        } else if (0 == strcmp(argv[arg], "-forcenoise")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -forcenoise parameter.\n");
                return -1;
            }
            ++arg;
            forceNoise = argv[arg];
        } else if (0 == strcmp(argv[arg], "-forcetone")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -forcetone parameter.\n");
                return -1;
            }
            ++arg;
            forceTone = argv[arg];
        } else if (0 == strcmp(argv[arg], "-o")) {
            if (arg+1 >= argc) {
                printf("Not enough arguments for -o parameter.\n");
                return -1;
            }
            ++arg;
            // we don't know how many channels there will be - check after loading the MOD
            output = atoi(argv[arg]);
            printf("Output ONLY channel %d\n", output);
        } else if (0 == strcmp(argv[arg], "-vol")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'vol' option\n");
                return -1;
            }
            bAutoVol = false;
            if (1 != sscanf(argv[arg], "%lf", &VolScale)) {
                printf("Failed to parse vol scale\n");
                return -1;
            }
            printf("Setting volume scale to %lf\n", VolScale);
        } else if (0 == strcmp(argv[arg], "-tune")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'tune' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%lf", &FreqScale)) {
                printf("Failed to parse tune frequency scale\n");
                return -1;
            }
            printf("Setting frequency scale to %lf\n", FreqScale);
		} else {
			printf("\rUnknown command '%s'\n", argv[arg]);
			return -1;
		}
		arg++;
	}

    // default settings - the ultimate process function is Mono8BitFirFilterRampMix()
    ModPlug_GetSettings(&settings);
    settings.mChannels = 1;
    settings.mFlags = MODPLUG_ENABLE_OVERSAMPLING | MODPLUG_ENABLE_NOISE_REDUCTION;
    settings.mBits = 16;
    settings.mFrequency = 44100;
    settings.mResamplingMode = MODPLUG_RESAMPLE_FIR;
    settings.mLoopCount = 0;
    ModPlug_SetSettings(&settings);

    // load the data
    {
        FILE *fp = fopen(argv[arg], "rb");
        if (NULL == fp) {
            printf("Failed to open '%s'\n", argv[arg]);
            return 1;
        }
        fseek(fp, 0, SEEK_END);
        int sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char *data = (unsigned char*)malloc(sz);
        sz = fread(data, 1, sz, fp);
        fclose(fp);

        // get ModPlug to parse the loaded data
        pMod = ModPlug_Load(data, sz);
        free(data); data=NULL;              // freed here, success or failure
        if (NULL == pMod) {
            printf("ModPlug failed to parse module\n");
            return 1;
        }
    }

    // mod-specific settings
    ModPlug_SetMasterVolume(pMod, 512);     // maximum volume

    // spit out some data!
    printf("Module name: %s\n", ModPlug_GetName(pMod));
    printf("Module type: ");
    switch (ModPlug_GetModuleType(pMod)) {
    case MOD_TYPE_NONE: printf("None (well that's odd...)\n"); break;
    case MOD_TYPE_MOD:  printf("MOD\n"); break;
    case MOD_TYPE_S3M:  printf("S3M\n"); break;
    case MOD_TYPE_XM:   printf("XM\n"); break;
    case MOD_TYPE_MED:  printf("MED\n"); break;
    case MOD_TYPE_MTM:  printf("MTM\n"); break;
    case MOD_TYPE_IT:   printf("IT\n"); break;
    case MOD_TYPE_669:  printf("669\n"); break;
    case MOD_TYPE_ULT:  printf("ULT\n"); break;
    case MOD_TYPE_STM:  printf("STM\n"); break;
    case MOD_TYPE_FAR:  printf("FAR\n"); break;
    case MOD_TYPE_AMF:  printf("AMF\n"); break;
    case MOD_TYPE_AMS:  printf("AMS\n"); break;
    case MOD_TYPE_DSM:  printf("DSM\n"); break;
    case MOD_TYPE_MDL:  printf("MDL\n"); break;
    case MOD_TYPE_OKT:  printf("OKT\n"); break;
    case MOD_TYPE_MID:  printf("MID\n"); break;
    case MOD_TYPE_DMF:  printf("DMF\n"); break;
    case MOD_TYPE_PTM:  printf("PTM\n"); break;
    case MOD_TYPE_DBM:  printf("DBM\n"); break;
    case MOD_TYPE_MT2:  printf("MT2\n"); break;
    case MOD_TYPE_AMF0: printf("AMF0\n"); break;
    case MOD_TYPE_PSM:  printf("PSM\n"); break;
    case MOD_TYPE_J2B:  printf("J2B\n"); break;
    case MOD_TYPE_ABC:  printf("ABC\n"); break;
    case MOD_TYPE_PAT:  printf("PAT\n"); break;
    case MOD_TYPE_UMX:  printf("UMX\n"); break;
        
    // unsupported types
    case MOD_TYPE_WAV:  printf("WAV\n");    // fallthrough
    default: printf("Unsupported file type.\n"); return 1;
    }
    channels = ModPlug_NumChannels(pMod);
    printf("Channels: %d\n", channels);

    if (useDrums) {
        // find and implement a 'drums' tag from the sample names
        // this is only supported on standard MODs, so don't use with instruments
        int cnt = ModPlug_NumSamples(pMod);
        if (cnt != 31) {
            printf("DRUMS tag supported only on standard 31 sample ProTracker MODs\n");
            return -1;
        }
        bool found = false;
        for (int idx=1; idx <= cnt; ++idx) {
            char buf[32];   // MUST be at least 32 bytes
            ModPlug_SampleName(pMod, idx, buf);
            if (0 == strncmp(buf,"DRUMS$",6)) {
                found = true;
                if (strlen(buf) != 14) {
                    printf("DRUMS tag found, but data is too short.\n");
                    return -1;
                }
                unsigned int x;
                if (1 != sscanf(&buf[6], "%x", &x)) {
                    printf("DRUMS tag found, but error reading hex value\n");
                    return -1;
                }
                for (int cnt=1; cnt<=31; ++cnt) {
                    MODINSTRUMENT *pIns = &pMod->mSoundFile.Ins[cnt];
                    if (NULL != pIns) {
                        pIns->isNoiseForced = true;
                        if (x&0x80000000) {
                            pIns->isNoise = true;
                        } else {
                            pIns->isNoise = false;
                        }
                    }
                    x<<=1;
                }
            }
        }
    }
    if (NULL != forceNoise) {
        while (forceNoise) {
            int x;
            if (1 != sscanf(forceNoise, "%d", &x)) {
                printf("Parse error reading forceNoise list\n");
                return -1;
            }
            if (x > MAX_INSTRUMENTS) {
                printf("%d is too large an instrument number for forceNoise\n",x);
                return -1;
            }
            MODINSTRUMENT *pIns = &pMod->mSoundFile.Ins[x];
            if (NULL != pIns) {
                pIns->isNoiseForced = true;
                pIns->isNoise = true;
            }
            forceNoise = strchr(forceNoise, ',');
            if (forceNoise) ++forceNoise;
        }
    }
    if (NULL != forceTone) {
        while (forceTone) {
            int x;
            if (1 != sscanf(forceTone, "%d", &x)) {
                printf("Parse error reading forceTone list\n");
                return -1;
            }
            if (x > MAX_INSTRUMENTS) {
                printf("%d is too large an instrument number for forceTone\n",x);
                return -1;
            }
            MODINSTRUMENT *pIns = &pMod->mSoundFile.Ins[x];
            if (NULL != pIns) {
                pIns->isNoiseForced = true;
                pIns->isNoise = false;
            }
            forceTone = strchr(forceTone, ',');
            if (forceTone) ++forceTone;
        }
    }

    // go through the internal converted instruments (which may be
    // instruments, samples, or both in the source file) and FFT
    // the sample data to see what we have.
    for (int idx = 0; idx<MAX_INSTRUMENTS; ++idx) {
        MODINSTRUMENT *pIns = &pMod->mSoundFile.Ins[idx];
        if (NULL == pIns) continue;
        if (NULL == pIns->pSample) continue;
        if (strlen(pIns->name) == 0) {
            char buf[32];
            ModPlug_SampleName(pMod, idx, buf);
            buf[21]=0;
            strcpy(pIns->name, buf);
        }
        if (pIns->isNoiseForced) {
            if (debug) printf("Instrument %2d IsNoise: %d (Forced)           (%s)\n", 
                idx, pIns->isNoise, pIns->name);
        } else {
            if (debug) printf("Analyzing instrument %d (%s)...", idx, pIns->name);
            pIns->SNR = guessSNR(idx, pIns->pSample, pIns->nLength, pIns->uFlags);
            if (pIns->SNR < snrcutoff) {
                pIns->isNoise = true;
            }
            if (debug) printf("\rInstrument %2d IsNoise: %d Estimated SNR: %2d%% (%s)\n", 
                idx, pIns->isNoise, pIns->SNR, pIns->name);
        }
    }

    if (verbose) {
        if (NULL != ModPlug_GetMessage(pMod)) {
            printf("Message: %s\n", ModPlug_GetMessage(pMod));
        }
        int cnt = ModPlug_NumInstruments(pMod);
        for (int idx=1; idx<=(cnt+1)/2; ++idx) {
            char buf[32];   // MUST be at least 32 bytes
            char buf2[32];
            ModPlug_InstrumentName(pMod, idx, buf);
            if (idx+(cnt+1)/2 <= cnt) {
                ModPlug_InstrumentName(pMod, idx+(cnt+1)/2, buf2);  // already checks for out of range
                printf("Instrument %2d: %-32s    %2d: %s\n", idx, buf, idx+(cnt+1)/2, buf2);
            } else {
                printf("Instrument %2d: %s\n", idx, buf);
            }
        }
        cnt = ModPlug_NumSamples(pMod);
        for (int idx=1; idx <= (cnt+1)/2; ++idx) {
            char buf[32];   // MUST be at least 32 bytes
            char buf2[32];
            ModPlug_SampleName(pMod, idx, buf);
            if (idx+(cnt+1)/2 <= cnt) {
                ModPlug_SampleName(pMod, idx+(cnt+1)/2, buf2);  // already checks for out of range
                printf("Sample %2d: %-32s    %2d: %s\n", idx, buf, idx+(cnt+1)/2, buf2);
            } else {
                printf("Sample %2d: %s\n", idx, buf);
            }
        }
    }

    // check output
    if (output > channels) {
        printf("Output channel %d does not exist in module.\n", output);
        return 1;
    }

    // with a sample rate of 44100, a 60hz sample happens exactly every 735 samples,
    // so that is what we'll run the MOD at. This way (hopefully) the original rate
    // (which is usually 50hz) comes through, giving us a free resample

    // open a file for each channel
    FILE *fp[MAX_CHANNELS];
    for (int idx=0; idx<MAX_CHANNELS; ++idx) {
        fp[idx] = NULL;
    }
    for (int idx=0; idx<channels; ++idx) {
        char buf[128];
        if ((output > 0) && (idx != output-1)) continue;
        int n = idx + addout;
        sprintf(buf, "%s_ton%02d.60hz", argv[arg], n);
        fp[idx] = fopen(buf, "w");
        if (NULL == fp[idx]) {
            printf("Failed to open output file '%s', code %d\n", buf, errno);
            return 1;
        } else {
            printf("Opened '%s' for output...\n", buf);
        }
    }

    // looping on autovol with manual break
    int rows;
    for (;;) {
        int maxVol = 0;

        ModPlug_Seek(pMod, 0);
        rows = 0;

        if (bAutoVol) {
            if (verbose) printf("Scanning for volume...\n");
        }

        for (;;) {
            char audioBuf[735*2];   // 735 16-bit mono samples
            int ret = ModPlug_Read(pMod, audioBuf, sizeof(audioBuf));
            if (ret < sizeof(audioBuf)) break;

            // but what we want is the tracker information inside the mod...
            for (int idx=0; idx<channels; ++idx) {
                // get the average volume over the period
                int vol;
                if (pMod->mSoundFile.Chn[idx].nAvgCnt == 0) {
                    // no activity on channel
                    vol = 0;
                } else {
                    // the result is a 16-bit magnitude (0-65535)
                    double tmpVol = ((double)pMod->mSoundFile.Chn[idx].nAvgVol / pMod->mSoundFile.Chn[idx].nAvgCnt) * VolScale;
                    vol = (int)(tmpVol+0.5);
                    // finally, shift 16 bits down to 8
                    vol >>= 8;
                }
                // vol should already be in the 8 bit magnitude we expect
                if (vol > 0xff) {
                    if (vol > 255+25) {
                        printf("Warning: clipping (%d vs 255)\n", vol);
                    }
                    vol = 0xff;
                }

                // now deal with the period
                int period = pMod->mSoundFile.Chn[idx].nFinalPeriod;
                // period is based on 3.5MHz, we want based on 111860.8Hz
                // PAL Amiga frequency used here, since most MODs are PAL
                // Not sure offhand why I need the /4, but it sounds right.
                // It looks like the periods might be multiplied by 4 as they are loaded...?
                period = (int)((double)period * FreqScale / (3546895 / 111860.8 / 4) + 0.5);
                if (period == 0) {
                    if (FreqScale < 1.0) {
                        printf("Warning: period became zero - frequency scale too low\n");
                    }
                    // don't allow 0
                    period = 1;
                }

                // and dump it to the file, if this isn't the auto pass
                if (bAutoVol) {
                    // autovol pass - check levels
                    if (vol > maxVol) maxVol = vol;
                } else {
                    // fixed pass, write it out
                    if (fp[idx]) {
                        fprintf(fp[idx], "0x%08X,0x%02X\n", period, vol);
                    }
                    // debug row if requested
                    if (debug2) printf("P:%5d V:%5d - ", period, vol);
                }
                // reset the average counters for the next segment
                pMod->mSoundFile.Chn[idx].nAvgVol = 0;
                pMod->mSoundFile.Chn[idx].nAvgCnt = 0;
            }
            if ((debug2)&&(!bAutoVol)) printf("\n");
            ++rows;
        }

        if (bAutoVol) {
            // calculate a volume scale ratio to max out the song, then repeat
            VolScale = (double)255/maxVol;
            bAutoVol = false;
            printf("Calculated volume scale of %lf\n", VolScale);
        } else {
            // we're done, do break
            break;
        }
    }

    for (int idx=0; idx<channels; ++idx) {
        if (fp[idx]) {
            fclose(fp[idx]);
        }
    }

    printf("Wrote %d rows...\n", rows);
    printf("\n** DONE **\n");
    return 0;
}

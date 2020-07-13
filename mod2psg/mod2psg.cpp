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

struct _ModPlugFile
{
	CSoundFile mSoundFile;
};

bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count
int channels = 0;                           // number of channels
ModPlug_Settings settings;                  // MODPlug settings
ModPlugFile *pMod;                          // the module object itself
bool bAutoVol = true;                       // whether to automatically auto-volume
double VolScale = 1.0;                      // song volume scale
double FreqScale = 1.0;                     // song frequency scale

int main(int argc, char *argv[]) {
	printf("Import MOD tracker-style files - v20200712\n\n");

    // TODO: set up noise channels and output noises on them
    // TODO: can we auto-detect noises? Certainly should be able to for MIDI, aren't they fixed?

    // TODO options:
    // - insvol (volume multiplier for instrument/sample, defaults to 1.0)
    // - instune (frequency multiplier for instrument/sample, defaults to 1.0)
    // - drumfreq (set a sample to a fixed drum frequency)
    // - drums (list of noise channels)

	if (argc < 2) {
		printf("mod2psg [-q] [-d] [-o <n>] [-add <n>] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -vol <float> - scale song volume by float (1.0=no change, disables automatic volume scale)\n");
        printf(" -tune <float> - scale song frequency by float (1.0=no change, 2.0=octave DOWN, 0.5=octave UP)\n");
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

    if (verbose) {
        if (NULL != ModPlug_GetMessage(pMod)) {
            printf("Message: %s\n", ModPlug_GetMessage(pMod));
        }
        int cnt = ModPlug_NumInstruments(pMod);
        for (int idx=0; idx<cnt/2; ++idx) {
            char buf[32];   // MUST be at least 32 bytes
            char buf2[32];
            ModPlug_InstrumentName(pMod, idx, buf);
            ModPlug_InstrumentName(pMod, idx+cnt/2, buf2);  // already checks for out of range
            printf("Instrument %2d: %-32s    %2d: %s\n", idx, buf, idx+cnt/2, buf2);
        }
        cnt = ModPlug_NumSamples(pMod);
        for (int idx=0; idx < cnt/2; ++idx) {
            char buf[32];   // MUST be at least 32 bytes
            char buf2[32];
            ModPlug_SampleName(pMod, idx, buf);
            ModPlug_SampleName(pMod, idx+cnt/2, buf2);  // already checks for out of range
            printf("Sample %2d: %-32s    %2d: %s\n", idx, buf, idx+cnt/2, buf2);
        }
    }

    // check output
    if (output > channels) {
        printf("Output channel %d does not exist in module.\n", output);
        return 1;
    }

    // TODO: can we analyze the samples to see whether they are noise or tone?
    // How do we do instruments? Are they always based on a sample too?
    // Remember we need to double the output channels to allow for noise or tone on any of them


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
                period *= FreqScale;
                period /= (int)(3546895 / 111860.8 / 4 + 0.5);
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
                    if (debug) printf("P:%5d V:%5d - ", period, vol);
                }
                // reset the average counters for the next segment
                pMod->mSoundFile.Chn[idx].nAvgVol = 0;
                pMod->mSoundFile.Chn[idx].nAvgCnt = 0;
            }
            if ((debug)&&(!bAutoVol)) printf("\n");
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

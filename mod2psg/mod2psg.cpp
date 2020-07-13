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

// frequency compensation for NTSC playback
// note: true NTSC is 3.579545, but the TI-99 clocked at 3.579543. I've
// compromised on this 99.9999% offset is compromised, to reduce error
// on both TI and systems which used the correct clock like SMS).
const double NTSC_COMP = (3579544.0 / 3546895.0);

int main(int argc, char *argv[]) {
	printf("Import MOD tracker-style files - v20200704\n\n");

	if (argc < 2) {
		printf("mod2psg [-q] [-d] [-o <n>] [-add <n>] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
		printf(" <filename> - MOD file to read.\n");
        printf("\nmod2psg uses the ModPlug library and should be able to read anything it can.\n");
        printf("Supported types should be: ABC, MOD, S3M, XM, IT, 669, AMF, AMS, DBM, DMF, DSM, FAR,\n");
        printf("MDL, MED, MID, MTM, OKT, PTM, STM, ULT, UMX, MT2 and PSM.\n");
        printf("If you have Timidity .pat patches for MIDI, you can set the MMPAT_PATH_TO_CFG\n");
        printf("environment variable and it should be able to use them.\n");
        printf("Will they help? I have no idea. ;)\n");
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

    int rows = 0;
    for (;;) {
        char audioBuf[735*2];   // 735 16-bit mono samples
        int ret = ModPlug_Read(pMod, audioBuf, sizeof(audioBuf));
        if (ret < sizeof(audioBuf)) break;

        // but what we want is the tracker information inside the mod...
        for (int idx=0; idx<channels; ++idx) {
            int period = pMod->mSoundFile.Chn[idx].nFinalPeriod;    // probably normally the same... but that's ok
            // get the average volume over the period
            int vol = pMod->mSoundFile.Chn[idx].nAvgCnt == 0 ? 0 : pMod->mSoundFile.Chn[idx].nAvgVol / pMod->mSoundFile.Chn[idx].nAvgCnt;    
            // vol should already be in the 8 bit magnitude we expect
            if (vol > 0xff) vol = 0xff;
            // period is based on 3.5MHz, we want based on 111860.8Hz
            // Adjust for NTSC clocking
            period = (int)(period * NTSC_COMP + 0.5);
            // PAL Amiga frequency used here, since most MODs are PAL
            period /= (int)(3546895 / 111860.8 / 2);
            if (fp[idx]) {
                fprintf(fp[idx], "0x%08X,0x%02X\n", period, vol);
            }
            // debug row if requested
            if (debug) printf("P:%5d V:%5d - ", period, vol);
            // reset the average counters for the next segment
            pMod->mSoundFile.Chn[idx].nAvgVol = 0;
            pMod->mSoundFile.Chn[idx].nAvgCnt = 0;
        }
        if (debug) printf("\n");
        ++rows;
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

// voice2psg.cpp : Use the FFT to convert a voice sample to PSG
// Conversion based on Artrag's Matlab project.
// Part of the vgmcomp2 package.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "kissfft\kiss_fft.h"
#include "kissfft\kiss_fftr.h"

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAX_CHANNELS 7                      // up to 6 voice channels (two chips) plus one noise

bool verbose = false;                       // emit more information
bool debug = false;                         // dump parser data
bool debug2 = false;                        // dump per row data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count
int channels = 3;                           // number of channels (one chip by default)
int outTone[MAX_CHANNELS][MAXTICKS];        // tone and volume for the song - last one is noise
int outVol[MAX_CHANNELS][MAXTICKS];
double songSpeedScale = 1.0;                // changes the samples per frame to adjust speed

// return memory is allocated and must be freed by the user
// also fills in 'chans', 'freq', 'bits', and 'samples'
unsigned char *readwav(char *fn, int &chans, int &freq, int &bits, int &samples) {
	unsigned char buf[44];
	unsigned char *ret;

	// not intended to be all-encompassing, just intended to work
	FILE *fp = fopen(fn, "rb");
	if (NULL == fp) {
		printf("* can't open %s (code %d)\n", fn, errno);
		return NULL;
	}

	if (1 != fread(buf, 44, 1, fp)) {
		fclose(fp);
		printf("* can't read WAV header on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(buf, "RIFF", 4)) {
		fclose(fp);
		printf("* not RIFF on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(&buf[8], "WAVE", 4)) {
		fclose(fp);
		printf("* not WAVE on %s\n", fn);
		return NULL;
	}

	if (0 != memcmp(&buf[12], "fmt ", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* fmt not as expected on %s\n", fn);
		return NULL;
	}

	// okay, we know that we have at least got a header
	if (0 != memcmp(&buf[20], "\x1\x0", 2)) {
		fclose(fp);
		printf("* not PCM on %s\n", fn);
		return NULL;
	}

	chans = buf[22] + buf[23]*256;
	if ((chans < 1) || (chans > 2)) {
		fclose(fp);
		printf("* %d chans on %s (support 1-2)\n", chans, fn);
		return NULL;
	}

	freq = buf[24] + buf[25]*256 + buf[26]*65536 + buf[27]*16777216;
	if ((freq < 1000) || (freq > 224000)) {
		fclose(fp);
		printf("Freq of %d on %s (support 1000-224000)\n", freq, fn);
		return NULL;
	}

	bits = buf[34] + buf[35]*256;
	if ((bits != 8)&&(bits != 16)) {
		fclose(fp);
		printf("Bits are %d on %s (support 8,16)\n", bits, fn);
		return NULL;
	}

	if (0 != memcmp(&buf[36], "data", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* data not as expected on %s\n", fn);
		return NULL;
	}

	samples = buf[40] + buf[41]*256 + buf[42]*65536 + buf[43]*16777216;	// data size in bytes
	
	// use that value to get our buffer before we scale it down to actual samples
	ret = (unsigned char*)malloc(samples);

	// and read it - this works if the above did since we are stopped
	// right at the beginning of the data chunk
	if (fread(ret, 1, samples, fp) != samples) {
		printf("+ warning: failed to read all samples from %s\n", fn);
	}
	fclose(fp);

	// now convert bytes to samples
	samples /= (bits/8);	// by bytes per sample
	samples /= chans;		// by number of channels - NOW it's samples

	// and we're done
	return ret;
}
                                            
// Based on the sample code by Artrag
// pSample is a pointer to sample data, len is number of samples.
// voices is number of tone voices to return. freqs is the array
// of tone frequencies. vols is the array of tone volumes.
// noisefreq is the top of the noise floor, roughly, and noisevol
// is the volume of the noise floor.
//
// pSample must be 44100hz, signed float, mono samples
// we expect len to be 735 samples (44100/60), and adapt if less.
//
// uses a shared buffer, so not thread safe
// input timedata has nfft scalar points
// output freqdata has nfft/2+1 complex points
// using the real-only FFT mode
//
// return true on success and false on failure
bool guessInfo(float *pSample, int len, int voices, int *freqs, int *vols, int &noisefreq, int &noisevol) {
    // num of FFTs reflect the steps we have available on the chip.
    // how to interpret: https://www.gaussianwaves.com/2015/11/interpreting-fft-results-complex-dft-frequency-bins-and-fftshift/
    const int numFft = 8192;    // 1/2 of these buckets are useful to us
    const int numSam = numFft;
    static float samp[numSam] = {0};
    static kiss_fft_cpx kout[numFft]={0,0}; // only numFft/2+1 are valid
    static kiss_fftr_cfg cfg	= NULL;

    // convert to float samples for ease of use
    if (len > numSam) len = numSam;
    if (len&1) --len;               // must be even
    if (len == 0) return false;     // no sample as default, tone

    // copy as much of the sample into our work buffer
    // this probably isn't needed in this implementation...
    for (int i=0; i<len; ++i) {
        samp[i] = pSample[i];
    }

    // if len < 735 bytes, then duplicate it, as that's too short
    // it's okay if speedscale caused this
    if (len < 735) {
        for (int idx=len; idx<735; ++idx) {
            samp[idx] = samp[idx%len];
        }
        len = 735;
    }

    // zero the rest of the buffer (duplicating is much worse)
    for (int idx=len; idx<numSam; ++idx) {
        samp[idx]=0;
    }

    if (NULL == cfg) {
        cfg = kiss_fftr_alloc(numFft, 0, NULL, NULL);
    }
    kiss_fftr(cfg, samp, kout);

    // out of the FFT, from 0-numFft/2 are frequencies 0-22050
    double scale = 22050 / (numFft/2);

    // so first, get the maximum
    // It's unclear to me what the range of maximum is. matlab appears to
    // return 0-1.0 for the real and imaginary parts, but here I'm getting
    // maximum values over 233,000, implying parts as high as 483. (A^2 + B^2 = C^2)
    // Since we are doign a known quantity here, we'll try using the maximum
    // as max volume for this frame and scaling everything by it
    // If this doesn't work, we can do the same but using a maximum for the whole sample.
    double maximum = 0;
    int maxpos = 0;
    for (int idx=0; idx<numFft/2+1; ++idx) {
        double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
        if ((idx != 0) && (idx != numFft/2)) {
            mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
        }
        if (mag2 > maximum) {
            if (idx*scale < 110) continue;  // can't play that low
            maximum=mag2;
            maxpos = idx;
        }
    }

    // we have the top peak, so go ahead and return that
    // Note: Artrag did this for volume - does this make any
    // sense in /this/ context?
    // y = y + amp(j)*(sin(2*pi*freq(j)*t));   // amplitude * sinewave(frequency * time)
    int cnt = 0;
    freqs[cnt] = int(maxpos*scale+.5);  // hz
    vols[cnt] = 255;    // maximum is maximum volume
    ++cnt;

    // wipe the peak and immediately around it
    kout[maxpos-1].r = kout[maxpos-1].i = 0;
    kout[maxpos].r = kout[maxpos].i = 0;
    kout[maxpos+1].r = kout[maxpos+1].i = 0;

    // find and wipe the next 'x' top peaks
    for (int i = cnt; i<voices; ++i) {
        double tmaximum = 0;
        int tmaxpos = 0;
        for (int idx=0; idx<numFft/2+1; ++idx) {
            double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
            if ((idx != 0) && (idx != numFft/2)) {
                mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
            }
            if (mag2 > tmaximum) {
                if (idx*scale < 110) continue;  // can't play that low
                tmaximum=mag2;
                tmaxpos = idx;
            }
        }

        freqs[cnt] = int(tmaxpos * scale+0.5);
        if (maximum == 0) {
            vols[cnt] = 0;
        } else {
            vols[cnt] = int((tmaximum / maximum) * 255);
        }
        ++cnt;

        // wipe the peak and immediately around it
        kout[tmaxpos-1].r = kout[tmaxpos-1].i = 0;
        kout[tmaxpos].r = kout[tmaxpos].i = 0;
        kout[tmaxpos+1].r = kout[tmaxpos+1].i = 0;
    }

    // now we do a floor noise analysis. This more or less
    // worked in mod2psg for determining noise, let's see
    // if it does any good at all for voice...

    // first, filter out the harmonics of maximum - nuke 1 buckets around each
    // not sure if I should do the .5max or not...?
    // skip if it's less than 10
    if (maxpos >= 10) {
        int idx = maxpos;
        while (idx < numFft/2+1) {
            kout[idx-1].r = kout[idx-1].i = 0;
            kout[idx].r = kout[idx].i = 0;
            kout[idx+1].r = kout[idx+1].i = 0;
            idx+=maxpos;
        }
    }

    // average the remaining energy, and track the highest frequency
    int count = 0;
    int maxnoisefreq = 0;
    double noiseavg = 0;
    int noisecnt = 0;
    // this one we WILL examine all buckets
    for (int idx=0; idx<numFft/2+1; ++idx) {
        double mag2=kout[idx].r*kout[idx].r + kout[idx].i*kout[idx].i;
        if ((idx != 0) && (idx != numFft/2)) {
            mag2*=2;    // symmetric counterpart implied, except for 0 (DC) and len/2 (Nyquist)
        }
        // ignore if under 5%
        if (maximum > 0) {
            if (mag2 / maximum > 0.05) {
                if (idx*scale < 110) continue;  // can't play that low
                maxnoisefreq = idx;
                noiseavg+=mag2;
                noisecnt++;
            }
        }
    }

    // more than 20% of FFT is noise
    if ((maximum > 0) && (((double)noisecnt/(numFft/2)) > 0.2)) {
        noisefreq = int(maxnoisefreq * scale + .5);
        noisevol = int(((noiseavg/noisecnt) / maximum) * 255);
    } else {
        noisevol = 0;
    }

    return true;
}

int main(int argc, char *argv[]) {
	printf("Parse voice to PSG - v20200730\n\n");

	if (argc < 2) {
		printf("voice2psg [-q] [-d] [-o <n>] [-add <n>] [-speedscale <n>] [-channels <n>] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -speedscale <float> - scale sample rate by float (1.0 = no change, 1.1=10%% faster, 0.9=10%% slower)\n");
        printf(" -channels <n> - number of channels to create (1-6, default 3, noise always created)\n");
		printf(" <filename> - wave file to read.\n");
        printf("\nBased on sample by Artrag\n");
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
        } else if (0 == strcmp(argv[arg], "-channels")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'channels' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%d", &channels)) {
                printf("Failed to parse channels\n");
                return -1;
            }
            if ((channels < 1) || (channels > 6)) {
                printf("Invalid channel count (%d) - must be 1-6\n", channels);
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
    // check output
    if (output-addout > channels) {
        printf("Output channel %d does not exist in output.\n", output);
        return 1;
    }

    // read in a wave file, then convert it to floating point samples
    // at 44100 hz.
    float *sampledata;
    int samples;
    {
        int chans, freq, bits;
        chans=freq=bits=samples=0;
        unsigned char *wavdata = readwav(argv[arg], chans, freq, bits, samples);
        if (NULL == wavdata) {
            printf("Failed to read wave file '%s'\n", argv[arg]);
            free(wavdata);
            return -1;
        }
        if ((bits != 8)&&(bits != 16)) {
            printf("Wave must be 8 or 16 (LE) bits only (got %d).\n", bits);
            free(wavdata);
            return -1;
        }
        if ((chans < 1)||(chans > 2)) {
            printf("Wave must be 1 or 2 channels only (got %d)\n", chans);
            free(wavdata);
            return -1;
        }

        // convert to a set of floating point samples for the FFT
        // resample to 44100 as we go
        int newsamples = int(((double)samples/freq) * 44100.0);
        double samplestep = (double)freq/44100.0;
        sampledata = (float*)malloc(sizeof(float)*newsamples);
        if (NULL == sampledata) { 
            printf("Can't allocate sample buffer.\n");
            free(wavdata);
            return -1;
        }
        double offset = 0;
        // resample with linear interpolation
        // also convert to mono float samples
        // only support mono or stereo, 8 or 16 bit
        for (int i=0; i<newsamples; ++i) {
            unsigned char *p = wavdata + int(floor(offset)*chans*(bits/8));
            float sample = 0;
            if (bits == 16) {
                short s1,s2;
                s1 = *p + ((*(p+1))<<8);
                if (chans == 2) {
                    p+=2;
                    s1 += *p + ((*(p+1))<<8);
                    s1 /= 2;
                }

                p+=2;
                s2 = *p + ((*(p+1))<<8);
                if (chans == 2) {
                    p+=2;
                    s2 += *p + ((*(p+1))<<8);
                    s2 /= 2;
                }

                // ratio is the percentage of s2 we take
                double ratio = offset - floor(offset);
                sample = float(((s1/32768.0)*(1-ratio)) + ((s2/32768.0)*(ratio)));
            } else {
                signed char s1,s2;
                s1 = *p;
                if (chans == 2) {
                    p++;
                    s1 += *p;
                    s1 /= 2;
                }

                p++;
                s2 = *p;
                if (chans == 2) {
                    p++;
                    s2 += *p;
                    s2 /= 2;
                }

                // ratio is the percentage of s2 we take
                double ratio = offset - floor(offset);
                sample = float(((s1/128.0)*(1-ratio)) + ((s2/128.0)*(ratio)));
            }
            sampledata[i] = sample;
            offset+=samplestep;
        }
        samples = newsamples;
        free(wavdata);

        if (debug2) {
            // write out the converted wave file to ensure it's correct
            // we'll write signed 16-bit for simplicity
            FILE *fp = fopen("samplesound.wav", "wb");
            if (NULL != fp) {
                fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x1\0\x1\0\x44\xac\0\0\x44\xac\0\0\x10\0\x10\0data\0\0\0\0",
                    1,44,fp);
                for (int idx=0; idx<samples; ++idx) {
                    short x = short(32767*sampledata[idx]);
                    fwrite(&x, 2, 1, fp);
                }
                fclose(fp);
                fp = fopen("samplesound.wav", "rb+");
                if (NULL != fp) {
                    int wavelen = samples*2;
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
        }
    }

    // with a sample rate of 44100, a 60hz sample happens exactly every 735 samples,
    // so that is what we'll run the sample at.
    memset(outTone, 0, sizeof(outTone));
    memset(outVol, 0, sizeof(outVol));

    int rows = 0;
    int samplepos = 0;
    int freqs[MAX_CHANNELS];
    int vols[MAX_CHANNELS];
    int noisefreq = 0;
    int noisevol = 0;
    int step735 = int(735*songSpeedScale+.5);

    // process 735 samples at a time - partial last frame isn't important
    while (samplepos + step735 < samples) {
        if (!guessInfo(&sampledata[samplepos], step735, channels, freqs, vols, noisefreq, noisevol)) {
            printf("GuessInfo failed to parse sample.\n");
            return -1;
        }

        // fill in the outputs - convert frequency
        for (int idx=0; idx<channels; ++idx) {
            if (freqs[idx] < 1) freqs[idx] = 1;
            if (freqs[idx] > 1023) freqs[idx] = 1023;
            outTone[idx][rows] = int(111860.8 / freqs[idx] + 0.5);
            outVol[idx][rows] = vols[idx];

            if (outTone[idx][rows] > 0xfff) {
                outTone[idx][rows] = 0xfff;
            }
        }
        outTone[MAX_CHANNELS-1][rows] = noisefreq;
        outVol[MAX_CHANNELS-1][rows] = noisevol;
        if (outTone[MAX_CHANNELS-1][rows] > 0xfff) {
            outTone[MAX_CHANNELS-1][rows] = 0xfff;
        }

        ++rows;
        samplepos += step735;
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
        // shouldn't be any high frequencies to worry about
        bool data = false;
        for (int r=0; r<rows; ++r) {
            if (outVol[idx][r] > 0) {
                data=true;
                break;
            }
        }
        if (!data) continue;

        char buf[128];
        if (idx != MAX_CHANNELS - 1) {
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

    printf("\n** DONE **\n");
    return 0;
}

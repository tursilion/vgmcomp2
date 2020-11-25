// voice2psg.cpp : Use the FFT to convert a voice sample to PSG
// Inspired by Artrag's Matlab project, but redone from scratch because MATLAB.
// Part of the vgmcomp2 package.
// Apparently another thing that helps is 120hz playback. But since this toolchain
// is locked to 60hz, we are kind of stuck there. Wonder if there are other tricks?
// Everyone else resamples down to 8khz and starts there, but that doesn't seem to
// help in my tests, and wouldn't it just degrade the signal anyway?

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "Gist/gist.h"

#define MAXTICKS 432000					    // about 2 hrs, but arbitrary
#define MAX_CHANNELS 32                     // normally up to 6 voice channels (two chips) plus one noise, but experimenting with more
                                            // sounds very recognizable at 12, not much improvement after 24. Gets softer with more channels.
                                            // Even 6 is not bad, though subtitles would be a good idea.

bool verbose = true;                        // emit more information
bool debug = false;                         // dump parser data
bool debug2 = false;                        // dump per row data
int output = 0;                             // which channel to output (0=all)
int addout = 0;                             // fixed value to add to the output count
int channels = 3;                           // number of channels (one chip by default)
int outTone[MAX_CHANNELS][MAXTICKS];        // tone and volume for the song - last one is noise
int outVol[MAX_CHANNELS][MAXTICKS];
double songSpeedScale = 1.0;                // changes the samples per frame to adjust speed
int sampleRate = 44100;
int maxFreq = 4000;                         // maximum frequency to search for (4000hz for 8khz sample rate - telephone quality)
double volumeScale = 4.0;                   // default volume scale used during energy conversion
int minStep = 5;                            // minimum frequency step PERCENT difference required to accept a new entry (todo: not configurable)

// returns a vector of doubles for the samples (-1 to +1)
// wave is converted from 8 or 16 bit to floating point, and from stereo to mono
// Unlike the matlab version, we'll do the resample at the same time, since
// that will save a copy or two. So you know the return type and frequency.
// After resampling, we maximize the volume before finally returning.
// returns empty vector on failure
double *wavread(const  char* name, int newFreq, int &samples) {
	unsigned char buf[512];
	double *ret = NULL;

	// not intended to be all-encompassing, just intended to work
	FILE *fp = fopen(name, "rb");
	if (NULL == fp) {
		printf("* can't open %s (code %d)\n", name, errno);
		return ret;
	}

	if (1 != fread(buf, sizeof(buf), 1, fp)) {
		fclose(fp);
		printf("* can't read WAV header on %s\n", name);
		return ret;
	}

	if (0 != memcmp(buf, "RIFF", 4)) {
		fclose(fp);
		printf("* not RIFF on %s\n", name);
		return ret;
	}

	if (0 != memcmp(&buf[8], "WAVE", 4)) {
		fclose(fp);
		printf("* not WAVE on %s\n", name);
		return ret;
	}

	if (0 != memcmp(&buf[12], "fmt ", 4)) {
		// technically this could be elsewhere, but that'd be unusual
		fclose(fp);
		printf("* fmt not as expected on %s\n", name);
		return ret;
	}

	// okay, we know that we have at least got a header
	if (0 != memcmp(&buf[20], "\x1\x0", 2)) {
		fclose(fp);
		printf("* not PCM on %s\n", name);
		return ret;
	}

	int chans = buf[22] + buf[23]*256;
	if ((chans < 1) || (chans > 2)) {
		fclose(fp);
		printf("* %d chans on %s (support 1-2)\n", chans, name);
		return ret;
	}

	int freq = buf[24] + buf[25]*256 + buf[26]*65536 + buf[27]*16777216;
	if ((freq < 1000) || (freq > 224000)) {
		fclose(fp);
		printf("Freq of %d on %s (support 1000-224000)\n", freq, name);
		return ret;
	}

	int bits = buf[34] + buf[35]*256;
	if ((bits != 8)&&(bits != 16)) {
		fclose(fp);
		printf("Bits are %d on %s (support 8,16)\n", bits, name);
		return ret;
	}

    // find the beginning of the data chunk
    int pos = 36;
    for (;;) {
        fseek(fp, pos, SEEK_SET);
        if (feof(fp)) {
		    fclose(fp);
		    printf("* data chunk not found on %s\n", name);
		    return ret;
	    }

        fread(buf, 8, 1, fp);
        if (0 == memcmp(buf, "data", 4)) {
            break;
        }

        int size = *((int*)&buf[4]);
        if (size < 0) {
		    fclose(fp);
		    printf("* parse error looking for data chunk on %s\n", name);
		    return ret;
	    }

        pos+=8+size;
    }

	int bytes = buf[4] + buf[5]*256 + buf[6]*65536 + buf[7]*16777216;	// data size in bytes

    // check if it's sane! More than, say, 100MB is just too silly)
    if (bytes > 100*1024*1024) {
        printf("Wave file too large - cut it into pieces!\n");
        return ret;
    }
	
	// use that value to get our buffer before we scale it down to actual samples
    unsigned char *tmpBuf = (unsigned char*)malloc(bytes);

	// and read it - this works if the above did since we are stopped
	// right at the beginning of the data chunk
	if (fread(tmpBuf, 1, bytes, fp) != bytes) {
		printf("+ warning: failed to read all bytes from %s\n", name);
	}
	fclose(fp);

	// now convert bytes to samples
	samples = bytes / (bits/8);	// by bytes per sample
	samples /= chans;		        // by number of channels - NOW it's samples

    if ((bits != 8)&&(bits != 16)) {
        printf("Wave must be 8 or 16 (LE) bits only (got %d).\n", bits);
        free(tmpBuf);
        return ret;
    }
    if ((chans < 1)||(chans > 2)) {
        printf("Wave must be 1 or 2 channels only (got %d)\n", chans);
        free(tmpBuf);
        return ret;
    }

    // convert to a set of floating point samples for the FFT
    // resample to newFreq as we go
    // original code inserted 2 frames of silence, so we will too
    int silence = int(round((2/60.0)*newFreq));
    int newsamples = int(((double)samples/freq) * newFreq) + silence;
    double samplestep = (double)freq/newFreq;
    ret = (double*)malloc(sizeof(double)*newsamples);
    for (int idx=0; idx<silence; ++idx) {
        ret[idx] = 0;
    }

    if (debug2) {
        printf("Original rate: %d hz\n", freq);
        printf("Sample scale: %lf\n", samplestep);
    }

    double offset = 0;
    double loudest = 0;
    // resample with linear interpolation
    // also convert to mono float samples
    // only support mono or stereo, 8 or 16 bit
    for (int i=silence; i<newsamples; ++i) {
        unsigned char *p = tmpBuf + int(floor(offset)*chans*(bits/8));
        double sample = 0;
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
            sample = double(((s1/32768.0)*(1-ratio)) + ((s2/32768.0)*(ratio)));
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
            sample = double(((s1/128.0)*(1-ratio)) + ((s2/128.0)*(ratio)));
        }
        ret[i] = sample;
        offset+=samplestep;

        if (abs(sample) > loudest) loudest = abs(sample);
    }
    samples = newsamples;
    free(tmpBuf);

    // one more pass - amplify as loud as possible
    // we want to over-amplify a little, the clipping doesn't
    // seem to hurt the result too badly
    loudest = 1.2/loudest;      // instead of 1.0

    if (debug2) {
        printf("Input volume scale: %lf\n", loudest);
    }

    for (int idx=0; idx<samples; ++idx) {
        ret[idx] *= loudest;
    }

    if (debug2) {
        // write out the converted wave file to ensure it's correct
        // we'll write signed 16-bit for simplicity
        FILE *fp = fopen("samplesound.wav", "wb");
        if (NULL != fp) {
            fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x1\0\x1\0\x44\xac\0\0\x44\xac\0\0\x10\0\x10\0data\0\0\0\0",
                1,44,fp);
            for (int idx=0; idx<samples; ++idx) {
                short x = short(32767*ret[idx]);
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

	// and we're done
    samples = newsamples;
	return ret;
}

int main(int argc, char *argv[]) {
	printf("Parse voice to PSG - v20201125\n\n");

	if (argc < 2) {
		printf("voice2psg [-q] [-d] [-o <n>] [-add <n>] [-speedscale <n>] [-channels <n>] <filename>\n");
		printf(" -q - quieter verbose data\n");
        printf(" -d - enable parser debug output\n");
        printf(" -o <n> - output only channel <n> (1-max)\n");
        printf(" -add <n> - add 'n' to the output channel number (use for multiple chips, otherwise starts at zero)\n");
        printf(" -speedscale <float> - scale sample rate by float (1.0 = no change, 1.1=10%% faster, 0.9=10%% slower)\n");
        printf(" -volume <float> - scale volume by float (4.0 = default, 4.1=louder, 3.9=quieter)\n");
        printf(" -channels <n> - number of channels to create (1-6, default 3)\n");
        printf(" -maxfreq <n> - maximum frequency to search for (default 4000)\n");
        printf(" -mindiff <n> - minimum difference as a percentage between two accepted frequencies (default 5)\n");
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
        } else if (0 == strcmp(argv[arg], "-volume")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'volume' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%lf", &volumeScale)) {
                printf("Failed to parse volume\n");
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
            if ((channels < 1) || (channels > MAX_CHANNELS-1)) {
                printf("Invalid channel count (%d) - must be 1-%d\n", channels, MAX_CHANNELS-1);
                return -1;
            }
        } else if (0 == strcmp(argv[arg], "-maxfreq")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'maxfreq' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%d", &maxFreq)) {
                printf("Failed to parse maxfreq\n");
                return -1;
            }
            if ((maxFreq < 110) || (maxFreq > sampleRate/2)) {
                printf("Invalid max frequency - must be 110-%d\n", sampleRate/2);
                return -1;
            }
        } else if (0 == strcmp(argv[arg], "-mindiff")) {
            ++arg;
            if (arg+1 >= argc) {
                printf("Not enough arguments for 'mindiff' option\n");
                return -1;
            }
            if (1 != sscanf(argv[arg], "%d", &minStep)) {
                printf("Failed to parse mindiff\n");
                return -1;
            }
            if ((minStep < 0) || (minStep > 100)) {
                printf("Invalid minimum difference - must be 0-100 (as a percentage)\n");
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

    // read in a wave file, which is double samples at 44100 hz.
    double *sampledata = NULL;
    int samples = 0;
    sampledata = wavread(argv[arg], sampleRate, samples);
    if (sampledata == NULL) {
        printf("Failed to read wave file '%s'\n", argv[arg]);
        return -1;
    }

    // with a sample rate of 44100, a 60hz sample happens exactly every 735 samples,
    // so that is what we'll run the sample at.
    memset(outTone, 0, sizeof(outTone));
    memset(outVol, 0, sizeof(outVol));

    int rows = 0;
    int samplepos = 0;
    int noisefreq = 0;
    int noisevol = 0;
    int step735 = int(735*songSpeedScale+.5)*2;
    int clipped = 0;

    // set up Gist
    Gist<double> gist(step735, sampleRate);

    // process two 735 samples at a time - partial last frame isn't important
    // online notes suggest that you need 20-40ms for speech recognition and
    // accurate pitch estimation, so let's try the longer period (32ms) and
    // see if it helps...
    while (samplepos + step735 < samples) {
        // calculate average volume first, before we corrupt it
        double avgVol = 0;
        for (int idx=samplepos; idx<samplepos + step735; idx++) {
            avgVol += abs(sampledata[idx]);
        }
        avgVol /= step735;

        gist.processAudioFrame(&sampledata[samplepos], step735);

        // available:
        // gist.rootMeanSquare() - average energy of all
        // gist.peakEnergy() - loudest energy
        // gist.zeroCrossingRate() - like it sounds
        // gist.spectralCentroid() - center of mass "brightness"
        // gist.spectralCrest() - ratio of peaks to effective
        // gist.spectralFlatness() - how flat - for instance, noises are flat
        // gist.spectralRolloff() - how much the audio fades out towards higher frequencies
        // gist.spectralKurtosis() - detects outliers in the pattern... idk
        // gist.energyDifference() - 
        // gist.spectralDifference() -
        // gist.spectralDifferenceHWR() - half wave rectified
        // gist.complexSpectralDifference() -
        // gist.highFrequencyContent() -
        // gist.getMagnitudeSpectrum() - get a full FFT spectrum
        // gist.pitch() - pitch estimation
        // gist.getMelFrequencySpectrum() - a spectrum better suited to voice recognition
        // gist.getMelFrequencyCepstralCoefficients() - characterises a frame of speech - http://www.practicalcryptography.com/miscellaneous/machine-learning/tutorial-cepstrum-and-lpccs/

        // get the maximum spectrum
        double max[MAX_CHANNELS], index[MAX_CHANNELS];
        for (int idx=0; idx<MAX_CHANNELS; ++idx) {
            max[idx] = 0;
            index[idx] = 0;
        }
        std::vector<double> x = gist.getMagnitudeSpectrum();
        
        // only worry about the range from 110hz to maxfreq
        // to avoid wasting channels on outputs at the same rate,
        // we will compare against existing notes and if they
        // are the same, we won't double up. This still might
        // be an issue with similar notes, especially at the
        // higher frequencies. With the default settings, at
        // 110hz it's only 6 steps between notes, though at 
        // 4000hz it's about 230 steps.
        double freqstep = (double)(sampleRate/2) / x.size();
        int off110 = int(110.0/freqstep+.5);
        int offMax = int((double)maxFreq/freqstep+.5);
        for (int idx = off110; idx<=offMax; ++idx) {
            bool matched = false;

            // first, see if there is one that we already match
            // if so, just replace it
            // this doesn't really help much, but saving it cause probably doesn't hurt
            int snCode = int(111860.8 / (idx*freqstep) + .5);
            for (int chan = 0; chan < channels; ++chan) {
                if (index[chan] == 0) continue;
                int thisSn = int(111860.8 / (index[chan]*freqstep) + .5);
                if (thisSn == snCode) {
                    matched = true;
                    // we found it either way, replace only if it's louder
                    if (x[idx] > max[chan]) {
                        max[chan] = x[idx];
                        index[chan] = idx;  // will still be different by one or two
                    }
                    break;
                }
            }
            if (matched) continue;

            // find the first one we're louder than, and insert into the sorted list
            for (int chan = 0; chan < channels; ++chan) {
                if (x[idx] > max[chan]) {
                    // if we are going to take this one, then we want to check the
                    // minstep steps around it, and take the loudest of those, then skip
                    // over. We assume are at the first step of a peak, so it gets louder
                    // then quieter - hopefully inside the step. Step is linear, so even
                    // though it's not hz, this ratio should still work. The intent of
                    // the default was a step of 3 at 100hz, roughly 1/2 note.
                    int minstepcnt = int(idx*(minStep/100.0)+.5);
                    int result = idx;       // this is the one we are /actually/ going to take
                    if (minstepcnt > 0) {
                        for (int xx = idx; xx<idx+minstepcnt; ++xx) {
                            if (x[xx] > x[result]) {
                                result = xx;
                            }
                        }
                    }

                    // shift down to make room
                    for (int ch = channels-1; ch > chan; --ch) {
                        max[ch] = max[ch-1];
                        index[ch] = index[ch-1];
                    }
                    // insert here
                    max[chan] = x[result];
                    index[chan] = result;

                    // and skip ahead if desired, -1 for the loop
                    if (minstepcnt > 1) {
                        idx += minstepcnt - 1;
                    }
                    break;
                }
            }
        }

        noisevol = 0;

        // it seems to sort of be on the right track, but the
        // noise inputs are not helping at all like this.
        // this approach also means I ONLY get noise or tone,
        // never both. I wonder if using the tone randomness
        // might be a good clue for noise?
        // using flatness for silence seems good though...
        if (gist.spectralFlatness() >= 0.90) {
            // this is either silent or noise
            if (gist.rootMeanSquare() < 0.01) {
                for (int idx=0; idx<channels; ++idx) {
                    outVol[idx][rows] = 0;
                }
            } else {
//                noisevol = int(gist.rootMeanSquare()*255);
//                noisefreq = int(111860.8 / gist.pitch() + 0.5);
            }
        } else {
            for (int idx=0; idx<channels; ++idx) {
                if (index[idx] == 0) {
                    // no voice here
                    outVol[idx][rows] = 0;
                } else {
                    double freq = index[idx]*freqstep;
                    if (freq < 110) freq=110;
                    if (freq > maxFreq) freq=maxFreq;
                    outTone[idx][rows] = int(111860.8 / freq + 0.5);

#if 0
                    double vol = (max[idx]/(step735/volumeScale))*256;
#else
                    double vol = (max[idx]/max[0])*(avgVol*256);
#endif
                    if (vol > 255) vol=255;
                    outVol[idx][rows] = int(vol);
                    if (outVol[idx][rows] > 255) {
                        outVol[idx][rows]=255;
                        ++clipped;
                    }
                }
            }
        }

        // fill in noise level... voice is usually either-or
        // could use the flatness level?
        outTone[MAX_CHANNELS-1][rows] = noisefreq;
        outVol[MAX_CHANNELS-1][rows] = noisevol;
        if (outTone[MAX_CHANNELS-1][rows] > 0xfff) {
            outTone[MAX_CHANNELS-1][rows] = 0xfff;
        }

        ++rows;
        samplepos += step735/2; // step only 1 frame
    }

    if (clipped > 0) {
        printf("Warning: %d samples clipped - consider reducing volume\n", clipped);
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

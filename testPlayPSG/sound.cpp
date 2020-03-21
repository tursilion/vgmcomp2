//
// (C) 2020 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this code.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//
//****************************************************
// Tursi's own from-scratch TMS9919 emulation
// Because everyone uses the MAME source.
//
// Based on documentation on the SN76489 from SMSPOWER
// and datasheets for the SN79489/SN79486/SN79484,
// and recordings from an actual TI99 console (gasp!)
//

// This is a virtual sound chip version that supports 100 channels, be they noise or tone
// This lets my player play back any supported song
// To do this, I've broken things into more of a loop

#include "stdafx.h"
#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "sound.h"

// some Classic99 stuff
LPDIRECTSOUNDBUFFER soundbuf;		// sound chip audio buffer
LPDIRECTSOUND lpds;					// DirectSound handle
int hzRate = 60;            		// 50 or 60 fps (HZ50 or HZ60)
extern void WriteAudioFrame(void *pData, int nLen);
void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol);       // to reduce up/down clicks

// helper for audio buffers
CRITICAL_SECTION csAudioBuf;

// value to fade every clock/16
#define FADECLKTICK (0.001/9.0)

int nClock = 3579545;					// NTSC, PAL may be at 3546893? - this is divided by 16 to tick
struct CHAN {
    CHAN() {
        nCounter = 1;
        nOutput = 1.0;
        nNoisePos = 1;
        LFSR = 0x4000;
        nRegister = 0;
        nVolume = 0;
        nFade = 1.0;
        isNoise = false;
        isActive = false;
    }

    int nCounter;                       // 10 bit countdown timer
    double nOutput;                     // output scale
    int nNoisePos;                      // whether noise is positive or negative (white noise only)
    unsigned short LFSR;				// noise shifter (only 15 bit)
    int nRegister;      				// frequency registers
    int nVolume;         				// volume attenuation
    double nFade;               		// emulates the voltage drift back to 0 with FADECLKTICK (TODO: what does this mean with a non-zero center?)
										// we should test this against an external chip with a clean circuit.
    bool isNoise;                       // true if this is a noise channel
    bool isActive;                      // true if this channel is active
} voice[100];

// audio
int AudioSampleRate=22050;				// in hz
unsigned int CalculatedAudioBufferSize=22080*2;	// round audiosample rate up to a multiple of 60 (later hzRate is used)

// The tapped bits are XORd together for the white noise
// generator feedback.
int nTappedBits=0x0003;

// logarithmic scale (linear isn't right!)
// the SMS Power example, (I convert below to percentages)
int sms_volume_table[16]={
   32767, 26028, 20675, 16422, 13045, 10362,  8231,  6538,
    5193,  4125,  3277,  2603,  2067,  1642,  1304,     0
};
double nVolumeTable[16];

// return 1 or 0 depending on odd parity of set bits
// function by Dave aka finaldave. Input value should
// be no more than 16 bits.
int parity(int val) {
	val^=val>>8;
	val^=val>>4;
	val^=val>>2;
	val^=val>>1;
	return val&1;
};

// called from main to activate a channel
void registerChan(int channel, bool noise) {
    voice[channel].isNoise = noise;
    voice[channel].isActive = true;
}

// prepare the sound emulation. 

// called from sound_init
void GenerateToneBuffer() {
	UCHAR *ptr1, *ptr2;
	unsigned long len1, len2;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX pcmwf;

	EnterCriticalSection(&csAudioBuf);

	// if we already have one, get rid of it
	if (NULL != soundbuf) {
		rampVolume(soundbuf, DSBVOLUME_MIN);
		Sleep(1);
		soundbuf->Stop();
		soundbuf->Release();
		soundbuf=NULL;
	}

	// calculate new buffer size - 1 second of sample rate, rounded up to a multiple of hzRate (fps)
	CalculatedAudioBufferSize=AudioSampleRate;
	if (CalculatedAudioBufferSize%(hzRate) > 0) {
		CalculatedAudioBufferSize=((CalculatedAudioBufferSize/(hzRate))+1)*(hzRate);
	}
	CalculatedAudioBufferSize*=2;		// now upscale from samples to bytes

	// Here's the format of the audio buffer, 16 bit signed today
	ZeroMemory(&pcmwf, sizeof(pcmwf));
	pcmwf.wFormatTag = WAVE_FORMAT_PCM;		// wave file
	pcmwf.nChannels=1;						// 1 channel (mono)
	pcmwf.nSamplesPerSec=AudioSampleRate;	// 22khz
	pcmwf.nBlockAlign=2;					// 2 bytes per sample * 1 channel
	pcmwf.nAvgBytesPerSec=pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	pcmwf.wBitsPerSample=16;				// 16 bit samples
	pcmwf.cbSize=0;							// always zero (extra data size, not struct size)

	ZeroMemory(&dsbd, sizeof(dsbd));
	dsbd.dwSize=sizeof(dsbd);
	dsbd.dwFlags=DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
	dsbd.dwBufferBytes=CalculatedAudioBufferSize;	// the sample is CalculatedAudioBufferSize bytes long
	dsbd.lpwfxFormat=&pcmwf;
	dsbd.guid3DAlgorithm=GUID_NULL;

	if (FAILED(lpds->CreateSoundBuffer(&dsbd, &soundbuf, NULL)))
	{
		printf("Failed to create sound buffer");
		LeaveCriticalSection(&csAudioBuf);
		return;
	}
	
	if (SUCCEEDED(soundbuf->Lock(0, CalculatedAudioBufferSize, (void**)&ptr1, &len1, (void**)&ptr2, &len2, DSBLOCK_ENTIREBUFFER)))
	{
		// since we haven't started the sound, hopefully the second pointer is null
		if (len2 != 0) {
            // just a warning, we'll still clear what we can
			printf("Failed to lock entire tone buffer!\n");
		}

        // just make sure it's all zeroed out
		memset(ptr1, 0, len1);
		
		// and unlock
		soundbuf->Unlock(ptr1, len1, ptr2, len2);
	}

	// Set the max volume
	soundbuf->SetVolume(DSBVOLUME_MAX);

	if (FAILED(soundbuf->Play(0, 0, DSBPLAY_LOOPING))) {
		printf("Voice DID NOT START");
	}

	LeaveCriticalSection(&csAudioBuf);
}

// freq - output sound frequency in hz
bool sound_init(int freq) {
	InitializeCriticalSection(&csAudioBuf);

    HRESULT res;
	if (FAILED(res=DirectSoundCreate(NULL, &lpds, NULL)))
	{
        printf("Failed DirectSoundCreate, code 0x%08X\n", res);
		lpds=NULL;		// no sound
		return false;
	}
	
	if (FAILED(res=lpds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL)))	// normal created a 22khz, 8 bit stereo DirectSound system
	{
        printf("Failed SetCooperativeLevel, code 0x%08X\n", res);
		lpds->Release();
		lpds=NULL;
		return false;
	}

	// use the SMS power Logarithmic table
	for (int idx=0; idx<16; idx++) {
		// this magic number makes maximum volume (32767) become about 0.9375
		nVolumeTable[idx]=(double)(sms_volume_table[idx])/34949.3333;	
	}

    // and set up the audio rate
	AudioSampleRate=freq;

	GenerateToneBuffer();

    return true;
}

// change the frequency counter on a channel
// chan - channel 0-99
// freq - frequency counter (0-4095) (EVEN FOR NOISE, which may include flags, also note expanded range!)
// clip - true to restrict to the TI PSG range
void setfreq(int chan, int freq, bool clip) {
	if ((chan < 0)||(chan > 99)) return;

	if (voice[chan].isNoise) {
		// reset shift register on trigger
        if (freq&NOISE_TRIGGER) {
    		voice[chan].LFSR=0x4000;	//	(15 bit)
        }
	} 
    
	// limit freq
	freq&=NOISE_PERIODIC | NOISE_MASK;    // TI chip is 0x3ff!

    if (clip) {
        if (freq > 0x3ff) {
            freq = 0x3ff;
        }
    }
    
    voice[chan].nRegister=freq;
	// don't update the counters, let them run out on their own

    static bool warn = false;
    if (!warn) {
        if (freq > 0x3ff) {
            warn=true;
            printf("\nWarning: out of range bass for PSG\n");
        }
    }
}

// change the volume on a channel
// chan - channel 0-3
// vol - 0 (loudest) to 15 (silent)
void setvol(int chan, int vol) {
	if ((chan < 0)||(chan > 99)) return;
	voice[chan].nVolume=vol&0xf;
}

// fill the output audio buffer with signed 16-bit values
void sound_update(short *buf, int nSamples) {
	// nClock is the input clock frequency, which runs through a divide by 16 counter
	// The frequency does not divide exactly by 16
	// AudioSampleRate is the frequency at which we actually output bytes
	// multiplying values by 1000 to improve accuracy of the final division (so ms instead of sec)
	double nTimePerClock=1000.0/(nClock/16.0);
	double nTimePerSample=1000.0/(double)AudioSampleRate;
	int nClocksPerSample = (int)(nTimePerSample / nTimePerClock + 0.5);		// +0.5 to round up if needed
	int inSamples = nSamples;

	while (nSamples) {
		// emulate drift to zero
		for (int idx=0; idx<100; idx++) {
			if (voice[idx].nFade > 0.0) {
				voice[idx].nFade-=FADECLKTICK*nClocksPerSample;
				if (voice[idx].nFade < 0.0) voice[idx].nFade=0.0;
			} else {
				voice[idx].nFade=0.0;
			}
		}

		// process channels
        int chanCount = 0;
        double output = 0;

		for (int idx=0; idx<100; idx++) {
            if (!voice[idx].isActive) continue;
            if (!voice[idx].isNoise) {
			    voice[idx].nCounter-=nClocksPerSample;
			    while (voice[idx].nCounter <= 0) {
				    voice[idx].nCounter+=(voice[idx].nRegister?voice[idx].nRegister:0x400);
				    voice[idx].nOutput*=-1.0;
				    voice[idx].nFade=1.0;
			    }
			    // A little check to eliminate high frequency tones
			    if ((voice[idx].nRegister != 0) && (voice[idx].nRegister <= (int)(111860.0/(double)(AudioSampleRate/2)))) {
				    voice[idx].nFade=0.0;
			    }
		    } else {
		        // noise channel 
		        voice[idx].nCounter-=nClocksPerSample;
		        while (voice[idx].nCounter <= 0) {
                    int regCnt = voice[idx].nRegister & NOISE_MASK;
				    voice[idx].nCounter+=(regCnt?regCnt:0x400);
			        voice[idx].nNoisePos*=-1;
			        double nOldOut=voice[idx].nOutput;
			        // Shift register is only kicked when the 
			        // Noise output sign goes from negative to positive
			        if (voice[idx].nNoisePos > 0) {
				        int in=0;
				        if (!(voice[idx].nRegister&NOISE_PERIODIC)) {
					        // white noise - actual tapped bits uncertain?
					        if (parity(voice[idx].LFSR&nTappedBits)) {
                                in=0x4000;
                            }

					        if (voice[idx].LFSR&0x0001) {
						        if (voice[idx].nOutput > 0.0) {
							        voice[idx].nOutput = -1.0;
						        } else {
							        voice[idx].nOutput = 1.0;
						        }
					        }
				        } else {
					        // periodic noise - tap bit 0 (again, BBC Micro)
					        if (voice[idx].LFSR&0x0001) {
						        in=0x4000;	// (15 bit shift)
						        voice[idx].nOutput=1.0;
					        } else {
						        voice[idx].nOutput=0.0;
					        }
				        }
				        voice[idx].LFSR>>=1;
				        voice[idx].LFSR|=in;
			        }
			        if (nOldOut != voice[idx].nOutput) {
				        voice[idx].nFade=1.0;
			        }
		        }
            }
            output += voice[idx].nOutput*nVolumeTable[voice[idx].nVolume]*voice[idx].nFade;
            ++chanCount;
        }

		// write sample
		nSamples--;

		// using division (single voices are quiet!)
		// write out one sample
		// output is now between 0.0 and chanCount, may be positive or negative
		output/=(double)chanCount;	// you aren't supposed to do this when mixing. Sorry. :)

		short nSample=(short)((double)0x7fff*output); 
		*(buf++)=nSample; 
	}
}

void MuteAudio() {
	// set overall volume to muted (this is not a sound chip effect, it's directly related to DirectSound)
	// it sets the maximum volume of all channels
	EnterCriticalSection(&csAudioBuf);

	if (NULL != soundbuf) {
		rampVolume(soundbuf, DSBVOLUME_MIN);
	}

	LeaveCriticalSection(&csAudioBuf);
}

void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol) {
    // want to finish in this many steps, as few as possible
    // There is probably no harm to this, but it does NOT affect the startup click...
    // It makes some impact on reset (but the click on finishing reset still happens)
    // and seems to resolve the shutdown click. It does slow things a bit.
    // The main click is caused by the DAC, so, I've got to do a little extra ramp
    // when the system first starts up just for that. That one we'll do live.
    // TODO: still not convinced of this, causes a flutter when it fades out.
    // Also causes a delay in processing, times every channel involved...
    const long step = (DSBVOLUME_MAX-DSBVOLUME_MIN);    // todo: normally divide this by number of steps, with it set to one step it's doing pretty much nothing
    long vol = newVol;
    
    if (NULL == ds) return;
    if (newVol > DSBVOLUME_MAX) newVol = DSBVOLUME_MAX;
    if (newVol < DSBVOLUME_MIN) newVol = DSBVOLUME_MIN;

    ds->GetVolume(&vol);

    // only one of these loops should run
    while (vol > newVol) {
        vol -= step;
        if (vol < newVol) vol=newVol;
        ds->SetVolume(vol);
        Sleep(10);
    }
    while (vol < newVol) {
        vol += step;
        if (vol > newVol) vol=newVol;
        ds->SetVolume(vol);
        Sleep(10);
    }
}

void UpdateSoundBuf(LPDIRECTSOUNDBUFFER soundbuf, void (*sound_update)(short *,int), StreamData *pDat) {
	DWORD iRead, iWrite;
	short *ptr1, *ptr2;
	DWORD len1, len2;

	EnterCriticalSection(&csAudioBuf);

	// DirectSound iWrite pointer just points a 'safe distance' ahead of iRead, usually about 15ms
	// so we need to maintain our own count of where we are writing
	soundbuf->GetCurrentPosition(&iRead, &iWrite);
	if (pDat->nLastWrite == 0xffffffff) {
		pDat->nLastWrite=iWrite;
	}
	
	// arbitrary - try to use a dynamic jitter buffer
	int nWriteAhead;
	if (pDat->nLastWrite<iRead) {
		nWriteAhead=pDat->nLastWrite+CalculatedAudioBufferSize-iRead;
	} else {
		nWriteAhead=pDat->nLastWrite-iRead;
	}
	nWriteAhead/=CalculatedAudioBufferSize/(hzRate);
	
	if (nWriteAhead > 29) {
		// this more likely means we actually fell behind!
		if (pDat->nMinJitterFrames < 10) {
			pDat->nMinJitterFrames++;
		} 
		if (pDat->nJitterFrames < pDat->nMinJitterFrames) {
			pDat->nJitterFrames=pDat->nMinJitterFrames;
		}
		nWriteAhead=0;
		pDat->nLastWrite=iWrite;
	}

	// update jitter buffer if we fall behind, but no more than 15 frames (that would only be 4 updates a second!)
	if ((nWriteAhead < 1) && (pDat->nJitterFrames < 15)) {
		pDat->nJitterFrames++;
	} else if ((nWriteAhead > pDat->nJitterFrames+1) && (pDat->nJitterFrames > pDat->nMinJitterFrames)) {
		// maybe we can shrink the buffer?
		pDat->nJitterFrames--;
	} else if ((nWriteAhead > pDat->nMinJitterFrames/2+1) && (pDat->nMinJitterFrames > 2)) {
		pDat->nMinJitterFrames--;
	}

	// doing it all right here limits the CPU's ability to interact
	// but luckily we should NORMALLY only do one frame at a time
	// as noted, the goal is to get it on a per-scanline basis
	while (nWriteAhead < pDat->nJitterFrames) {
		if (SUCCEEDED(soundbuf->Lock(pDat->nLastWrite, CalculatedAudioBufferSize/(hzRate), (void**)&ptr1, &len1, (void**)&ptr2, &len2, 0))) {
			if (len1 > 0) {
				sound_update(ptr1, len1/2);		// divide by 2 for 16 bit samples
			}
			if (len2 > 0) {
				sound_update(ptr2, len2/2);		// divide by 2 for 16 bit samples
			}

			// carry on
			soundbuf->Unlock(ptr1, len1, ptr2, len2);

			// update write pointer
			pDat->nLastWrite += CalculatedAudioBufferSize/(hzRate);
			if (pDat->nLastWrite >= CalculatedAudioBufferSize) {
				pDat->nLastWrite-=CalculatedAudioBufferSize;
			}
		} else {
			printf("Failed to lock sound buffer!");
			break;
		}
		nWriteAhead++;
    }

	LeaveCriticalSection(&csAudioBuf);
}

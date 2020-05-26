// This is NOT the same one in testPlayer - this is by and large
// the sound emulation from Classic99, so register writes work
// as they would be expected to.

//
// (C) 2009 Mike Brent aka Tursi aka HarmlessLion.com
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

#include <windows.h>
#include <dsound.h>
#include <stdio.h>
#include "sound.h"

// some Classic99 stuff
int hzRate = 60;		// 50 or 60 fps (HZ50 or HZ60)
LPDIRECTSOUNDBUFFER soundbuf;		// sound chip audio buffer
LPDIRECTSOUND lpds;					// DirectSound handle
void rampVolume(LPDIRECTSOUNDBUFFER ds, long newVol);       // to reduce up/down clicks

// helper for audio buffers
CRITICAL_SECTION csAudioBuf;

// value to fade every clock/16
// this value compares with the recordings I made of the noise generator back in the day
// This is good, but why doesn't this match the math above, though?
#define FADECLKTICK (0.001/9.0)

int nClock = 3579545;					// NTSC, PAL may be at 3546893? - this is divided by 16 to tick
int nCounter[4]= {0,0,0,1};				// 10 bit countdown timer
double nOutput[4]={1.0,1.0,1.0,1.0};	// output scale
int nNoisePos=1;						// whether noise is positive or negative (white noise only)
unsigned short LFSR=0x4000;				// noise shifter (only 15 bit)
int nRegister[4]={0,0,0,0};				// frequency registers
int nVolume[4]={0,0,0,0};				// volume attenuation
double nFade[4]={1.0,1.0,1.0,1.0};		// emulates the voltage drift back to 0 with FADECLKTICK
										// we should test this against an external chip with a clean circuit.
int max_volume;

// audio
int AudioSampleRate=22050;				// in hz
unsigned int CalculatedAudioBufferSize=22080*2;	// round audiosample rate up to a multiple of 60 (later hzRate is used)

// The tapped bits are XORd together for the white noise
// generator feedback.
// These are the BBC micro version/Coleco? Need to check
// against a real TI noise generator. 
// This matches what MAME uses (although MAME shifts the other way ;) )
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

// prepare the sound emulation. 
// freq - output sound frequency in hz
void sound_init(int freq) {
    HRESULT res;
	
    InitializeCriticalSection(&csAudioBuf);

    if (FAILED(res=DirectSoundCreate(NULL, &lpds, NULL)))
	{
        printf("Failed DirectSoundCreate, code 0x%08X\n", res);
		lpds=NULL;		// no sound
		return;
	}
	
	if (FAILED(res=lpds->SetCooperativeLevel(GetDesktopWindow(), DSSCL_NORMAL)))	// normal created a 22khz, 8 bit stereo DirectSound system
	{
        printf("Failed SetCooperativeLevel, code 0x%08X\n", res);
		lpds->Release();
		lpds=NULL;
		return;
	}

    // use the SMS power Logarithmic table
	for (int idx=0; idx<16; idx++) {
		// this magic number makes maximum volume (32767) become about 0.9375
		nVolumeTable[idx]=(double)(sms_volume_table[idx])/34949.3333;	
	}

    // and set up the audio rate
	AudioSampleRate=freq;

	GenerateToneBuffer();
}

// change the frequency counter on a channel
// chan - channel 0-3 (3 is noise)
// freq - frequency counter (0-1023) or noise code (0-7)
void setfreq(int chan, int freq) {
	if ((chan < 0)||(chan > 3)) return;

	if (chan==3) {
		// limit noise 
		freq&=0x07;
		nRegister[3]=freq;

		// reset shift register
		LFSR=0x4000;	//	(15 bit)
		switch (nRegister[3]&0x03) {
			// these values work but check the datasheet dividers
			case 0: nCounter[3]=0x10; break;
			case 1: nCounter[3]=0x20; break;
			case 2: nCounter[3]=0x40; break;
			// even when the count is zero, the noise shift still counts
			// down, so counting down from 0 is the same as wrapping up to 0x400
			case 3: nCounter[3]=(nRegister[2]?nRegister[2]:0x400); break;		// is never zero!
		}
	} else {
		// limit freq
		freq&=0x3ff;
		nRegister[chan]=freq;
		// don't update the counters, let them run out on their own
	}
}

// change the volume on a channel
// chan - channel 0-3
// vol - 0 (loudest) to 15 (silent)
void setvol(int chan, int vol) {
	if ((chan < 0)||(chan > 3)) return;

	nVolume[chan]=vol&0xf;
}

// fill the output audio buffer with signed 16-bit values
// nAudioIn contains a fixed value to add to all samples (used to mix in the casette audio)
// (this emu doesn't run speech through there, though, speech gets its own buffer for now)
void sound_update(short *buf, double nAudioIn, int nSamples) {
	// nClock is the input clock frequency, which runs through a divide by 16 counter
	// The frequency does not divide exactly by 16
	// AudioSampleRate is the frequency at which we actually output bytes
	// multiplying values by 1000 to improve accuracy of the final division (so ms instead of sec)
	double nTimePerClock=1000.0/(nClock/16.0);
	double nTimePerSample=1000.0/(double)AudioSampleRate;
	int nClocksPerSample = (int)(nTimePerSample / nTimePerClock + 0.5);		// +0.5 to round up if needed
	int newdacpos = 0;
	int inSamples = nSamples;

	while (nSamples) {
		// emulate drift to zero
		for (int idx=0; idx<4; idx++) {
			if (nFade[idx] > 0.0) {
				nFade[idx]-=FADECLKTICK*nClocksPerSample;
				if (nFade[idx] < 0.0) nFade[idx]=0.0;
			} else {
				nFade[idx]=0.0;
			}
		}

		// tone channels

		for (int idx=0; idx<3; idx++) {
            // Further Testing with the chip that SMS Power's doc covers (SN76489)
            // 0 outputs a 1024 count tone, just like the TI, but 1 DOES output a flat line.
            // On the TI (SN76494, I think), 1 outputs the highest pitch (count of 1)
            // However, my 99/4 pics show THAT machine with an SN76489! 
            // My plank TI has an SN94624 (early name? TMS9919 -> SN94624 -> SN76494 -> SN76489)
            // And my 2.2 QI console has an SN76494!
            // So maybe we can't say with certainty which chip is in which machine?
            // Myths and legends:
            // - SN76489 grows volume from 2.5v down to 0 (matches my old scopes of the 494), but SN76489A grows volume from 0 up.
            // - SN76496 is the same as the SN7689A but adds the Audio In pin (all TI used chips have this, even the older ones)
            // So right now, I believe there are two main versions, differing largely by the behaviour of count 0x001:
            // Original (high frequency): TMS9919, SN94624, SN76494?
            // New (flat line): SN76489, SN76489A, SN76496
			nCounter[idx]-=nClocksPerSample;
			while (nCounter[idx] <= 0) {    // should be able to do this without a loop, it would be faster (well, in the rare cases it needs to loop)!
				nCounter[idx]+=(nRegister[idx]?nRegister[idx]:0x400);
				nOutput[idx]*=-1.0;
				nFade[idx]=1.0;
			}
			// A little check to eliminate high frequency tones
			// If the frequency is greater than 1/2 the sample rate,
			// then mute it (we'll do that with the nFade value.) 
			// Noises can't get higher than audible frequencies (even with high user defined rates),
			// so we don't need to worry about them.
			if ((nRegister[idx] != 0) && (nRegister[idx] <= (int)(111860.0/(double)(AudioSampleRate/2)))) {
				// this would be too high a frequency, so we'll merge it into the DAC channel (elsewhere)
				// and kill this channel. The reason is that the high frequency ends up
				// creating artifacts with the lower frequency output rate, and you don't
				// get an inaudible tone but awful noise
				nFade[idx]=0.0;
				//nAudioIn += nVolumeTable[nVolume[idx]];	// not strictly right, the high frequency output adds some distortion. But close enough.
				//if (nAudioIn >= 1.0) nAudioIn = 1.0;	// clip
			}
		}

		// noise channel 
		nCounter[3]-=nClocksPerSample;
		while (nCounter[3] <= 0) {
			switch (nRegister[3]&0x03) {
				case 0: nCounter[3]+=0x10; break;
				case 1: nCounter[3]+=0x20; break;
				case 2: nCounter[3]+=0x40; break;
				// even when the count is zero, the noise shift still counts
				// down, so counting down from 0 is the same as wrapping up to 0x400
				// same is with the tone above :)
				case 3: nCounter[3]+=(nRegister[2]?nRegister[2]:0x400); break;		// is never zero!
			}
			nNoisePos*=-1;
			double nOldOut=nOutput[3];
			// Shift register is only kicked when the 
			// Noise output sign goes from negative to positive
			if (nNoisePos > 0) {
				int in=0;
				if (nRegister[3]&0x4) {
					// white noise - actual tapped bits uncertain?
					// This doesn't currently look right.. need to
					// sample a full sequence of TI white noise at
					// a known rate and study the pattern.
					if (parity(LFSR&nTappedBits)) in=0x4000;
					if (LFSR&0x01) {
						// the SMSPower documentation says it never goes negative,
						// but (my very old) recordings say white noise does goes negative,
						// and periodic noise doesn't. Need to sit down and record these
						// noises properly and see what they really do. 
						// For now I am going to swing negative to play nicely with
						// the tone channels. 
						// TODO: I need to verify noise vs tone on a clean system.
						// need to test for 0 because periodic noise sets it
						if (nOutput[3] == 0.0) {
							nOutput[3] = 1.0;
						} else {
							nOutput[3]*=-1.0;
						}
					}
				} else {
					// periodic noise - tap bit 0 (again, BBC Micro)
					// Compared against TI samples, this looks right
					if (LFSR&0x0001) {
						in=0x4000;	// (15 bit shift)
						// TODO: verify periodic noise as well as white noise
						// always positive
						nOutput[3]=1.0;
					} else {
						nOutput[3]=0.0;
					}
				}
				LFSR>>=1;
				LFSR|=in;
			}
			if (nOldOut != nOutput[3]) {
				nFade[3]=1.0;
			}
		}

		// write sample
		nSamples--;
		double output;

		// using division (single voices are quiet!)
		// write out one sample
		output = nOutput[0]*nVolumeTable[nVolume[0]]*nFade[0] +
				nOutput[1]*nVolumeTable[nVolume[1]]*nFade[1] +
				nOutput[2]*nVolumeTable[nVolume[2]]*nFade[2] +
				nOutput[3]*nVolumeTable[nVolume[3]]*nFade[3];
		// output is now between 0.0 and 4.0, may be positive or negative
		output/=4.0;	// you aren't supposed to do this when mixing. Sorry. :)

		short nSample=(short)((double)0x7fff*output); 
		*(buf++)=nSample; 
	}
}

void SetSoundVolumes() {
	// set overall volume (this is not a sound chip effect, it's directly related to DirectSound)
	// it sets the maximum volume of all channels
	EnterCriticalSection(&csAudioBuf);

	int nRange=(MIN_VOLUME*max_volume)/100;	// negative
	if (NULL != soundbuf) {
		rampVolume(soundbuf, MIN_VOLUME - nRange);
	}

	LeaveCriticalSection(&csAudioBuf);
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
    // Also causes a delay in processing, times every channel involved...
    const long step = (DSBVOLUME_MAX-DSBVOLUME_MIN);    // normally divide this by number of steps, with it set to one step it's doing pretty much nothing
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

void UpdateSoundBuf(LPDIRECTSOUNDBUFFER soundbuf, void (*sound_update)(short *,double,int), StreamData *pDat) {
	DWORD iRead, iWrite;
	short *ptr1, *ptr2;
	DWORD len1, len2;
	static char *pRecordBuffer = NULL;
	static int nRecordBufferSize = 0;

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
				sound_update(ptr1, 0, len1/2);		// divide by 2 for 16 bit samples
			}
			if (len2 > 0) {
				sound_update(ptr2, 0, len2/2);		// divide by 2 for 16 bit samples
			}

			// carry on
			soundbuf->Unlock(ptr1, len1, ptr2, len2);

			// update write pointer
			pDat->nLastWrite += CalculatedAudioBufferSize/(hzRate);
			if (pDat->nLastWrite >= CalculatedAudioBufferSize) {
				pDat->nLastWrite-=CalculatedAudioBufferSize;
			}
		} else {
			break;
		}
		nWriteAhead++;
    }

	LeaveCriticalSection(&csAudioBuf);
}


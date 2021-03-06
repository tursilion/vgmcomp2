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

typedef unsigned __int8 UINT8;
typedef unsigned __int8 Byte;
typedef unsigned __int16 Word;
typedef unsigned __int32 DWord;

// must be higher than DSBVOLUME_MIN!
#define MIN_VOLUME -2000

#define NOISE_MASK     0x00FFF
#define NOISE_TRIGGER  0x10000
#define NOISE_PERIODIC 0x20000

struct StreamData {
	StreamData() {
		nLastWrite=0xffffffff;
		nJitterFrames=4;		// jitter buffer (increments on first entry, then if we fall behind!)
		nMinJitterFrames=4;		// minimum jitter buffer, increases only if we fall behind!
	};

	DWORD nLastWrite;
	int nJitterFrames;			// jitter buffer (increments on first entry, then if we fall behind!)
	int nMinJitterFrames;		// minimum jitter buffer, increases only if we fall behind!
};

extern CRITICAL_SECTION csAudioBuf;

bool sound_init(int freq);
void setfreq(int chan, int freq, bool clip);
void setvol(int chan, int vol);
void sound_update(short *buf, int nSamples);
void MuteAudio();
void UpdateSoundBuf(LPDIRECTSOUNDBUFFER soundbuf, void (*sound_update)(short *,int), StreamData *pDat);

void registerChan(int chan, bool noise);

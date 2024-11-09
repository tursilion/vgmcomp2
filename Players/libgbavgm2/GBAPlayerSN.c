// Wrapper for CPlayer.c that gives us the SN functions
// this directly includes CPlayer.c

// Okay, this is working on hardware (though Journey to Silius is not - bad convert?)
// Looks like CPU load is on the order of a couple of scanlines, though I'm not sure I
// am reading the output correctly - need to get a bit more working then I can just
// print the number of scanlines it takes. 

// This also includes a complete SN emulator adapted from Classic99,
// because the GBC hardware isn't really up to the task (retriggers
// needed for volume control corrupted the audio pretty badly).

// include GBA defines
#include <string.h>
#include "tursigb.h"

void setvol(int chan, int vol);

// We are pretty free on buffersize, but we probably want 1/60th second or less
// the GSM player in Cool Herders used 160 byte blocks (GSM requirement) and 11khz
#define BUFFERSIZE 128
#define BANK_COUNT 4
signed char AudioBuf[BUFFERSIZE*BANK_COUNT] __attribute__ ((__aligned__(32)));    
static int nBank=0;                    // Bank number for snupdateaudio() (start offset so bank 0 is filled first)

// SN emulation
int nCounter[4]= {0,0,0,1};				// 10 bit countdown timer
int nNoisePos=1;						// whether noise is positive or negative (white noise only)
unsigned short LFSR=0x4000;			    // noise shifter (only 15 bit)
int nRegister[4]={0,0,0,0};				// frequency registers
int nVolume[4]={0,0,0,0};				// volume attenuation
int nOutput[4]={1,1,1,1};       	    // output scale (positive or negative or zero)
//int nTappedBits=0x0003;               // not using this mask, checking 2 bits directly
int nVolumeTable[16]={
   32767, 26028, 20675, 16422, 13045, 10362,  8231,  6538,
    5193,  4125,  3277,  2603,  2067,  1642,  1304,     0
};

// sn chip sim init - make sure to call this early
// This enables DirectSound A using Timer 1 for frequency and Timer 2 for refill
// Warning: overwrites REG_SOUNDCNT_X, REG_SOUNDCNT_L and REG_SOUNDCNT_H completely
void gbasninit() {
    REG_SOUNDCNT_X = 0x0080;    // enable sound registers

    // Empty the buffer
    memset(AudioBuf, 0, sizeof(AudioBuf));
        
    // make sure Timers are off
    REG_TM1CNT = 0;
    REG_TM2CNT = 0;
    // make sure DMA channel 1 is turned off
    REG_DMA1CNT_H = 0;

    nBank=0;          // Bank number for snupdateaudio()
    setvol(0,15);
    setvol(1,15);
    setvol(2,15);
    setvol(3,15);   // mute the simulated generators

    // enable and reset Direct Sound channel A, at full volume, using
    // Timer 0 to determine frequency
    // Player thus uses DSA, DMA1, TM1 and TM2. It sets interrupts on TM2 for empty buffer
    // Note DSB is automatically set here for TM0, but needs to be enabled (50% by default)
    REG_SOUNDCNT_L = 0;     // turn off GBC output
    REG_SOUNDCNT_H = DSA_OUTPUT_RATIO_100 | // 100% direct sound A output
                     DSA_OUTPUT_TO_BOTH |   // output Direct Sound A to both right and left speakers
                     DSA_TIMER1 |           // use timer 1 to determine the playback frequency of Direct Sound A
                     DSA_FIFO_RESET;        // reset the FIFO for Direct Sound A

    // Default to 11khz on timer 1
    SetAudioFreq(FREQUENCY_11);
    // timer 2 will cascade on timer 1 and enable it to trigger every time we finish a buffer
    REG_TM2D   = (0xffff - (BUFFERSIZE - 1));   // (-1 cause it triggers on overflow, not at 0xffff)
    // init the DMA transfer
    REG_DMA1SAD = (u32)AudioBuf;
    REG_DMA1DAD = (u32)REG_FIFO_A;
    
    // Enable both timers, timer 2 first
    // Timer 2 throws an interrupt to indicate it's time to
    // refill the buffer.
    REG_TM2CNT = TIMER_CASCADE|TIMER_ENABLED|TIMER_INTERRUPT;
    REG_TM1CNT = TIMER_ENABLED;

    // go ahead and start loading audio (WORD_DMA, COUNT and DEST_REG_SAME are apparently ignored)
    REG_DMA1CNT_H = ENABLE_DMA | START_ON_FIFO_EMPTY | WORD_DMA | DMA_REPEAT | DEST_REG_SAME;
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
			case 3: nCounter[3]=(nRegister[2]?nRegister[2]:0x400); 
					break;		// is never zero!
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

// sn chip simulation - write data
void snsim(unsigned char c) {
    static unsigned short latch_byte = 0;
	static unsigned short oldFreq[3]={0,0,0};   // tone generator frequencies

	// 'c' contains the byte currently being written to the sound chip
	// all functions are 1 or 2 bytes long, as follows					
	//
	// BYTE		BIT		PURPOSE											
	//	1		0		always '1' (latch bit)							
	//			1-3		Operation:	000 - tone 1 frequency				
	//								001 - tone 1 volume					
	//								010 - tone 2 frequency				
	//								011 - tone 2 volume					
	//								100 - tone 3 frequency				
	//								101 - tone 3 volume					
	//								110 - noise control					
	//								111 - noise volume					
	//			4-7		Least sig. frequency bits for tone, or volume	
	//					setting (0-F), or type of noise.				
	//					(volume F is off)								
	//					Noise set:	4 - always 0						
	//								5 - 0=periodic noise, 1=white noise 
	//								6-7 - shift rate from table, or 11	
	//									to get rate from voice 3.		
	//	2		0-1		Always '0'. This byte only used for frequency	
	//			2-7		Most sig. frequency bits						
	//
	// Commands are instantaneous

	// Latch anytime the high bit is set
	// This byte still immediately changes the channel
	if (c&0x80) {
		latch_byte=c;
	}

	switch (c&0xf0)										// check command
	{	
	case 0x90:											// Voice 1 vol
	case 0xb0:											// Voice 2 vol
	case 0xd0:											// Voice 3 vol
	case 0xf0:											// Noise volume
		setvol((c&0x60)>>5, c&0x0f);
		break;

	case 0xe0:
		setfreq(3, c&0x07);                             // Noise - get type
		break;

	case 0x80:											// Voice 1 frequency
	case 0xa0:											// Voice 2 frequency
	case 0xc0:											// Voice 3 frequency
        {
            int nChan=(latch_byte&0x60)>>5;     // was just set above
            oldFreq[nChan]&=0xfff0;
            oldFreq[nChan]|=c&0x0f;
            setfreq(nChan, oldFreq[nChan]);
        }
		break;
        
	default:											// Any other byte (ie: 0x80 not set)
        {
            int nChan=(latch_byte&0x60)>>5;
            // latch clear - data to whatever is latched
            // TODO: re-verify this on hardware, it doesn't agree with the SMS Power doc
            // as far as the volume and noise goes!
            if (latch_byte&0x10) {
                // volume register
                setvol(nChan, c&0x0f);
            } else if (nChan==3) {
                // noise register
                setfreq(3, c&0x07);
            } else {
                // tone generator - most significant bits
                oldFreq[nChan]&=0xf;
                oldFreq[nChan]|=(c&0x3f)<<4;
                setfreq(nChan, oldFreq[nChan]);
            }
        }
		break;
	}
}

#if 0
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
#endif

// call this when it's time to fill the next audio buffer
// we need 8 bit signed samples - note if not in iwram,
// then in certain low pitched noise samples this can notably slow down
// If we still have problems, we could use lookup tables instead of loops, ROM is not an issue.
void CODE_IN_IWRAM snupdateaudio() {
    // swap output banks
	nBank += BUFFERSIZE;
	if (nBank >= BUFFERSIZE*BANK_COUNT) {
        nBank=0;
    } else if (nBank == BUFFERSIZE) {  // play pointer should be 1 bank behind us
		// You /must/ reset this to 0 before loading the count
        REG_DMA1CNT_H = 0;
        // Not necessary to rewrite this pointer so long as it hasn't been changed!
        //REG_DMA1SAD = (u32)AudioBuf;    // reset to beginning of buffer
        REG_DMA1CNT_H = ENABLE_DMA | START_ON_FIFO_EMPTY | WORD_DMA | DMA_REPEAT | DEST_REG_SAME;
    }

    signed char *buf = &AudioBuf[nBank];

	// nClock is the input clock frequency, which runs through a divide by 16 counter
	// The frequency does not divide exactly by 16
	// AudioSampleRate is the frequency at which we actually output bytes
	// multiplying values by 1000 to improve accuracy of the final division (so ms instead of sec)
	//const int AudioSampleRate = 11000;
    //int nClock = 3579545;					// NTSC, PAL may be at 3546893? - this is divided by 16 to tick
	//double nTimePerClock=1000.0/(nClock/16.0);
	//double nTimePerSample=1000.0/(double)AudioSampleRate;
	//int nClocksPerSample = (int)(nTimePerSample / nTimePerClock + 0.5);		// +0.5 to round up if needed
	int nClocksPerSample = 20;  // at 11000Hz sample rate (20.33832386)
	int nSamples = BUFFERSIZE;

	while (nSamples) {
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
			while (nCounter[idx] <= 0) {    
                // I did a test with division, and it was worse!
				nCounter[idx]+=(nRegister[idx]?nRegister[idx]:0x400);
				nOutput[idx]*=-1;
			}
			// A little check to eliminate high frequency tones
			// If the frequency is greater than 1/2 the sample rate,
			// then mute it (have to use the nOutput)
			// Noises can't get higher than audible frequencies (even with high user defined rates),
			// so we don't need to worry about them.
			if ((nRegister[idx] != 0) && (nRegister[idx] <= nClocksPerSample)) {    //(111860/(AudioSampleRate/2)))) {
                // not supporting DAC in this player, so just mute this one
				// The reason is that the high frequency ends up
				// creating artifacts with the lower frequency output rate, and you don't
				// get an inaudible tone but awful noise
				nOutput[idx]=0;
			} else if (nOutput[idx] == 0) {
                // undo a previous mute
                nOutput[idx] = 1;
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
			// Shift register is only kicked when the 
			// Noise output sign goes from negative to positive
			if (nNoisePos > 0) {
				int in=0;
				if (nRegister[3]&0x4) {
					// white noise - actual tapped bits uncertain?
					// This doesn't currently look right.. need to
					// sample a full sequence of TI white noise at
					// a known rate and study the pattern.

                    // tapped bits = 0x0003 - if odd parity (ie: one or the other set), OR in 0x4000
                    if (((LFSR&0x0002)>>1)^(LFSR&0x0001)) in = 0x4000;
					//if (parity(LFSR&nTappedBits)) in=0x4000;
					if (LFSR&0x01) {
						// the SMSPower documentation says it never goes negative,
						// but (my very old) recordings say white noise does goes negative,
						// and periodic noise doesn't. Need to sit down and record these
						// noises properly and see what they really do. 
						// For now I am going to swing negative to play nicely with
						// the tone channels. 
						// TODO: I need to verify noise vs tone on a clean system.
						// need to test for 0 because periodic noise sets it
						if (nOutput[3] == 0) {
							nOutput[3] = 1;
						} else {
							nOutput[3]*=-1;
						}
					}
				} else {
					// periodic noise - tap bit 0 (again, BBC Micro)
					// Compared against TI samples, this looks right
					if (LFSR&0x0001) {
						in=0x4000;	// (15 bit shift)
						// TODO: verify periodic noise as well as white noise
						// always positive
						nOutput[3]=1;
					} else {
						nOutput[3]=0;
					}
                }
				LFSR>>=1;
				LFSR|=in;
			}
		}

		// write sample
		nSamples--;
		
		// with just 4 channels, I can shift instead of divide
		// write out one sample. Note volume table is 16 bit
		int output;
		output = nOutput[0]*nVolumeTable[nVolume[0]] +
			 	 nOutput[1]*nVolumeTable[nVolume[1]] +
			 	 nOutput[2]*nVolumeTable[nVolume[2]] +
			 	 nOutput[3]*nVolumeTable[nVolume[3]];
		// output is now between 0 and 131068, may be positive or negative
		// shift 2 bits to divide by 4, then 8 bits to make 8-bit range
		output>>=2+8;    // you aren't supposed to do this when mixing. Sorry. :)
		*(buf++)=output; 
	}
}

// no prefix for SN player
//#define SONG_PREFIX sfx_

// build for SN
//#define USE_SN_PSG

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    if (((mutes)&(0x80>>((chan)-1)))==0) snsim(x);

// and now include the actual implementation
#include "CPlayer.c"

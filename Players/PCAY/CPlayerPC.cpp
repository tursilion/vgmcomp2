// helper code to let the PC write to the sound chip
// this is included directly in CPlayer.c

// now our sound chip access...
#ifdef USE_SN_PSG
#include "sound.h"

// writes a byte to the sound chip, Classic99 wsndbyte
void writeSound(int c) {
	unsigned int x;	        							// temp variable
	static int oldFreq[3]={0,0,0};						// tone generator frequencies
    static int latch_byte = 0;

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
		x=(c&0x07);										// Noise - get type
		setfreq(3, c&0x07);
		break;

//	case 0x80:											// Voice 1 frequency
//	case 0xa0:											// Voice 2 frequency
//	case 0xc0:											// Voice 3 frequency
	default:											// Any other byte
		int nChan=(latch_byte&0x60)>>5;
		if (c&0x80) {
			// latch write - least significant bits of a tone register
			// (definately not noise, noise was broken out earlier)
			oldFreq[nChan]&=0xfff0;
			oldFreq[nChan]|=c&0x0f;
		} else {
			// latch clear - data to whatever is latched
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
			}
		}
		setfreq(nChan, oldFreq[nChan]);
		break;
	}
}

// provide the WRITE_BYTE_TO_SOUND_CHIP macro
#define WRITE_BYTE_TO_SOUND_CHIP(x) writeSound(x)

#endif

#ifdef USE_AY_PSG
#include "ayemu_8912.h"
ayemu_ay_t psg;
ayemu_ay_reg_frame_t regs;

void writeSound(int reg, int c) {
    regs[reg] = c;
    ayemu_set_regs (&psg, regs);
}

#define WRITE_BYTE_TO_SOUND_CHIP(reg,x) writeSound(reg,x)

// wrapper function for the Classic99 audio driver
void ay_update(short *buf, double nAudioIn, int nSamples) {
    ayemu_gen_sound (&psg, buf, nSamples*2);
}

#endif

// and now include the actual implementation
#include "../CPlayer/CPlayer.c"

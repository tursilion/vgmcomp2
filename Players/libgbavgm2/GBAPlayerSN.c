// Wrapper for CPlayer.c that gives us the SN functions
// this directly includes CPlayer.c

#define SND1_VOL *((unsigned short*)(0x4000062))
#define SND2_VOL *((unsigned short*)(0x4000068))
// channel 3 has no useful volume - wave channel
#define SND4_VOL *((unsigned short*)(0x4000078))

#define SND1_FREQ *((unsigned short*)(0x4000064))
#define SND2_FREQ *((unsigned short*)(0x400006C))
#define SND3_FREQ *((unsigned short*)(0x4000074))
#define SND4_FREQ *((unsigned short*)(0x400007C))

// some registers needed for init
#define SND1_SWEEP *((unsigned short*)(0x4000060))

#define SND3_WAVE  *((unsigned short*)(0x4000070))
#define SND3_LEN   *((unsigned short*)(0x4000072))
#define SND3_WRAM   ((unsigned short*)(0x4000090))

#define SNDCNT_L   *((unsigned short*)(0x4000080))
#define SNDCNT_H   *((unsigned short*)(0x4000082))
#define SNDCNT_X   *((unsigned short*)(0x4000084))

// lookup table for SN frequency counts to GBA noise values - precalculated by brute force search in Blassic
// these values include R and S, 15 bit width, no length and retrigger bit
const unsigned short noiselookup[1024] = {
32912,33015,33012,32997,33010,32995,32981,32981,33009,32967,32979,32979,32965,32965,32965,33008,
33008,33008,32951,32951,32963,32963,32963,32963,32949,32949,32949,32949,32949,32992,32992,32992,
32992,32992,32992,32935,32935,32935,32935,32935,32947,32947,32947,32947,32947,32947,32947,32933,
32933,32933,32933,32933,32933,32933,32933,32933,32933,32933,32976,32976,32976,32976,32976,32976,
32976,32976,32976,32976,32976,32919,32919,32919,32919,32919,32919,32919,32919,32919,32919,32919,
32931,32931,32931,32931,32931,32931,32931,32931,32931,32931,32931,32931,32931,32931,32917,32917,
32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,32917,
32917,32917,32917,32917,32960,32960,32960,32960,32960,32960,32960,32960,32960,32960,32960,32960,
32960,32960,32960,32960,32960,32960,32960,32960,32960,32960,32903,32903,32903,32903,32903,32903,
32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32903,32915,
32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,
32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32915,32901,32901,32901,32901,
32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,
32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,32901,
32901,32901,32901,32901,32901,32901,32901,32944,32944,32944,32944,32944,32944,32944,32944,32944,
32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,
32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,32944,
32944,32944,32944,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,
32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,
32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32887,32899,32899,32899,
32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,
32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,
32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,32899,
32899,32899,32899,32899,32899,32899,32899,32899,32885,32885,32885,32885,32885,32885,32885,32885,
32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,
32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,
32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,
32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,
32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32885,32928,32928,32928,
32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,
32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,
32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,
32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,
32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,32928,
32928,32928,32928,32928,32928,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,
32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,
32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,
32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,
32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,
32871,32871,32871,32871,32871,32871,32871,32871,32871,32871,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,
32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32883,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,
32869,32869,32869,32869,32869,32869,32869,32869,32869,32869,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,
32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912,32912
};

void fillsndram(unsigned char vol) {
    static unsigned short cbank = 0x0040;   // current selected bank
    unsigned short val;
    
    vol &= 0x0f;
    val = (vol<<12)|(vol<<8)|(vol<<4)|vol;

    // This square wave pattern should result in the same frequency codes as the first two channels
    SND3_WRAM[0] = val;
    SND3_WRAM[1] = val;
    SND3_WRAM[2] = 0x0000;
    SND3_WRAM[3] = 0x0000;
    SND3_WRAM[4] = val;
    SND3_WRAM[5] = val;
    SND3_WRAM[6] = 0x0000;
    SND3_WRAM[7] = 0x0000;

    cbank = (cbank?0:0x0040);
    SND3_WAVE = cbank|0x0080;   // start and swap banks
}

// sn chip sim init - make sure to call this early
// NOTE: this turns off the DMA channels, so turn them on after calling this if needed
void gbasninit() {
    SNDCNT_X   = 0x0080;    // enable sound registers

    // turn off channel 1 sweep
    SND1_SWEEP = 0x0008;    // sweep 0, decrease (recommended), time 0
    SND1_VOL   = 0x1840;    // len 0, duty 50%, no envelope, increase, mute
    SND1_FREQ  = 0xC000;    // trigger, dur, freq 0, length off, off

    // Channel 2 doesn't have sweep
    SND2_VOL   = 0x1840;    // len 0, duty 50%, no envelope, increase, mute
    SND2_FREQ  = 0xC000;    // trigger, dur, freq 0, length off, off

    // Set up channel 3 with 32 4-bit samples representing a 50% square wave
    // This channel doesn't have a proper volume register
    SND3_WAVE  = 0x0040;    // one bank/32 digits, bank 1 selected, off
    SND3_LEN   = 0x0000;    // len 0, volume 0, force off
    SND3_FREQ  = 0xC000;    // trigger, dur, freq 0, length off, off
    fillsndram(0);  // fill with mute
    SND3_LEN   = 0x2000;    // len 0, 100% volume, force off

    SND4_VOL   = 0x1800;    // len 0, no envelope, increase, mute
    SND4_FREQ  = 0xC000;    // trigger, len, ratio 0, counter 15 bit, shift clock 0, len 0, off

    SNDCNT_L   = 0xFF77;    // all four channels left and right, maximum volume left and right
    SNDCNT_H   = 0x0002;    // max volume for sound 1-4, DMA sound off
}

// sn chip simulation
void snsim(unsigned char x) {
// For tone channels and wave channel:
// Ratio of SN frequency to GBA frequency should be 1.1728 * SNTONE
// Max SNTONE is 0x3FF, so multiply by 16 for fixed point:
// 0x3FF.0. 1.1728 becomes approximately 0x0013 (actually 0x0012.C3C9EED)
// If that sounds too far off, I can use a lookup table again. Not against 4k of data on the GBA scale.
    static unsigned char last = 0x80;
    static unsigned char regs[11] = { 0,0,0, 0,0,0, 0,0,0, 0,0 };
    unsigned short val;
    unsigned char chan = x&0xf0;
    if ((x&0x80) == 0) {
        chan = last+1;
    } else {
        last = chan;
        x &= 0x0F;  // least significant nibble
    }

    // frequency doesn't need a retrigger, but volume does

    switch (chan) {
        case 0x80:
            regs[0] = x;                // update register
            val = (regs[1]<<4)|x;       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND1_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0x81:
            regs[1] = x;                // update register
            val = ((unsigned int)x<<4)|regs[0];       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND1_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0x90:
        case 0x91:
            x &= 0x0f;
            regs[2] = x;
            x = 15 - x;                 // invert volume
            SND1_VOL = ((unsigned int)x<<12)|0x0840;  // volume at 50% duty, increase
            SND1_FREQ |= 0x8000;
            break;

        case 0xA0:
            regs[3] = x;                // update register
            val = (regs[4]<<4)|x;       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND2_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0xA1:
            regs[4] = x;                // update register
            val = ((unsigned int)x<<4)|regs[3];       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND2_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0xB0:
        case 0xB1:
            x &= 0x0f;
            regs[5] = x;
            x = 15 - x;                 // invert volume
            SND2_VOL = ((unsigned int)x<<12)|0x0840;  // volume at 50% duty, increase
            SND2_FREQ |= 0x8000;
            break;

        case 0xC0:
            regs[6] = x;                // update register
            val = (regs[7]<<4)|x;       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND3_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0xC1:
            regs[7] = x;                // update register
            val = ((unsigned int)x<<4)|regs[6];       // calculate SN code
            //val = ((val<<4)*0x13)>>4;   // scale up to GBA code
            SND3_FREQ = ((2047-val)&0x7ff);       // load it
            break;

        case 0xD0:
        case 0xD1:
            // this is a little different as we have to re-write the wave RAM
            x &= 0x0f;
            regs[8] = x;
            x = 15 - x;                 // invert volume
            fillsndram(x);
            SND3_FREQ |= 0x8000;
            break;

        case 0xE0:
        case 0xE1:
            // TODO: we COULD emulate the noise channel on one of the DMA channels, though that feels like overkill
            // we'll see how far off this is first
            x &= 0x0f;
            if (x != regs[9]) {
                regs[9] = x;
                // we don't have periodic, so accept only white noise, otherwise mute
                if (x&0x04) {
                    // figure out the TI shift rate
                    switch (x&0x03) {
                        case 0: val = 16;   break;
                        case 1: val = 32;   break;
                        case 2: val = 64;   break;
                        case 3: 
                            val = (regs[7]<<4)|regs[6]; // calculate SN code from channel 3
                            break;
                    }
                    val &= 0x3ff;   // make sure it's in range before we look it up
                    SND4_FREQ = noiselookup[1023-val];   // just use a table - it's only 2k
                } else {
                    // periodic noise disabled
                    SND4_VOL = 0x0000;
                    SND4_FREQ |= 0x8000;
                }
            }
            break;

        case 0xF0:
        case 0xF1:
            x &= 0x0f;
            regs[10] = x;
            x = 15 - x;                 // invert volume
            SND4_VOL = ((unsigned int)x<<12)|0x0800;  // volume (noise has no duty period), increase
            SND4_FREQ |= 0x8000;
            break;
    }
}

// no prefix for SN player
//#define SONG_PREFIX sfx_

// build for SN
#define USE_SN_PSG

// You must also provide "WRITE_BYTE_TO_SOUND_CHIP" as a macro. This takes
// three arguments: songActive (for mutes), channel (for mutes), byte
// the SN PSG, channel is 1-4 (no zero), and for the AY channel is
// the register number (0-10). See the CPlayerXX.c files for examples.
#define WRITE_BYTE_TO_SOUND_CHIP(mutes,chan,x) \
    if (((mutes)&(0x80>>((chan)-1)))==0) snsim(x);

// and now include the actual implementation
#include "CPlayer.c"

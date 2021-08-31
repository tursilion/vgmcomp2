// Quickplayer for GCC using libti99 and libtivgm2
// Just a quick example that the Windows tool fills in for easy usage.

#include <vdp.h>
#include <sound.h>
#ifdef BUILD_TI99
#include <TISNPlay.h>
#include <TISIDPlay.h>
#endif
#ifdef BUILD_COLECO
#include <ColecoSNPlay.h>
#include <ColecoAYPlay.h>
#endif
void *memcpy(void *destination, const void *source, int num);

// trampoline functions for chaining
#ifdef BUILD_TI99
// TI trampoline expects a cartridge header and boots the first program
const unsigned char tramp[] = {
    0xC8,0x00,          //  mov r0,@>6000           * page select (hard code page here)
    0x60,0x00,          //
    0xC0,0x20,          //  mov @>6006,r0           * pointer to program list
    0x60,0x06,          //
    0x05,0xC0,          //  inct r0                 * point to start address of first program
    0xC0,0x10,          //  mov *r0,r0              * get start address
    0x04,0x50           //  b *r0                   * execute it
};
#endif
#ifdef BUILD_COLECO
// Coleco trampoline simply jumps to the root of the page
const unsigned char tramp[] = {
    0x3A, 0xFF, 0xFF,   //  LD   a,($ffff)   ; page select (hard code page here)
    0xC3, 0x00, 0xC0    //  JP   $c000       ; execute it
};
#endif

// Windows app formats a full screen into here
// the four bytes after it are the pointers to
// the first song, and then the second. The first
// song is usually at >A000 and the second is
// whereever it fits. Either can be 0 for no song.
// data is in appropriate endian for the system,
// and the second chip is AY or SID depending on system.
// The next 3 bytes are the SID configuration registers,
// used by the TI only. Coleco does not use these at all.
// The fourth byte is a loop flag - 0 for no,
// anything else for yes. Loops is ignored if chaining.
// Bytes 5 and 6 are pointers to an address that contains
// information about chaining. It's externally set if
// there is a loader that handles playing. In both versions
// the value is a cartridge page number, which the code
// jumps to. (The TI version will parse the cart header).
// This structure is the same in all player programs.
// This one doesn't need to be volatile because we make
// no decisions based on its contents
const unsigned char textout[768] = {
    "~~~~DATAHERE~~~~\0"
};

// 00: six bytes of flag (~~FLAG)
// 06: two bytes for SN music (can be 0)
// 08: two bytes for SID or AY music (can be 0)
// 0A: three bytes of SID flags
// 0D: one byte of loop
// 0E: two bytes of pointer to a memory address for chaining
// This struct must exist in all player programs
// The older player doesn't have a ~~FLAG section, it's part of ~~~~DATAHERE~~~~,
// so if you can't find ~~FLAG then you don't have chaining.
const unsigned char flags[18] = {
    "~~FLAGxxyySSSL\0\0:"
};

unsigned int firstSong;     // for SN
unsigned int secondSong;    // for SID or AY

int main() {
    unsigned char done;
    unsigned int *chain;

    // setup the display
    set_graphics(VDP_SPR_8x8);
    charsetlc();
    vdpmemset(gColor, 0xf4, 32);
    VDP_SET_REGISTER(VDP_REG_COL, 0x04);

    // draw the user text - windows app has prepared a
    // full 768 byte screen for us
    vdpmemcpy(gImage, textout, 768);

    // get the song pointers
    firstSong = *((unsigned int*)&flags[6]);
    secondSong = *((unsigned int*)&flags[8]);

#ifdef BUILD_TI99 
    if (0 != secondSong) {
        // this is a SID song, so read the SID registers too
        SidCtrl1 = flags[10];
        SidCtrl2 = flags[11];
        SidCtrl3 = flags[12];
    }
#endif

    // const is fine for easy storage and patching, but for the decision making on
    // the loop value, we need to pretend it's volatile
    volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];

    do {
        // start the song(s)
        if (0 != firstSong) {
            StartSong((unsigned char*)firstSong, 0);
        }
        if (0 != secondSong) {
    #ifdef BUILD_TI99
            StartSID((unsigned char*)secondSong, 0);
    #endif
    #ifdef BUILD_COLECO
            ay_StartSong((unsigned char*)secondSong, 0);
    #endif
        }

        // and play it on the interrupt
        done = 0;
        while (!done) {
			done = 1;
		    vdpwaitvint();	// waits for a console interrupts, allows quit/etc
            if (0 != firstSong) {
                if (isSNPlaying) {
                    CALL_PLAYER_SN;
                    done = 0;
                }
            }
            if (0 != secondSong) {
        #ifdef BUILD_TI99
                if (isSIDPlaying) {
                    CALL_PLAYER_SID;
                    done = 0;
                }
        #endif
        #ifdef BUILD_COLECO
                if (isAYPlaying) {
                    CALL_PLAYER_AY;
                    done = 0;
                }
        #endif
            }
        }

        // loop, chain, or reset if it is finished
        // chain is normally set by a loader and takes priority
        chain = (unsigned int *)(*((unsigned int*)(&flags[14])));
        if (chain) {
            // look up the value to chain to
            unsigned int chained = *chain;
            if (chained) {
                // need to trampoline to the specified cartridge page
                // TI and Coleco each have different code and targets
    #ifdef BUILD_TI99
                memcpy((void*)0x8320, tramp, sizeof(tramp));   // 0x8320 so we don't overwrite the C registers
                *((unsigned int*)0x8322) = chained;     // patch the pointer
                ((void(*)())0x8320)();                  // call the function, never return
    #endif
    #ifdef BUILD_COLECO
                memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
                *((unsigned int*)0x7001) = chained;     // patch the pointer, chained should be on the stack
                ((void(*)())0x7000)();                  // call the function, never return
    #endif
            }
        }
    } while (*pLoop);

    // otherwise, reboot
#ifdef BUILD_TI99
    // GCC syntax (though technically my code will reset on exit anyway)
    __asm__ ("limi 0\n\tblwp @>0000");
#endif
#ifdef BUILD_COLECO
    // SDCC syntax
    __asm
        rst 0x00
    __endasm;
#endif

    // never reached
    return 2;

}

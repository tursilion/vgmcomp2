//
// (C) 2004 Mike Brent aka Tursi aka HarmlessLion.com
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
// Tursi's GB defines (from: all over the place)

#ifndef TURSIGB_H
#define TURSIGB_H

/* Copy this code into RAM for more speed */
#define CODE_IN_IWRAM __attribute__ ((section (".iwram"), long_call))
/* this code is in ROM - generate long jumps as needed. 4,1 wait states */
#define CODE_IN_ROM __attribute__ ((section (".text"), long_call))
/* Use this data in EWRAM (256k bank) 2 wait states */
#define DATA_IN_EWRAM __attribute__ ((section (".ewram")))

typedef     unsigned char           u8;
typedef     unsigned short int      u16;
typedef     unsigned int            u32;
typedef     unsigned long long int  u64;
typedef     signed int              s32;
typedef     int                     bool;
#define true (1)
#define false (0)

// Video functions
#define VCOUNT (*(volatile u16*)0x04000006)
//#define vsync() { while (VCOUNT != 159); while (VCOUNT < 160); }
#define vsync() { while (VCOUNT != 160); }
#define WAITSYNC(X) { unsigned int xend = myJiffies+(X); while (myJiffies<xend); }

// Video tables
#define BG_PALETTE  (0x5000000)
#define SPR_PALETTE (0x5000200)
#define BG_RAM_BASE (0x6000000)
#define SPR_RAM_BASE (0x6010000)

// Video control
#define REG_DISPCNT    *(volatile u32*)0x4000000

// Flags
#define MODE0               0x0000      // 0-3 text
#define MODE1               0x0001      // 0-1 text, 2 rotation
#define MODE2               0x0002      // 0-1 text, 2-3 rotation
#define MODE3               0x0003      // 15-bit bitmapped
#define MODE4               0x0004      // 8-bit bitmapped
#define MODE5               0x0005      // 15-bit bitmapped (160x128)

#define PAGE2               0x0010
#define PROCESS_HBLANK      0x0020
#define SPRITES_1D          0x0040
#define SPRITES_2D          0x0000
#define VIDEO_BLANK         0x0080

#define BG0_ENABLE          0x0100
#define BG1_ENABLE          0x0200
#define BG2_ENABLE          0x0400
#define BG3_ENABLE          0x0800
#define SPRITE_ENABLE       0x1000
#define WINDOW0_ENABLE      0x2000
#define WINDOW1_ENABLE      0x4000
#define SPRITE_WINDOWS      0x8000

// Video status
#define REG_DISPSTAT   *(volatile u16*)0x4000004

// flags
#define VREFRESH            0x0001
#define HREFRESH            0x0002
#define VCOUNT_TRIGGER      0x0004
#define VBLANK_IRQ          0x0008
#define HBLANK_IRQ          0x0010
#define VCOUNT_TRIGGER_IRQ  0x0020

// Background control registers
#define REG_BG0CNT      *(volatile u16*)0x4000008
#define REG_BG1CNT      *(volatile u16*)0x400000A
#define REG_BG2CNT      *(volatile u16*)0x400000C
#define REG_BG3CNT      *(volatile u16*)0x400000E

// flags
#define PRIORITY0       0x0000      // top
#define PRIORITY1       0x0001
#define PRIORITY2       0x0002
#define PRIORITY3       0x0003

#define TILE_DAT_BANK0  0x0000
#define TILE_DAT_BANK1  0x0004
#define TILE_DAT_BANK2  0x0008
#define TILE_DAT_BANK3  0x000C

#define MOSAIC_ON       0x0040
#define COLORS_16       0x0000
#define COLORS_256      0x0080
#define SCREEN_WRAP     0x2000      // rotational bg's only

#define MAP_SIZE_1x1    0x0000
#define MAP_SIZE_2x1    0x4000
#define MAP_SIZE_1x2    0x8000
#define MAP_SIZE_2x2    0xC000

#define TILE_MAP_BANK_SHIFT 8

// Scroll registers (text only)
#define REG_BG0_HSCROLL   *(volatile u16*)0x4000010
#define REG_BG0_VSCROLL   *(volatile u16*)0x4000012
#define REG_BG1_HSCROLL   *(volatile u16*)0x4000014
#define REG_BG1_VSCROLL   *(volatile u16*)0x4000016
#define REG_BG2_HSCROLL   *(volatile u16*)0x4000018
#define REG_BG2_VSCROLL   *(volatile u16*)0x400001A
#define REG_BG3_HSCROLL   *(volatile u16*)0x400001C
#define REG_BG3_VSCROLL   *(volatile u16*)0x400001E

// bg rotation scaling
#define REG_BG2PA         *(volatile u16*)0x4000020
#define REG_BG2PB         *(volatile u16*)0x4000022
#define REG_BG2PC         *(volatile u16*)0x4000024
#define REG_BG2PD         *(volatile u16*)0x4000026
#define REG_BG2X          *(volatile u32*)0x4000028
#define REG_BG2X_L        *(volatile u16*)0x4000028
#define REG_BG2X_H        *(volatile u16*)0x400002A
#define REG_BG2Y          *(volatile u32*)0x400002C
#define REG_BG2Y_L        *(volatile u16*)0x400002C
#define REG_BG2Y_H        *(volatile u16*)0x400002E

#define REG_BG3PA         *(volatile u16*)0x4000030
#define REG_BG3PB         *(volatile u16*)0x4000032
#define REG_BG3PC         *(volatile u16*)0x4000034
#define REG_BG3PD         *(volatile u16*)0x4000036
#define REG_BG3X          *(volatile u32*)0x4000038
#define REG_BG3X_L        *(volatile u16*)0x4000038
#define REG_BG3X_H        *(volatile u16*)0x400003A
#define REG_BG3Y          *(volatile u32*)0x400003C
#define REG_BG3Y_L        *(volatile u16*)0x400003C
#define REG_BG3Y_H        *(volatile u16*)0x400003E

// Windowing
#define REG_WIN0H         *(volatile u16*)0x4000040
#define REG_WIN1H         *(volatile u16*)0x4000042
#define REG_WIN0V         *(volatile u16*)0x4000044
#define REG_WIN1V         *(volatile u16*)0x4000046

// Window in/out control
#define REG_WININ         *(volatile u16*)0x4000048

#define WININ0_BG0          0x0001
#define WININ0_BG1          0x0002
#define WININ0_BG2          0x0004
#define WININ0_BG3          0x0008
#define WININ0_OBJ          0x0010
#define WININ0_SFX          0x0020

#define WININ1_BG0          0x0100
#define WININ1_BG1          0x0200
#define WININ1_BG2          0x0400
#define WININ1_BG3          0x0800
#define WININ1_OBJ          0x1000
#define WININ1_SFX          0x2000

#define REG_WINOUT        *(volatile u16*)0x400004A

#define WINOUT_BG0          0x0001
#define WINOUT_BG1          0x0002
#define WINOUT_BG2          0x0004
#define WINOUT_BG3          0x0008
#define WINOUT_OBJ          0x0010
#define WINOUT_SFX          0x0020

#define WINOBJ_BG0          0x0100
#define WINOBJ_BG1          0x0200
#define WINOBJ_BG2          0x0400
#define WINOBJ_BG3          0x0800
#define WINOBJ_OBJ          0x1000
#define WINOBJ_SFX          0x2000

// Video Effects
#define REG_MOSAIC          *(volatile u32*)0x400004C
#define REG_BLDCNT          *(volatile u16*)0x4000050
#define REG_BLDALPHA        *(volatile u16*)0x4000052
#define REG_BLDY            *(volatile u16*)0x4000054
#define BLDY_BLACK 16

// Flags
#define SOURCE_BG0          0x0001
#define SOURCE_BG1          0x0002
#define SOURCE_BG2          0x0004
#define SOURCE_BG3          0x0008
#define SOURCE_SPRITES      0x0010
#define SOURCE_BACKDROP     0x0020
#define TARGET_BG0          0x0100
#define TARGET_BG1          0x0200
#define TARGET_BG2          0x0400
#define TARGET_BG3          0x0800
#define TARGET_SPRITES      0x1000
#define TARGET_BACKDROP     0x2000
#define ALPHA_BLEND         0x0040
#define LIGHTEN             0x0080
#define DARKEN              0x00C0

// Sprites
// be careful with sprites and background priorities. If you have sprites
// that need different priorities relative to the background, it is important
// that they be sorted in the sprite table with the higher priority sprites
// first. Otherwise they will affect the priority of other sprites!
#define OAM_BASE            ((u16*)0x07000000)

#define SPR_SETY(n,y)       *(OAM_BASE+((n)<<2))=((*(OAM_BASE+((n)<<2)))&0xff00)|(y);
#define SPR_ROTSCALEON(n)   *(OAM_BASE+((n)<<2))|=0x0100;
#define SPR_ROTSCALEOFF(n)  *(OAM_BASE+((n)<<2))&=0xfeff;
#define SPR_DOUBLEON(n)     *(OAM_BASE+((n)<<2))|=0x0200;
#define SPR_DOUBLEOFF(n)    *(OAM_BASE+((n)<<2))&=0xfdff;
#define SPR_NORMAL(n)       *(OAM_BASE+((n)<<2))&=0xf3ff;
#define SPR_TRANSPARENT(n)  *(OAM_BASE+((n)<<2))=(*(OAM_BASE+((n)<<2))&0xfdff)|0x0400;
#define SPR_OBJWINDOW(n)    *(OAM_BASE+((n)<<2))=(*(OAM_BASE+((n)<<2))&0xfdff)|0x0800;
#define SPR_MOSAICON(n)     *(OAM_BASE+((n)<<2))|=0x1000;
#define SPR_MOSAICOFF(n)    *(OAM_BASE+((n)<<2))&=0xefff;
#define SPR_256COLOR(n)     *(OAM_BASE+((n)<<2))|=0x2000;
#define SPR_16COLOR(n)      *(OAM_BASE+((n)<<2))&=0xdfff;
#define SPR_SETSIZE(n, s)   *(OAM_BASE+((n)<<2))&=0x3fff; *(OAM_BASE+((n)<<2))|=((s)>>2)<<14; *(OAM_BASE+((n)<<2)+1)&=0x3fff; *(OAM_BASE+((n)<<2)+1)|=((s)&3)<<14;
#define SPR_8x8     0x0
#define SPR_16x16   0x1
#define SPR_32x32   0x2
#define SPR_64x64   0x3
#define SPR_16x8    0x4
#define SPR_32x8    0x5
#define SPR_32x16   0x6
#define SPR_64x32   0x7
#define SPR_8x16    0x8
#define SPR_8x32    0x9
#define SPR_16x32   0xa
#define SPR_32x64   0xb
#define SPR_SETX(n, x)      *(OAM_BASE+((n)<<2)+1)=((*(OAM_BASE+((n)<<2)+1))&0xfe00)|(x);
#define SPR_FLIPH(n)        *(OAM_BASE+((n)<<2)+1)|=0x1000;
#define SPR_NOFLIPH(n)      *(OAM_BASE+((n)<<2)+1)&=0xefff;
#define SPR_FLIPV(n)        *(OAM_BASE+((n)<<2)+1)|=0x2000;
#define SPR_NOFLIPV(n)      *(OAM_BASE+((n)<<2)+1)&=0xdfff;
#define SPR_ROTINDEX(n,x)   *(OAM_BASE+((n)<<2)+1)&=0xc1ff; *(OAM_BASE+((n)<<2)+1)|=(x)<<10;
#define SPR_SETTILE(n,x)    *(OAM_BASE+((n)<<2)+2)&=0xfc00; *(OAM_BASE+((n)<<2)+2)|=(x);
#define SPR_SETPRIORITY(n,x) *(OAM_BASE+((n)<<2)+2)&=0xf3ff; *(OAM_BASE+((n)<<2)+2)|=(x)<<10;
#define SPR_SETPALETTE(n,x) *(OAM_BASE+((n)<<2)+2)&=0x0fff; *(OAM_BASE+((n)<<2)+2)|=(x)<<12;
// Rotation and scaling are split up over four sprites, 't' is the table number, from 0-31
#define SPR_SETPA(t,x)      *(OAM_BASE+((t)<<4)+3)=(x);
#define SPR_SETPB(t,x)      *(OAM_BASE+((t)<<4)+7)=(x);
#define SPR_SETPC(t,x)      *(OAM_BASE+((t)<<4)+11)=(x);
#define SPR_SETPD(t,x)      *(OAM_BASE+((t)<<4)+15)=(x);

// sound register definitions
#define REG_SOUNDCNT_L        *(volatile u16*)0x04000080
#define REG_SOUNDCNT_H        *(volatile u16*)0x04000082
#define REG_SOUNDCNT_X        *(volatile u32*)0x04000084       // master sound enable and sound 1-4 play status

// flags
#define SND_ENABLED           0x00000080
#define SND_OUTPUT_RATIO_25   0x0000
#define SND_OUTPUT_RATIO_50   0x0001
#define SND_OUTPUT_RATIO_100  0x0002
#define DSA_OUTPUT_RATIO_50   0x0000
#define DSA_OUTPUT_RATIO_100  0x0004
#define DSA_OUTPUT_TO_RIGHT   0x0100
#define DSA_OUTPUT_TO_LEFT    0x0200
#define DSA_OUTPUT_TO_BOTH    0x0300
#define DSA_TIMER0            0x0000
#define DSA_TIMER1            0x0400
#define DSA_FIFO_RESET        0x0800
#define DSB_OUTPUT_RATIO_50   0x0000
#define DSB_OUTPUT_RATIO_100  0x0008
#define DSB_OUTPUT_TO_RIGHT   0x1000
#define DSB_OUTPUT_TO_LEFT    0x2000
#define DSB_OUTPUT_TO_BOTH    0x3000
#define DSB_TIMER0            0x0000
#define DSB_TIMER1            0x4000
#define DSB_FIFO_RESET        0x8000

// DMA register definitions
#define REG_DMA0SAD         *(volatile u32*)0x40000B0  // source address (note: 27 bit!)
#define REG_DMA0DAD         *(volatile u32*)0x40000B4  // destination address (all are 27 bit)
#define REG_DMA0CNT_L       *(volatile u16*)0x40000B8  // count register
#define REG_DMA0CNT_H       *(volatile u16*)0x40000BA  // control register

#define REG_DMA1SAD         *(volatile u32*)0x40000BC  // source address (all others are 28 bit)
#define REG_DMA1DAD         *(volatile u32*)0x40000C0  // destination address
#define REG_DMA1CNT_L       *(volatile u16*)0x40000C4  // count register
#define REG_DMA1CNT_H       *(volatile u16*)0x40000C6  // control register

#define REG_DMA2SAD         *(volatile u32*)0x40000C8  // source address
#define REG_DMA2DAD         *(volatile u32*)0x40000CC  // destination address
#define REG_DMA2CNT_L       *(volatile u16*)0x40000D0  // count register
#define REG_DMA2CNT_H       *(volatile u16*)0x40000D2  // control register

#define REG_DMA3SAD         *(volatile u32*)0x40000D4  // source address
#define REG_DMA3DAD         *(volatile u32*)0x40000D8  // destination address (28 bit)
#define REG_DMA3CNT_L       *(volatile u16*)0x40000DC  // count register
#define REG_DMA3CNT_H       *(volatile u16*)0x40000DE  // control register

// DMA flags
#define ENABLE_DMA          0x8000
#define DMA_INTERRUPT       0x4000
#define START_IMMEDIATELY   0x0000
#define START_ON_VBLANK     0x1000
#define START_ON_HBLANK     0x2000
#define START_ON_DRAW_LINE  0x3000
#define START_ON_FIFO_EMPTY 0x3000
#define GAMEPAK_DRQ         0x0800
#define WORD_DMA            0x0400
#define HALF_WORD_DMA       0x0000
#define DMA_REPEAT          0x0200
#define SRC_REG_INC         0x0000
#define SRC_REG_DEC         0x0080
#define SRC_REG_SAME        0x0100
// (increment, but reset after transfer)
#define SRC_REG_RESET       0x0180
#define DEST_REG_INC        0x0000
#define DEST_REG_DEC        0x0020
#define DEST_REG_SAME       0x0040

// Timer 0 register definitions
#define REG_TM0D            *(volatile u16*)0x4000100
#define REG_TM0CNT          *(volatile u16*)0x4000102
// Timer 1 register definitions
#define REG_TM1D            *(volatile u16*)0x4000104
#define REG_TM1CNT          *(volatile u16*)0x4000106
// Timer 2 register definitions
#define REG_TM2D            *(volatile u16*)0x4000108
#define REG_TM2CNT          *(volatile u16*)0x400010A
// Timer 3 register definitions
#define REG_TM3D            *(volatile u16*)0x400010C
#define REG_TM3CNT          *(volatile u16*)0x400010E

// Timer frequency
#define TIMER_16MHZ         0x0
#define TIMER_262KHZ        0x1
#define TIMER_65KHZ         0x2
#define TIMER_16KHZ         0x3
// Timer flags
#define TIMER_ENABLED       0x0080
#define TIMER_INTERRUPT     0x0040
#define TIMER_CASCADE       0x0004

// FIFO address defines
#define REG_FIFO_A          0x040000A0
#define REG_FIFO_B          0x040000A4

// Interrupt addresses
#define REG_IE              *(volatile u16*)0x4000200
#define REG_IF              *(volatile u16*)0x4000202
#define REG_IME             *(volatile u16*)0x4000208
#define INT_VECTOR          *(volatile void**)(0x3007ffc)      // store the interrupt handler address here

// Interrupt bits
#define INT_VBLANK          0x0001
#define INT_HBLANK          0x0002
#define INT_VCOUNT          0x0004
#define INT_TIMER0          0x0008
#define INT_TIMER1          0x0010
#define INT_TIMER2          0x0020
#define INT_TIMER3          0x0040
#define INT_SERIAL          0x0080
#define INT_DMA0            0x0100
#define INT_DMA1            0x0200
#define INT_DMA2            0x0400
#define INT_DMA3            0x0800
#define INT_KEY             0x1000
#define INT_CASSETTE        0x2000

// Keypad
#define REG_KEYINPUT        *(volatile u16*)0x4000130
#define REG_KEYCNT          *(volatile u16*)0x4000132

// Keypad bits (are '0' when pressed!)
#define BTN_A               0x0001
#define BTN_B               0x0002
#define BTN_SELECT          0x0004
#define BTN_START           0x0008
#define BTN_RIGHT           0x0010
#define BTN_LEFT            0x0020
#define BTN_UP              0x0040
#define BTN_DOWN            0x0080
#define BTN_R               0x0100
#define BTN_L               0x0200
#define BTN_MASK            0x03ff

#define BTN_INT_ENABLE          0x4000
#define BTN_INT_CONDITION       0x8000  /* 0 = OR, 1 = AND */

// Wait state control
#define REG_WSCNT           *(volatile u16*)0x4000204

// flags
#define WAIT_SRAM_4     0x0000
#define WAIT_SRAM_3     0x0001
#define WAIT_SRAM_2     0x0002
#define WAIT_SRAM_8     0x0003

#define WAIT_ROM8_4     0x0000
#define WAIT_ROM8_3     0x0004
#define WAIT_ROM8_2     0x0008
#define WAIT_ROM8_8     0x000C
#define WAIT_ROM8_SUB_2 0x0000
#define WAIT_ROM8_SUB_1 0x0010

#define WAIT_ROMA_4     0x0000
#define WAIT_ROMA_3     0x0020
#define WAIT_ROMA_2     0x0040
#define WAIT_ROMA_8     0x0060
#define WAIT_ROMA_SUB_4 0x0000
#define WAIT_ROMA_SUB_1 0x0080

#define WAIT_ROMC_4     0x0000
#define WAIT_ROMC_3     0x0100
#define WAIT_ROMC_2     0x0200
#define WAIT_ROMC_8     0x0300
#define WAIT_ROMC_SUB_8 0x0000
#define WAIT_ROMC_SUB_1 0x0400

#define ROM_PREFETCH    0x4000

// DMA3 based memcpy - max size is 32k - watch alignment!
void fastcopy16(u32 pDest, u32 pSrc, u32 nCount);
void fastcopy(u32 pDest, u32 pSrc, u32 nCount);
void fastset(u32 pDest, u32 value, u32 nCount);
void fastset16(u32 pDest, u16 value, u32 nCount);

#endif
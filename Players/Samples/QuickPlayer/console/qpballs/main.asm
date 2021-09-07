;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module main
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _memcpy
	.globl _SongLoop
	.globl _StartSong
	.globl _vdpscreenchar
	.globl _vdpchar
	.globl _vdpmemread
	.globl _vdpmemcpy
	.globl _vdpmemset
	.globl _set_graphics_raw
	.globl _frame
	.globl _nextSprite
	.globl _i2
	.globl _idx
	.globl _firstSong
	.globl _nOldVol
	.globl _nOldTarg
	.globl _finalcount
	.globl _delaypos
	.globl _delaytone
	.globl _delayvol
	.globl _colortab
	.globl _sprpath
	.globl _flags
	.globl _textout
	.globl _tramp
	.globl _fontxz
	.globl _fontvy
	.globl _fontqt
	.globl _fontnp
	.globl _fontjl
	.globl _font
	.globl _charmap
	.globl _delayDrums
	.globl _delayTones
	.globl _drums
	.globl _tones
	.globl _tonetarget
	.globl _drumhit
	.globl _tonehit
	.globl _ballsprite
	.globl _colorchan
	.globl _colorfade
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
_VDPRD	=	0x00be
_VDPST	=	0x00bf
_VDPWA	=	0x00bf
_VDPWD	=	0x00be
_SOUND	=	0x00ff
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
_sprpath::
	.ds 374
_colortab::
	.ds 32
_delayvol::
	.ds 120
_delaytone::
	.ds 240
_delaypos::
	.ds 1
_finalcount::
	.ds 1
_nOldTarg::
	.ds 8
_nOldVol::
	.ds 4
_firstSong::
	.ds 2
_idx::
	.ds 1
_i2::
	.ds 1
_main_lastFade_262145_94:
	.ds 1
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _INITIALIZED
_nextSprite::
	.ds 1
_frame::
	.ds 1
;--------------------------------------------------------
; absolute external ram data
;--------------------------------------------------------
	.area _DABS (ABS)
;--------------------------------------------------------
; global & static initialisations
;--------------------------------------------------------
	.area _HOME
	.area _GSINIT
	.area _GSFINAL
	.area _GSINIT
;main.c:421: static idx_t lastFade = 0;
	ld	iy, #_main_lastFade_262145_94
	ld	0 (iy), #0x00
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;main.c:132: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	ld	hl, #-9
	add	hl, sp
	ld	sp, hl
;main.c:137: unsigned char x = set_graphics_raw(VDP_SPR_8x8);	// set graphics mode with 8x8 sprites
	xor	a, a
	push	af
	inc	sp
	call	_set_graphics_raw
	ld	-7 (ix), l
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x0f
	out	(_VDPWA), a
	ld	a, #0x83
	out	(_VDPWA), a
;main.c:139: gColor = 0x03c0;
	ld	hl, #0x03c0
	ld	(_gColor), hl
;main.c:140: vdpmemset(gColor, 0x10, 32);			// all colors to black on transparent
	ld	hl, #0x0020
	push	hl
	ld	a, #0x10
	push	af
	inc	sp
	ld	hl, #0x03c0
	push	hl
	call	_vdpmemset
	pop	af
;main.c:141: vdpmemset(gPattern, 0, 8);				// char 0 is blank
	inc	sp
	ld	hl,#0x0008
	ex	(sp),hl
	xor	a, a
	push	af
	inc	sp
	ld	hl, (_gPattern)
	push	hl
	call	_vdpmemset
	pop	af
;main.c:142: vdpmemset(gImage, 0, 768);				// clear screen to char 0
	inc	sp
	ld	hl,#0x0300
	ex	(sp),hl
	xor	a, a
	push	af
	inc	sp
	ld	hl, (_gImage)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;main.c:143: vdpchar(gSprite, 0xd0);					// all sprites disabled
	ld	a, #0xd0
	push	af
	inc	sp
	ld	hl, (_gSprite)
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x0d
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;main.c:147: for (idx=0; idx<4; ++idx) {
	ld	hl, #_idx
	ld	(hl), #0x00
00196$:
;main.c:148: for (i2=0; i2<DELAYTIME; ++i2) {
	ld	hl, #_i2
	ld	(hl), #0x00
00194$:
;main.c:149: delayvol[idx][i2] = 0x0f | (0x90+(idx*0x20));
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ex	de, hl
	ld	hl, #_delayvol
	add	hl, de
	ex	de, hl
	ld	a,(#_i2 + 0)
	add	a, e
	ld	c, a
	ld	a, #0x00
	adc	a, d
	ld	b, a
	ld	a,(#_idx + 0)
	rrca
	rrca
	rrca
	and	a, #0xe0
	add	a, #0x90
	or	a, #0x0f
	ld	(bc), a
;main.c:150: delaytone[idx][i2] = idx*0x2000+0x8001;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, hl
	ld	de, #_delaytone
	add	hl, de
	ld	a,(#_i2 + 0)
	add	a, a
	add	a, l
	ld	c, a
	ld	a, #0x00
	adc	a, h
	ld	b, a
	ld	a,(#_idx + 0)
	rrca
	rrca
	rrca
	and	a, #0xe0
	ld	d, a
	ld	e, #0x00
	ld	hl, #0x8001
	add	hl, de
	ex	de, hl
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
;main.c:148: for (i2=0; i2<DELAYTIME; ++i2) {
	ld	iy, #_i2
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x1e
	jr	C, 00194$
;main.c:147: for (idx=0; idx<4; ++idx) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x04
	jp	C, 00196$
;main.c:163: for (idx = 2; idx < 178; idx += 8) {
	ld	0 (iy), #0x02
00198$:
;main.c:164: vdpmemcpy(gPattern+(idx*8), tonehit, 6*8);
	ld	bc, #_tonehit
	ld	a, (#_idx + 0)
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, (_gPattern)
	add	hl, de
	ld	de, #0x0030
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:163: for (idx = 2; idx < 178; idx += 8) {
	ld	iy, #_idx
	ld	a, 0 (iy)
	add	a, #0x08
	ld	(_idx+0), a
	ld	a, 0 (iy)
	sub	a, #0xb2
	jr	C, 00198$
;main.c:166: for (idx = 178; idx < 242; idx += 8) {
	ld	0 (iy), #0xb2
00200$:
;main.c:167: vdpmemcpy(gPattern+(idx*8), drumhit, 6*8);
	ld	bc, #_drumhit
	ld	a, (#_idx + 0)
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	de, (_gPattern)
	add	hl, de
	ld	de, #0x0030
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:166: for (idx = 178; idx < 242; idx += 8) {
	ld	iy, #_idx
	ld	a, 0 (iy)
	add	a, #0x08
	ld	(_idx+0), a
	ld	a, 0 (iy)
	sub	a, #0xf2
	jr	C, 00200$
;main.c:170: vdpmemcpy(gPattern+8, ballsprite, 8);
	ld	bc, #_ballsprite+0
	ld	hl, (_gPattern)
	ld	de, #0x0008
	add	hl, de
	ld	de, #0x0008
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:173: vdpmemcpy(gPattern+(240*8), font, 16*8);
	ld	bc, #_font+0
	ld	hl, (_gPattern)
	ld	de, #0x0780
	add	hl, de
	ld	de, #0x0080
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:174: vdpmemcpy(gPattern+(176*8), fontjl, 2*8);
	ld	bc, #_fontjl+0
	ld	hl, (_gPattern)
	ld	de, #0x0580
	add	hl, de
	ld	de, #0x0010
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:175: vdpmemcpy(gPattern+(184*8), fontnp, 2*8);
	ld	bc, #_fontnp+0
	ld	hl, (_gPattern)
	ld	de, #0x05c0
	add	hl, de
	ld	de, #0x0010
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:176: vdpmemcpy(gPattern+(192*8), fontqt, 2*8);
	ld	bc, #_fontqt+0
	ld	hl, (_gPattern)
	ld	de, #0x0600
	add	hl, de
	ld	de, #0x0010
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:177: vdpmemcpy(gPattern+(200*8), fontvy, 2*8);
	ld	bc, #_fontvy+0
	ld	hl, (_gPattern)
	ld	de, #0x0640
	add	hl, de
	ld	de, #0x0010
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:178: vdpmemcpy(gPattern+(232*8), fontxz, 2*8);
	ld	bc, #_fontxz+0
	ld	hl, (_gPattern)
	ld	de, #0x0740
	add	hl, de
	ld	de, #0x0010
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:182: for (idx=0; idx<22; idx++) {
	ld	hl, #_idx
	ld	(hl), #0x00
00202$:
;main.c:183: idx_t r=tones[idx*2];
	ld	iy, #_idx
	ld	l, 0 (iy)
	ld	h, #0x00
	add	hl, hl
	ld	de, #_tones
	add	hl, de
	ld	e, (hl)
;main.c:184: idx_t c=tones[idx*2+1];
	ld	d, 0 (iy)
	ld	a, d
	add	a, a
	inc	a
	ld	c, a
	rla
	sbc	a, a
	ld	b, a
	ld	hl, #_tones
	add	hl, bc
	ld	a, (hl)
	ld	-9 (ix), a
;main.c:185: idx_t ch=idx*8+FIRST_TONE_CHAR;
	ld	a, d
	add	a, a
	add	a, a
	add	a, a
	add	a, #0x02
	ld	-8 (ix), a
;main.c:187: vdpscreenchar(VDP_SCREEN_POS(r-1,c-1), ch);
	ld	a, -9 (ix)
	ld	-6 (ix), a
	ld	c, a
	dec	c
	ld	-5 (ix), c
	ld	-4 (ix), e
	ld	-3 (ix), #0
	ld	a, -4 (ix)
	add	a, #0xff
	ld	-2 (ix), a
	ld	a, -3 (ix)
	adc	a, #0xff
	ld	-1 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	ld	b, h
	ld	l, -5 (ix)
	ld	h, #0x00
	add	a, l
	ld	l, a
	ld	a, b
	adc	a, h
	ld	h, a
;main.c:187: vdpscreenchar(VDP_SCREEN_POS(r-1,c-1), ch);
	push	bc
	push	de
	ld	a, -8 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;main.c:188: vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch+1);
	ld	a, -8 (ix)
	ld	-3 (ix), a
	inc	a
	ld	-4 (ix), a
	ld	d, #0x00
	ld	l, e
	ld	h, d
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	b, #0x00
	add	hl, bc
;main.c:188: vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch+1);
	push	de
	ld	a, -4 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
;main.c:190: vdpscreenchar(VDP_SCREEN_POS(r-1,c), ch+2);
	ld	a, -3 (ix)
	add	a, #0x02
	ld	-4 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	c, -9 (ix)
	ld	b, #0x00
	add	hl, bc
;main.c:190: vdpscreenchar(VDP_SCREEN_POS(r-1,c), ch+2);
	push	bc
	push	de
	ld	a, -4 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;main.c:191: vdpscreenchar(VDP_SCREEN_POS(r,c), ch+3);
	ld	h, -3 (ix)
	inc	h
	inc	h
	inc	h
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	ex	de, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	ld	a, e
	add	a, c
	ld	c, a
	ld	a, d
	adc	a, b
	ld	b, a
;main.c:191: vdpscreenchar(VDP_SCREEN_POS(r,c), ch+3);
	push	de
	push	hl
	inc	sp
	push	bc
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
;main.c:193: vdpscreenchar(VDP_SCREEN_POS(r-1,c+1), ch+4);
	ld	a, -3 (ix)
	add	a, #0x04
	ld	-4 (ix), a
	ld	c, -6 (ix)
	inc	c
	ld	a, c
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	b, #0x00
	add	a, l
	ld	l, a
	ld	a, b
	adc	a, h
	ld	h, a
;main.c:193: vdpscreenchar(VDP_SCREEN_POS(r-1,c+1), ch+4);
	push	bc
	push	de
	ld	a, -4 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;main.c:194: vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+5);
	ld	a, -3 (ix)
	add	a, #0x05
	ld	b, a
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	ld	l, c
	ld	h, #0x00
	add	hl, de
;main.c:194: vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+5);
	push	bc
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
;main.c:182: for (idx=0; idx<22; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x16
	jp	C, 00202$
;main.c:197: for (idx=0; idx<8; idx++) {
	ld	0 (iy), #0x00
00204$:
;main.c:198: idx_t r=drums[idx*2];
	ld	iy, #_idx
	ld	l, 0 (iy)
	ld	h, #0x00
	add	hl, hl
	ld	de, #_drums
	add	hl, de
	ld	a, (hl)
	ld	-6 (ix), a
;main.c:199: idx_t c=drums[idx*2+1];
	ld	b, 0 (iy)
	ld	a, b
	add	a, a
	inc	a
	ld	e, a
	rla
	sbc	a, a
	ld	d, a
	ld	hl, #_drums
	add	hl, de
	ld	c, (hl)
;main.c:200: idx_t ch=idx*8+FIRST_DRUM_CHAR;
	ld	a, b
	add	a, a
	add	a, a
	add	a, a
	add	a, #0xb2
	ld	-1 (ix), a
;main.c:202: vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch);
	ld	-5 (ix), c
	ld	a, c
	add	a, #0xff
	ld	-4 (ix), a
	ld	e, a
	ld	a, -6 (ix)
	ld	-3 (ix), a
	ld	-2 (ix), #0
	ld	l, a
	ld	h, #0
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	d, #0x00
	add	hl, de
;main.c:202: vdpscreenchar(VDP_SCREEN_POS(r,c-1), ch);
	push	bc
	ld	a, -1 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	bc
;main.c:203: vdpscreenchar(VDP_SCREEN_POS(r+1,c-1), ch+1);
	ld	b, -1 (ix)
	inc	b
	ld	e, -4 (ix)
	ld	l, -6 (ix)
	ld	h, #0x00
	inc	hl
	ex	(sp), hl
	pop	hl
	push	hl
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	d, #0x00
	add	hl, de
;main.c:203: vdpscreenchar(VDP_SCREEN_POS(r+1,c-1), ch+1);
	push	bc
	push	bc
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	bc
;main.c:205: vdpscreenchar(VDP_SCREEN_POS(r,c), ch+2);
	ld	d, -1 (ix)
	inc	d
	inc	d
	ld	l, -3 (ix)
	ld	h, -2 (ix)
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	b, #0x00
	add	hl, bc
;main.c:205: vdpscreenchar(VDP_SCREEN_POS(r,c), ch+2);
	push	bc
	push	de
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	bc
;main.c:206: vdpscreenchar(VDP_SCREEN_POS(r+1,c), ch+3);
	ld	h, -1 (ix)
	inc	h
	inc	h
	inc	h
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	pop	de
	ex	de, hl
	push	hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	ld	a, e
	add	a, c
	ld	c, a
	ld	a, d
	adc	a, b
	ld	b, a
;main.c:206: vdpscreenchar(VDP_SCREEN_POS(r+1,c), ch+3);
	push	de
	push	hl
	inc	sp
	push	bc
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
;main.c:208: vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+4);
	ld	a, -1 (ix)
	add	a, #0x04
	ld	-4 (ix), a
	ld	c, -5 (ix)
	inc	c
	ld	a, c
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	ld	l, -3 (ix)
	ld	h, -2 (ix)
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	b, #0x00
	add	a, l
	ld	l, a
	ld	a, b
	adc	a, h
	ld	h, a
;main.c:208: vdpscreenchar(VDP_SCREEN_POS(r,c+1), ch+4);
	push	bc
	push	de
	ld	a, -4 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;main.c:209: vdpscreenchar(VDP_SCREEN_POS(r+1,c+1), ch+5);
	ld	a, -1 (ix)
	add	a, #0x05
	ld	b, a
	ld	a, c
;d:/work/coleco/libti99coleco/vdp.h:75: inline int VDP_SCREEN_POS(unsigned int r, unsigned char c)			{	return (((r)<<5)+(c));						}
	ld	h, #0x00
	ld	l, a
	add	hl, de
;main.c:209: vdpscreenchar(VDP_SCREEN_POS(r+1,c+1), ch+5);
	push	bc
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
;main.c:197: for (idx=0; idx<8; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x08
	jp	C, 00204$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, -7 (ix)
	out	(_VDPWA), a
	ld	a, #0x81
	out	(_VDPWA), a
;main.c:219: vdpmemread(gColor, colortab, 32);
	ld	hl, #0x0020
	push	hl
	ld	hl, #_colortab
	push	hl
	ld	hl, (_gColor)
	push	hl
	call	_vdpmemread
	pop	af
	pop	af
	pop	af
;main.c:225: VDP_SET_ADDRESS_WRITE(gImage+(21*32));
	ld	iy, #_gImage
	ld	a, 0 (iy)
	add	a, #0xa0
	ld	b, a
	ld	a, 1 (iy)
	adc	a, #0x02
	ld	c, a
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, b
	out	(_VDPWA), a
	ld	a, c
	or	a, #0x40
	out	(_VDPWA), a
;main.c:226: for (idx=0; idx<32*3; ++idx) {
	ld	hl, #_idx
	ld	(hl), #0x00
	ld	bc, #_textout+0
00206$:
;main.c:227: unsigned char c = textout[idx];
	ld	hl, (_idx)
	ld	h, #0x00
	add	hl, bc
	ld	e, (hl)
;main.c:228: if (c>96) c-=32;	// make uppercase
	ld	a, #0x60
	sub	a, e
	jr	NC, 00108$
	ld	a, e
	add	a, #0xe0
	ld	e, a
00108$:
;main.c:229: if ((c<32)||(c>90)) {
	ld	a, e
	sub	a, #0x20
	jr	C, 00109$
	ld	a, #0x5a
	sub	a, e
	jr	NC, 00110$
00109$:
;main.c:230: VDPWD=132;
	ld	a, #0x84
	out	(_VDPWD), a
	jr	00207$
00110$:
;main.c:232: VDPWD=charmap[c-32];
	ld	a, e
	add	a, #0xe0
	ld	e, a
	rla
	sbc	a, a
	ld	d, a
	ld	hl, #_charmap
	add	hl, de
	ld	a, (hl)
	out	(_VDPWD), a
00207$:
;main.c:226: for (idx=0; idx<32*3; ++idx) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x60
	jr	C, 00206$
;main.c:238: firstSong = *((unsigned int*)&flags[6]);
	ld	hl, #(_flags + 0x0006)
	ld	a, (hl)
	ld	(_firstSong+0), a
	inc	hl
	ld	a, (hl)
	ld	(_firstSong+1), a
;main.c:241: volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];
;main.c:243: do {
00174$:
;main.c:245: finalcount = 30;
	ld	hl, #_finalcount
	ld	(hl), #0x1e
;main.c:247: for (idx=0; idx<4; idx++) {
	ld	hl, #_idx
	ld	(hl), #0x00
00210$:
;main.c:248: nOldVol[idx]=0xff;
	ld	a, #<(_nOldVol)
	ld	hl, #_idx
	add	a, (hl)
	ld	c, a
	ld	a, #>(_nOldVol)
	adc	a, #0x00
	ld	b, a
	ld	a, #0xff
	ld	(bc), a
;main.c:249: nOldTarg[idx]=255;
	ld	a, #<(_nOldTarg)
	ld	hl, #_idx
	add	a, (hl)
	ld	c, a
	ld	a, #>(_nOldTarg)
	adc	a, #0x00
	ld	b, a
	ld	a, #0xff
	ld	(bc), a
;main.c:250: nOldTarg[idx+4]=255;
	ld	a,(#_idx + 0)
	add	a, #0x04
	ld	c, a
	rla
	sbc	a, a
	ld	b, a
	ld	hl, #_nOldTarg
	add	hl, bc
	ld	(hl), #0xff
;main.c:251: for (i2=0; i2<DELAYTIME; i2++) {
	ld	hl, #_i2
	ld	(hl), #0x00
00208$:
;main.c:253: delayvol[idx][i2]=(idx*0x20) + 0x9f;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ex	de, hl
	ld	hl, #_delayvol
	add	hl, de
	ex	de, hl
	ld	a,(#_i2 + 0)
	add	a, e
	ld	c, a
	ld	a, #0x00
	adc	a, d
	ld	b, a
	ld	a,(#_idx + 0)
	rrca
	rrca
	rrca
	and	a, #0xe0
	add	a, #0x9f
	ld	(bc), a
;main.c:251: for (i2=0; i2<DELAYTIME; i2++) {
	ld	iy, #_i2
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x1e
	jr	C, 00208$
;main.c:247: for (idx=0; idx<4; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x04
	jr	C, 00210$
;main.c:257: for (idx=0; idx<MAX_SPRITES; idx++) {
	ld	0 (iy), #0x00
00212$:
;main.c:259: sprpath[idx].z=0;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	de, #_sprpath
	add	hl, de
	ld	bc, #0x0009
	add	hl, bc
	ld	(hl), #0x00
;main.c:257: for (idx=0; idx<MAX_SPRITES; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x22
	jr	C, 00212$
;main.c:262: if (0 != firstSong) {
	ld	iy, #_firstSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00118$
;main.c:263: StartSong((unsigned char*)firstSong,0);
	ld	hl, (_firstSong)
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_StartSong
	pop	af
	inc	sp
;main.c:268: songNote[0]=0x8001;
	ld	hl, #0x8001
	ld	(_songNote), hl
;main.c:269: songNote[1]=0xa001;
	ld	h, #0xa0
	ld	((_songNote + 0x0002)), hl
;main.c:270: songNote[2]=0xc001;
	ld	h, #0xc0
	ld	((_songNote + 0x0004)), hl
;main.c:271: songNote[3]=0xe101;
	ld	h, #0xe1
	ld	((_songNote + 0x0006)), hl
00118$:
;main.c:273: delaypos = 0;
	ld	hl, #_delaypos
	ld	(hl), #0x00
;main.c:275: while (finalcount) {
00169$:
	ld	a,(#_finalcount + 0)
	or	a, a
	jp	Z, 00171$
;main.c:287: VDP_WAIT_VBLANK_CRU;					// wait for int
00119$:
	ld	a,(#_vdpLimi + 0)
	rlca
	jr	NC, 00119$
;main.c:288: VDP_CLEAR_VBLANK;						// clear it
	ld	hl, #_vdpLimi
	ld	(hl), #0x00
	in	a, (_VDPST)
	ld	(_VDP_STATUS_MIRROR+0), a
;main.c:292: VDP_SET_ADDRESS_WRITE(gSprite);
	ld	bc, (_gSprite)
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, c
	out	(_VDPWA), a
	ld	a, b
	or	a, #0x40
	out	(_VDPWA), a
;main.c:293: for (idx=0; idx<MAX_SPRITES; ++idx) {
	ld	hl, #_idx
	ld	(hl), #0x00
00214$:
;main.c:294: if (sprpath[idx].z < 0) {
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ex	de, hl
	ld	hl, #_sprpath
	add	hl, de
	ex	de, hl
	ld	c, e
	ld	b, d
	ld	hl, #9
	add	hl, bc
	ld	c, (hl)
	bit	7, c
	jr	Z, 00215$
;main.c:295: VDPWD=(sprpath[idx].y>>4)+(sprpath[idx].z);	// integer y position plus z bounce
	ld	l, e
	ld	h, d
	inc	hl
	inc	hl
	ld	b, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, b
	ld	b, #0x04
00518$:
	srl	h
	rr	l
	djnz	00518$
	ld	a, l
	add	a, c
	out	(_VDPWD), a
;main.c:296: VDPWD=sprpath[idx].x>>4;
	ld	l, e
	ld	h, d
	ld	c, (hl)
	inc	hl
	ld	l, (hl)
	ld	b, #0x04
00519$:
	srl	l
	rr	c
	djnz	00519$
	ld	a, c
	out	(_VDPWD), a
;main.c:298: VDPWD=1;		// char 1 is the ball
	ld	a, #0x01
	out	(_VDPWD), a
;main.c:299: VDPWD=sprpath[idx].col;
	ld	hl, #10
	add	hl, de
	ld	a, (hl)
	out	(_VDPWD), a
00215$:
;main.c:293: for (idx=0; idx<MAX_SPRITES; ++idx) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x22
	jr	C, 00214$
;main.c:302: VDPWD = 0xd0;	// terminate the sprite table
	ld	a, #0xd0
	out	(_VDPWD), a
;main.c:305: CALL_PLAYER_SN;
	call	_SongLoop
;main.c:307: if (!(songActive&SONGACTIVEACTIVE)) {
	ld	hl, (#(_songNote + 0x0006) + 0)
	bit	0, l
	jr	NZ, 00126$
;main.c:309: --finalcount;
	ld	hl, #_finalcount
	dec	(hl)
00126$:
;main.c:312: ++frame;
	ld	hl, #_frame
	inc	(hl)
;main.c:315: for (idx=0; idx<4; idx++) {
	ld	hl, #_idx
	ld	(hl), #0x00
00216$:
;main.c:316: idx_t arpvoice = idx + ((frame&1)<<2);	// start at 0 or 4
	ld	a,(#_frame + 0)
	and	a, #0x01
	add	a, a
	add	a, a
	ld	hl, #_idx
	ld	c, (hl)
	add	a, c
	ld	-5 (ix), a
;main.c:319: delayvol[idx][delaypos] = songVol[idx];
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	ex	de, hl
	ld	hl, #_delayvol
	add	hl, de
	ex	de, hl
	ld	a,(#_delaypos + 0)
	add	a, e
	ld	c, a
	ld	a, #0x00
	adc	a, d
	ld	b, a
	ld	hl, #_songVol+0
	ld	de, (_idx)
	ld	d, #0x00
	add	hl, de
	ld	a, (hl)
	ld	(bc), a
;main.c:321: unsigned int x = songNote[idx];
	ld	bc, #_songNote+0
	ld	a, (#_idx + 0)
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	add	hl, bc
	ld	a, (hl)
	ld	-2 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-1 (ix), a
;main.c:322: delaytone[idx][delaypos] = x;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, hl
	ld	de, #_delaytone
	add	hl, de
	ld	a,(#_delaypos + 0)
	add	a, a
	ld	e, a
	ld	d, #0x00
	add	hl, de
	ld	a, -2 (ix)
	ld	(hl), a
	inc	hl
	ld	a, -1 (ix)
	ld	(hl), a
;main.c:325: if (idx != 3) {
	ld	a,(#_idx + 0)
	sub	a, #0x03
	jr	Z, 00128$
;main.c:327: x=((x<<4)&0x03f0)|((x>>8)&0x0f);
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	and	a, #0xf0
	ld	c, a
	ld	a, h
	and	a, #0x03
	ld	b, a
	ld	a, -1 (ix)
	and	a, #0x0f
	ld	e, #0x00
	or	a, c
	ld	c, a
	ld	a, e
	or	a, b
	ld	b, a
	ld	-2 (ix), c
	ld	-1 (ix), b
00128$:
;main.c:331: idx_t targ = 255;
	ld	-4 (ix), #0xff
;main.c:332: if (((songVol[idx]&0x0f) < 0x0f)) {
	ld	bc, #_songVol+0
	ld	hl, (_idx)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	and	a, #0x0f
	ld	c, a
	ld	b, #0x00
	ld	a, c
	sub	a, #0x0f
	ld	a, b
	rla
	ccf
	rra
	sbc	a, #0x80
	jp	NC, 00147$
;main.c:333: if (idx == 3) {
	ld	a,(#_idx + 0)
	sub	a, #0x03
	jr	NZ, 00130$
;main.c:334: targ=((x>>8)&0x07);		//+FIRST_DRUM_COLOR;	// added just to be unique?
	ld	a, -1 (ix)
	and	a, #0x07
	ld	-4 (ix), a
	jr	00131$
00130$:
;main.c:336: targ=tonetarget[x];		//+FIRST_TONE_COLOR;	// added to just be unique?
	ld	a, #<(_tonetarget)
	add	a, -2 (ix)
	ld	c, a
	ld	a, #>(_tonetarget)
	adc	a, -1 (ix)
	ld	b, a
	ld	a, (bc)
	ld	-4 (ix), a
00131$:
;main.c:339: if ((songVol[idx]+4 < nOldVol[idx])||((targ!=nOldTarg[idx])&&(targ!=nOldTarg[arpvoice]))) {
	ld	bc, #_songVol+0
	ld	hl, (_idx)
	ld	h, #0x00
	add	hl, bc
	ld	c, (hl)
	ld	b, #0x00
	inc	bc
	inc	bc
	inc	bc
	inc	bc
	ld	a, #<(_nOldVol)
	ld	hl, #_idx
	add	a, (hl)
	ld	e, a
	ld	a, #>(_nOldVol)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	ld	e, a
	ld	d, #0x00
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	jp	PO, 00525$
	xor	a, #0x80
00525$:
	jp	M, 00142$
	ld	a, #<(_nOldTarg)
	ld	hl, #_idx
	add	a, (hl)
	ld	c, a
	ld	a, #>(_nOldTarg)
	adc	a, #0x00
	ld	b, a
	ld	a, (bc)
	ld	c, a
	ld	a, -4 (ix)
	sub	a, c
	jp	Z,00147$
	ld	a, #<(_nOldTarg)
	add	a, -5 (ix)
	ld	l, a
	ld	a, #>(_nOldTarg)
	adc	a, #0x00
	ld	h, a
	ld	a,-4 (ix)
	sub	a,(hl)
	jp	Z,00147$
00142$:
;main.c:341: if (sprpath[nextSprite].z == 0) {
	ld	bc, (_nextSprite)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	de, #_sprpath
	add	hl, de
	ld	de, #0x0009
	add	hl, de
	ld	a, (hl)
;main.c:348: idx_t y = nextSprite++;
	ld	iy, #_nextSprite
	ld	c, 0 (iy)
	inc	c
;main.c:341: if (sprpath[nextSprite].z == 0) {
	or	a, a
	jp	NZ, 00140$
;main.c:348: idx_t y = nextSprite++;
	ld	b, 0 (iy)
	ld	0 (iy), c
	ld	-1 (ix), b
;main.c:349: if (nextSprite >= MAX_SPRITES) nextSprite = 0;	// wraparound
	ld	a, 0 (iy)
	sub	a, #0x22
	jr	C, 00133$
	ld	0 (iy), #0x00
00133$:
;main.c:354: idx_t p = targ*2;
	ld	a, -4 (ix)
	add	a, a
	ld	-2 (ix), a
;main.c:353: if (idx == 3) {
	ld	a,(#_idx + 0)
	sub	a, #0x03
	jr	NZ, 00135$
;main.c:354: idx_t p = targ*2;
	ld	e, -2 (ix)
;main.c:355: tx=drums[p+1]*8;
	ld	a, e
	inc	a
	ld	c, a
	rla
	sbc	a, a
	ld	b, a
	ld	hl, #_drums
	add	hl, bc
	ld	a, (hl)
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
;main.c:356: ty=drums[p]*8;
	ld	hl, #_drums
	ld	d, #0x00
	add	hl, de
	ld	a, (hl)
	add	a, a
	add	a, a
	add	a, a
	ld	-3 (ix), a
;main.c:357: d = delayDrums[targ];
	ld	a, #<(_delayDrums)
	add	a, -4 (ix)
	ld	l, a
	ld	a, #>(_delayDrums)
	adc	a, #0x00
	ld	h, a
	ld	c, (hl)
	jr	00136$
00135$:
;main.c:359: idx_t p = targ * 2;
	ld	e, -2 (ix)
;main.c:360: tx=tones[p+1]*8;
	ld	a, e
	inc	a
	ld	c, a
	rla
	sbc	a, a
	ld	b, a
	ld	hl, #_tones
	add	hl, bc
	ld	a, (hl)
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
;main.c:361: ty=tones[p]*8-4;
	ld	hl, #_tones
	ld	d, #0x00
	add	hl, de
	ld	a, (hl)
	add	a, a
	add	a, a
	add	a, a
	add	a, #0xfc
	ld	-3 (ix), a
;main.c:362: d = delayTones[targ];
	ld	a, #<(_delayTones)
	add	a, -4 (ix)
	ld	l, a
	ld	a, #>(_delayTones)
	adc	a, #0x00
	ld	h, a
	ld	c, (hl)
00136$:
;main.c:370: sprpath[y].z = (DELAYTIME+1)-d;	// so it's always at least 1
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, de
	ld	de, #_sprpath
	add	hl, de
	ld	-2 (ix), l
	ld	-1 (ix), h
	ld	de, #0x0009
	add	hl, de
	ld	a, #0x1f
	sub	a, c
	ld	(hl), a
;main.c:387: sprpath[y].xstep = ((int)(tx-sx)<<4) / d;
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ld	de, #0x0004
	add	hl, de
	ld	e, b
	ld	d, #0x00
	ld	a, e
	add	a, #0x84
	ld	e, a
	ld	a, d
	adc	a, #0xff
	ld	d, a
	ex	de, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	ld	a, c
	rla
	sbc	a, a
	ld	b, a
	push	hl
	push	bc
	push	bc
	push	de
	call	__divsint
	pop	af
	pop	af
	ex	de, hl
	pop	bc
	pop	hl
	ld	(hl), e
	inc	hl
	ld	(hl), d
;main.c:388: sprpath[y].ystep = ((int)(ty-sy)<<4) / d;
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ld	de, #0x0006
	add	hl, de
	ld	a, -3 (ix)
	ld	d, #0x00
	add	a, #0x88
	ld	e, a
	ld	a, d
	adc	a, #0xff
	ld	d, a
	ex	de, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	push	hl
	push	bc
	push	bc
	push	de
	call	__divsint
	pop	af
	pop	af
	ex	de, hl
	pop	bc
	pop	hl
	ld	(hl), e
	inc	hl
	ld	(hl), d
;main.c:393: sprpath[y].zstep = -(d/2);	// half time up, half time down
	ld	a, -2 (ix)
	add	a, #0x08
	ld	e, a
	ld	a, -1 (ix)
	adc	a, #0x00
	ld	d, a
	ld	l, c
	ld	h, b
	bit	7, b
	jr	Z, 00226$
	ld	l, c
	ld	h, b
	inc	hl
00226$:
	sra	h
	rr	l
	xor	a, a
	sub	a, l
	ld	(de), a
;main.c:398: sprpath[y].x = (sx<<4);
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	ld	(hl), #0xc0
	inc	hl
	ld	(hl), #0x07
;main.c:399: sprpath[y].y = (sy<<4);	// need to set this after the delay
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	inc	hl
	inc	hl
	ld	(hl), #0x80
	inc	hl
	ld	(hl), #0x07
;main.c:402: sprpath[y].col=colorchan[idx]>>4;
	ld	a, -2 (ix)
	add	a, #0x0a
	ld	c, a
	ld	a, -1 (ix)
	adc	a, #0x00
	ld	b, a
	ld	a, #<(_colorchan)
	ld	hl, #_idx
	add	a, (hl)
	ld	e, a
	ld	a, #>(_colorchan)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	rlca
	rlca
	rlca
	rlca
	and	a, #0x0f
	ld	(bc), a
	jr	00147$
00140$:
;main.c:405: nextSprite++;
	ld	iy, #_nextSprite
;main.c:406: if (nextSprite >= MAX_SPRITES) nextSprite = 0;	// wraparound
	ld	0 (iy), c
	ld	a, c
	sub	a, #0x22
	jr	C, 00147$
	ld	0 (iy), #0x00
00147$:
;main.c:412: nOldVol[idx]=songVol[idx];
	ld	a, #<(_nOldVol)
	ld	hl, #_idx
	add	a, (hl)
	ld	c, a
	ld	a, #>(_nOldVol)
	adc	a, #0x00
	ld	b, a
	ld	hl, #_songVol+0
	ld	de, (_idx)
	ld	d, #0x00
	add	hl, de
	ld	a, (hl)
	ld	(bc), a
;main.c:413: nOldTarg[arpvoice]=targ;	// this may be idx or idx+4
	ld	a, -5 (ix)
	add	a, #<(_nOldTarg)
	ld	c, a
	ld	a, #0x00
	adc	a, #>(_nOldTarg)
	ld	b, a
	ld	a, -4 (ix)
	ld	(bc), a
;main.c:315: for (idx=0; idx<4; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x04
	jp	C, 00216$
;main.c:416: ++delaypos;
	ld	iy, #_delaypos
	inc	0 (iy)
;main.c:417: if (delaypos>=DELAYTIME) delaypos=0;
	ld	a, 0 (iy)
	sub	a, #0x1e
	jr	C, 00150$
	ld	0 (iy), #0x00
00150$:
;main.c:422: lastFade &= 0x1f;
	ld	a,(#_main_lastFade_262145_94 + 0)
	and	a, #0x1f
	ld	(_main_lastFade_262145_94+0), a
;main.c:423: for (idx=0; idx<4; ++idx) {
	ld	hl, #_idx
	ld	(hl), #0x04
00220$:
;main.c:424: colortab[lastFade]=colorfade[colortab[lastFade]>>4];
	ld	a, #<(_colortab)
	ld	hl, #_main_lastFade_262145_94
	add	a, (hl)
	ld	c, a
	ld	a, #>(_colortab)
	adc	a, #0x00
	ld	b, a
	ld	a, (bc)
	rlca
	rlca
	rlca
	rlca
	and	a, #0x0f
	add	a, #<(_colorfade)
	ld	e, a
	ld	a, #0x00
	adc	a, #>(_colorfade)
	ld	d, a
	ld	a, (de)
	ld	(bc), a
;main.c:425: ++lastFade;
	ld	hl, #_main_lastFade_262145_94
	inc	(hl)
	ld	iy, #_idx
	dec	0 (iy)
;main.c:423: for (idx=0; idx<4; ++idx) {
	ld	a, 0 (iy)
	or	a, a
	jr	NZ, 00220$
;main.c:429: for (idx=0; idx<3; idx++) {
	ld	0 (iy), #0x00
00221$:
;main.c:431: unsigned int x = delaytone[idx][delaypos];
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, hl
	ld	de, #_delaytone
	add	hl, de
	ld	iy, #_delaypos
	ld	a, 0 (iy)
	add	a, a
	ld	e, a
	ld	d, #0x00
	add	hl, de
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
;main.c:443: SOUND = x>>8;
	ld	-2 (ix), b
	ld	-1 (ix), #0x00
	ld	a, -2 (ix)
	out	(_SOUND), a
;main.c:444: SOUND = x&0xff;
	ld	a, c
	out	(_SOUND), a
;main.c:447: SOUND = delayvol[idx][delaypos];
	ld	de, (_idx)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, de
	add	hl, hl
	ex	de, hl
	ld	hl, #_delayvol
	add	hl, de
	ex	de, hl
;main.c:319: delayvol[idx][delaypos] = songVol[idx];
	ld	a, 0 (iy)
;main.c:447: SOUND = delayvol[idx][delaypos];
	add	a, e
	ld	e, a
	ld	a, #0x00
	adc	a, d
	ld	d, a
	ld	a, (de)
	out	(_SOUND), a
;main.c:449: if ((delayvol[idx][delaypos]&0x0f) < 0x0f) {
	ld	a, (de)
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
	ld	a, e
	sub	a, #0x0f
	ld	a, d
	rla
	ccf
	rra
	sbc	a, #0x80
	jr	NC, 00222$
;main.c:451: x=((x<<4)&0x03f0)|((x>>8)&0x0f);
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	and	a, #0xf0
	ld	c, a
	ld	a, h
	and	a, #0x03
	ld	b, a
	ld	a, -2 (ix)
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
	ld	a, c
	or	a, e
	ld	c, a
	ld	a, b
	or	a, d
	ld	b, a
;main.c:452: unsigned char set = tonetarget[x] + FIRST_TONE_COLOR;
	ld	hl, #_tonetarget
	add	hl, bc
	ld	a, (hl)
;main.c:453: colortab[set] = colorchan[idx];
	add	a, #<(_colortab)
	ld	c, a
	ld	a, #0x00
	adc	a, #>(_colortab)
	ld	b, a
	ld	a, #<(_colorchan)
	ld	hl, #_idx
	add	a, (hl)
	ld	e, a
	ld	a, #>(_colorchan)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	ld	(bc), a
00222$:
;main.c:429: for (idx=0; idx<3; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x03
	jp	C, 00221$
;main.c:460: unsigned int x = delaytone[3][delaypos];
	ld	iy, #_delaypos
	ld	a, 0 (iy)
	add	a, a
	add	a, #<((_delaytone + 0x00b4))
	ld	l, a
	ld	a, #0x00
	adc	a, #>((_delaytone + 0x00b4))
	ld	h, a
	ld	c, (hl)
	inc	hl
	ld	e, (hl)
;main.c:469: SOUND = x>>8;
	ld	a, e
	out	(_SOUND), a
;main.c:319: delayvol[idx][delaypos] = songVol[idx];
	ld	a, 0 (iy)
;main.c:472: SOUND = delayvol[3][delaypos];
	add	a, #<((_delayvol + 0x005a))
	ld	c, a
	ld	a, #0x00
	adc	a, #>((_delayvol + 0x005a))
	ld	b, a
	ld	a, (bc)
	out	(_SOUND), a
;main.c:474: if ((delayvol[3][delaypos]&0x0f) < 0x0f) {
	ld	a, (bc)
	and	a, #0x0f
	ld	b, a
	ld	c, #0x00
	ld	a, b
	sub	a, #0x0f
	ld	a, c
	rla
	ccf
	rra
	sbc	a, #0x80
	jr	NC, 00156$
;main.c:475: idx_t set = ((x>>8)&0x7)+FIRST_DRUM_COLOR;
	ld	a, e
	and	a, #0x07
	add	a, #0x16
;main.c:476: colortab[set] = colorchan[3];
	add	a, #<(_colortab)
	ld	c, a
	ld	a, #0x00
	adc	a, #>(_colortab)
	ld	b, a
	ld	a, (#_colorchan + 3)
	ld	(bc), a
00156$:
;main.c:481: vdpmemcpy(gColor, colortab, 32);
	ld	hl, #0x0020
	push	hl
	ld	hl, #_colortab
	push	hl
	ld	hl, (_gColor)
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;main.c:485: for (idx = 0; idx<MAX_SPRITES; idx++) {
	ld	hl, #_idx
	ld	(hl), #0x00
00223$:
;main.c:487: if (sprpath[idx].z == 0) continue;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	de, #_sprpath
	add	hl, de
	ld	a, l
	add	a, #0x09
	ld	e, a
	ld	a, h
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	ld	c, a
	or	a, a
	jp	Z, 00167$
;main.c:490: if (sprpath[idx].z > 0) {
	xor	a, a
	sub	a, c
	jp	PO, 00530$
	xor	a, #0x80
00530$:
	jp	P, 00163$
;main.c:491: --sprpath[idx].z;
	ld	a, c
	dec	a
	ld	(de), a
;main.c:493: if (sprpath[idx].z > 1) {
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	de, #_sprpath
	add	hl, de
	ld	a, l
	add	a, #0x09
	ld	c, a
	ld	a, h
	adc	a, #0x00
	ld	b, a
	ld	a, (bc)
	ld	e, a
	ld	a, #0x01
	sub	a, e
	jp	PO, 00531$
	xor	a, #0x80
00531$:
	jp	M, 00167$
;main.c:497: sprpath[idx].z = sprpath[idx].zstep;
	ld	de, #0x0008
	add	hl, de
	ld	a, (hl)
	ld	(bc), a
	jr	00164$
00163$:
;main.c:501: sprpath[idx].z += sprpath[idx].zstep;
	push	bc
	ld	bc, #0x0008
	add	hl, bc
	pop	bc
	ld	a, (hl)
	add	a, c
	ld	(de), a
00164$:
;main.c:505: if (sprpath[idx].z >= 0) {
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ex	de, hl
	ld	hl, #_sprpath
	add	hl, de
	ex	de, hl
	ld	hl, #0x0009
	add	hl, de
	bit	7, (hl)
	jr	NZ, 00166$
;main.c:506: sprpath[idx].z = 0;
	ld	(hl), #0x00
;main.c:507: continue;
	jr	00167$
00166$:
;main.c:512: ++sprpath[idx].zstep;
	ld	hl, #0x0008
	add	hl, de
	inc	(hl)
;main.c:518: sprpath[idx].x += sprpath[idx].xstep;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ld	a, #<(_sprpath)
	add	a, l
	ld	c, a
	ld	a, #>(_sprpath)
	adc	a, h
	ld	b, a
	ld	l, c
	ld	h, b
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	add	hl, de
	ex	de, hl
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
;main.c:519: sprpath[idx].y += sprpath[idx].ystep;
	ld	bc, (_idx)
	ld	b, #0x00
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
	ex	de, hl
	ld	hl, #_sprpath
	add	hl, de
	ex	de, hl
	ld	c, e
	ld	b, d
	inc	bc
	inc	bc
	ld	a, (bc)
	ld	-2 (ix), a
	inc	bc
	ld	a, (bc)
	ld	-1 (ix), a
	dec	bc
	ld	hl, #6
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, e
	add	a, -2 (ix)
	ld	e, a
	ld	a, d
	adc	a, -1 (ix)
	ld	d, a
	ld	a, e
	ld	(bc), a
	inc	bc
	ld	a, d
	ld	(bc), a
00167$:
;main.c:485: for (idx = 0; idx<MAX_SPRITES; idx++) {
	ld	iy, #_idx
	inc	0 (iy)
	ld	a, 0 (iy)
	sub	a, #0x22
	jp	C, 00223$
	jp	00169$
00171$:
;main.c:524: chain = (unsigned int)(*((volatile unsigned int*)(&flags[14])));
	ld	hl, (#(_flags + 0x000e) + 0)
	ld	c, l
;main.c:525: if (chain) {
	ld	a,h
	ld	b,a
	or	a, l
	jr	Z, 00175$
;main.c:534: memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
	push	bc
	ld	hl, #0x000e
	push	hl
	ld	hl, #_tramp
	push	hl
	ld	hl, #0x7000
	push	hl
	call	_memcpy
	pop	af
	pop	af
	pop	af
	pop	bc
;main.c:535: *((unsigned int*)0x7001) = chain;     // patch the pointer, chained should be on the stack
	ld	(0x7001), bc
;main.c:536: ((void(*)())0x7000)();                  // call the function, never return
	call	0x7000
00175$:
;main.c:540: } while (*pLoop);
	ld	a, (#(_flags + 0x000d) + 0)
	or	a, a
	jp	NZ, 00174$
;main.c:543: SOUND=0x9F;
	ld	a, #0x9f
	out	(_SOUND), a
;main.c:544: SOUND=0xBF;
	ld	a, #0xbf
	out	(_SOUND), a
;main.c:545: SOUND=0xDF;
	ld	a, #0xdf
	out	(_SOUND), a
;main.c:546: SOUND=0xFF;
	ld	a, #0xff
	out	(_SOUND), a
;main.c:549: return 0;
	ld	hl, #0x0000
;main.c:550: }
	ld	sp, ix
	pop	ix
	ret
_colorfade:
	.db #0x00	; 0
	.db #0x10	; 16
	.db #0xc0	; 192
	.db #0x20	; 32
	.db #0x10	; 16
	.db #0x40	; 64
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x60	; 96
	.db #0x80	; 128
	.db #0x10	; 16
	.db #0xa0	; 160
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0xe0	; 224
_colorchan:
	.db #0x90	; 144
	.db #0x30	; 48	'0'
	.db #0x50	; 80	'P'
	.db #0xb0	; 176
_ballsprite:
	.db #0x3c	; 60
	.db #0x7e	; 126
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0x7e	; 126
	.db #0x3c	; 60
_tonehit:
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
_drumhit:
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0x00	; 0
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0x00	; 0
	.db #0x80	; 128
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0x80	; 128
	.db #0x40	; 64
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0x80	; 128
	.db #0x00	; 0
_tonetarget:
	.db #0x00	; 0
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x14	; 20
	.db #0x13	; 19
	.db #0x12	; 18
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x10	; 16
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0e	; 14
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x14	; 20
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0d	; 13
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x0a	; 10
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
_tones:
	.db #0x0d	; 13
	.db #0x01	; 1
	.db #0x0b	; 11
	.db #0x03	; 3
	.db #0x09	; 9
	.db #0x05	; 5
	.db #0x0c	; 12
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x0a	; 10
	.db #0x09	; 9
	.db #0x05	; 5
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x0d	; 13
	.db #0x09	; 9
	.db #0x0f	; 15
	.db #0x04	; 4
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x11	; 17
	.db #0x0b	; 11
	.db #0x12	; 18
	.db #0x08	; 8
	.db #0x13	; 19
	.db #0x0a	; 10
	.db #0x15	; 21
	.db #0x05	; 5
	.db #0x15	; 21
	.db #0x0c	; 12
	.db #0x17	; 23
	.db #0x07	; 7
	.db #0x17	; 23
	.db #0x09	; 9
	.db #0x19	; 25
	.db #0x0b	; 11
	.db #0x1b	; 27
	.db #0x0d	; 13
	.db #0x1d	; 29
_drums:
	.db #0x13	; 19
	.db #0x07	; 7
	.db #0x11	; 17
	.db #0x09	; 9
	.db #0x0f	; 15
	.db #0x0b	; 11
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x13	; 19
	.db #0x17	; 23
	.db #0x11	; 17
	.db #0x15	; 21
	.db #0x0f	; 15
	.db #0x13	; 19
	.db #0x0f	; 15
	.db #0x17	; 23
_delayTones:
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x18	; 24
	.db #0x1e	; 30
	.db #0x18	; 24
	.db #0x1e	; 30
	.db #0x18	; 24
	.db #0x10	; 16
	.db #0x18	; 24
	.db #0x0e	; 14
	.db #0x18	; 24
	.db #0x16	; 22
	.db #0x0e	; 14
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x1e	; 30
	.db #0x16	; 22
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
_delayDrums:
	.db #0x19	; 25
	.db #0x11	; 17
	.db #0x09	; 9
	.db #0x11	; 17
	.db #0x17	; 23
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x0f	; 15
_charmap:
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0xf0	; 240
	.db #0xf1	; 241
	.db #0xf2	; 242
	.db #0xf3	; 243
	.db #0xf4	; 244
	.db #0xf5	; 245
	.db #0xf6	; 246
	.db #0xf7	; 247
	.db #0xf8	; 248
	.db #0xb0	; 176
	.db #0xf9	; 249
	.db #0xb1	; 177
	.db #0xfa	; 250
	.db #0xb8	; 184
	.db #0xfb	; 251
	.db #0xb9	; 185
	.db #0xc0	; 192
	.db #0xfc	; 252
	.db #0xfd	; 253
	.db #0xc1	; 193
	.db #0xfe	; 254
	.db #0xc8	; 200
	.db #0xff	; 255
	.db #0xe8	; 232
	.db #0xc9	; 201
	.db #0xe9	; 233
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_font:
	.db #0x20	; 32
	.db #0x50	; 80	'P'
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xf8	; 248
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0xf0	; 240
	.db #0x48	; 72	'H'
	.db #0x48	; 72	'H'
	.db #0x70	; 112	'p'
	.db #0x48	; 72	'H'
	.db #0x48	; 72	'H'
	.db #0xf0	; 240
	.db #0x00	; 0
	.db #0x30	; 48	'0'
	.db #0x48	; 72	'H'
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x48	; 72	'H'
	.db #0x30	; 48	'0'
	.db #0x00	; 0
	.db #0xe0	; 224
	.db #0x50	; 80	'P'
	.db #0x48	; 72	'H'
	.db #0x48	; 72	'H'
	.db #0x48	; 72	'H'
	.db #0x50	; 80	'P'
	.db #0xe0	; 224
	.db #0x00	; 0
	.db #0xf8	; 248
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0xf0	; 240
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0xf8	; 248
	.db #0x00	; 0
	.db #0xf8	; 248
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0xf0	; 240
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x88	; 136
	.db #0x80	; 128
	.db #0xb8	; 184
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x70	; 112	'p'
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xf8	; 248
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x70	; 112	'p'
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0x90	; 144
	.db #0xa0	; 160
	.db #0xc0	; 192
	.db #0xa0	; 160
	.db #0x90	; 144
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0xd8	; 216
	.db #0xa8	; 168
	.db #0xa8	; 168
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x70	; 112	'p'
	.db #0x00	; 0
	.db #0xf0	; 240
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xf0	; 240
	.db #0xa0	; 160
	.db #0x90	; 144
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x88	; 136
	.db #0x80	; 128
	.db #0x70	; 112	'p'
	.db #0x08	; 8
	.db #0x88	; 136
	.db #0x70	; 112	'p'
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x70	; 112	'p'
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xa8	; 168
	.db #0xa8	; 168
	.db #0xd8	; 216
	.db #0x88	; 136
	.db #0x00	; 0
_fontjl:
	.db #0x38	; 56	'8'
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x90	; 144
	.db #0x90	; 144
	.db #0x60	; 96
	.db #0x00	; 0
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0xf8	; 248
	.db #0x00	; 0
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_fontnp:
	.db #0x88	; 136
	.db #0xc8	; 200
	.db #0xc8	; 200
	.db #0xa8	; 168
	.db #0x98	; 152
	.db #0x98	; 152
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0xf0	; 240
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xf0	; 240
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x80	; 128
	.db #0x00	; 0
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_fontqt:
	.db #0x70	; 112	'p'
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xa8	; 168
	.db #0x90	; 144
	.db #0x68	; 104	'h'
	.db #0x00	; 0
	.db #0xf8	; 248
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x00	; 0
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_fontvy:
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x50	; 80	'P'
	.db #0x50	; 80	'P'
	.db #0x20	; 32
	.db #0x00	; 0
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x70	; 112	'p'
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x00	; 0
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_fontxz:
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x50	; 80	'P'
	.db #0x20	; 32
	.db #0x50	; 80	'P'
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x00	; 0
	.db #0xf8	; 248
	.db #0x08	; 8
	.db #0x10	; 16
	.db #0x20	; 32
	.db #0x40	; 64
	.db #0x80	; 128
	.db #0xf8	; 248
	.db #0x00	; 0
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_tramp:
	.db #0xc8	; 200
	.db #0x00	; 0
	.db #0x60	; 96
	.db #0x00	; 0
	.db #0xc0	; 192
	.db #0x20	; 32
	.db #0x60	; 96
	.db #0x06	; 6
	.db #0x05	; 5
	.db #0xc0	; 192
	.db #0xc0	; 192
	.db #0x10	; 16
	.db #0x04	; 4
	.db #0x50	; 80	'P'
_textout:
	.ascii "~~~~DATAHERE~~~~3"
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
_flags:
	.ascii "~~FLAGxxyySSSL"
	.db 0x00
	.db 0x00
	.ascii ":"
	.db 0x00
	.area _CODE
	.area _INITIALIZER
__xinit__nextSprite:
	.db #0x00	; 0
__xinit__frame:
	.db #0x00	; 0
	.area _CABS (ABS)

;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.0.0 #11528 (MINGW64)
;--------------------------------------------------------
	.module colecosnplay
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _kscan
	.globl _charset
	.globl _sprite
	.globl _vchar
	.globl _faster_hexprint
	.globl _vdpwaitvint
	.globl _vdpchar
	.globl _vdpmemset
	.globl _set_graphics
	.globl _SongLoop
	.globl _StartSong
;--------------------------------------------------------
; special function registers
;--------------------------------------------------------
_SOUND	=	0x00ff
_VDPRD	=	0x00be
_VDPST	=	0x00bf
_VDPWA	=	0x00bf
_VDPWD	=	0x00be
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _DATA
;--------------------------------------------------------
; ram data
;--------------------------------------------------------
	.area _INITIALIZED
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
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;colecosnplay.c:18: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	push	af
	push	af
;colecosnplay.c:20: set_graphics(VDP_MODE1_SPRMAG);
	ld	a, #0x01
	push	af
	inc	sp
	call	_set_graphics
	inc	sp
;colecosnplay.c:21: charset();
	call	_charset
;colecosnplay.c:22: vdpmemset(gColor, 0xe0, 32);
	ld	hl, #0x0020
	push	hl
	ld	a, #0xe0
	push	af
	inc	sp
	ld	hl, (_gColor)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;colecosnplay.c:35: for (unsigned char idx=0; idx<4; ++idx) {
	ld	c, #0x00
00119$:
;colecosnplay.c:36: sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
	ld	a,c
	cp	a,#0x04
	jr	NC,00101$
	rrca
	rrca
	and	a, #0xc0
	add	a, #0x10
	ld	h, a
	ld	d, c
	inc	d
	inc	d
	inc	d
	inc	d
	ld	a, c
	add	a, #0x31
	ld	b, a
	push	bc
	push	hl
	inc	sp
	ld	a, #0xa8
	push	af
	inc	sp
	ld	e, b
	push	de
	ld	a, c
	push	af
	inc	sp
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecosnplay.c:35: for (unsigned char idx=0; idx<4; ++idx) {
	inc	c
	jr	00119$
00101$:
;colecosnplay.c:39: VDPWD = 0xd0;
	ld	a, #0xd0
	out	(_VDPWD), a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnplay.c:45: StartSong((unsigned char*)mysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_mysong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
00127$:
;colecosnplay.c:49: vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
	call	_vdpwaitvint
;colecosnplay.c:50: CALL_PLAYER_SN;
	call	_SongLoop
;colecosnplay.c:54: VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);
	ld	iy, #_gImage
	ld	a, 0 (iy)
	add	a, #0xe0
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
;colecosnplay.c:57: for (unsigned char idx=0; idx<4; ++idx) {
	ld	bc, #_songVol+0
	xor	a, a
	ld	-1 (ix), a
00122$:
	ld	a, -1 (ix)
	sub	a, #0x04
	jr	NC,00102$
;colecosnplay.c:58: int row = songNote[idx];
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, hl
	ld	de, #_songNote
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
;colecosnplay.c:14: faster_hexprint(x>>8);
	ld	h, d
	ld	a, h
	rlc	a
	sbc	a, a
	ld	l, a
	push	bc
	push	de
	push	hl
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	de
	pop	bc
;colecosnplay.c:15: faster_hexprint(x&0xff);
	ld	a, e
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecosnplay.c:61: VDPWD = ' ';
	ld	a, #0x20
	out	(_VDPWD), a
;colecosnplay.c:62: faster_hexprint(songVol[idx]);
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecosnplay.c:63: VDPWD = ' ';
	ld	a, #0x20
	out	(_VDPWD), a
;colecosnplay.c:57: for (unsigned char idx=0; idx<4; ++idx) {
	inc	-1 (ix)
	jr	00122$
00102$:
;colecosnplay.c:67: for (unsigned char idx=0; idx<4; ++idx) {
	ld	c, #0x00
00125$:
	ld	a, c
	sub	a, #0x04
	jp	NC, 00106$
;colecosnplay.c:68: int row = songNote[idx];
	ld	-4 (ix), c
	xor	a, a
	ld	-3 (ix), a
	pop	hl
	push	hl
	add	hl, hl
	ld	de, #_songNote
	add	hl, de
	ld	a, (hl)
	ld	-2 (ix), a
	inc	hl
	ld	a, (hl)
;colecosnplay.c:73: row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
	ld	-1 (ix), a
	and	a, #0x0f
	ld	e, a
	rlc	a
	sbc	a, a
	ld	d, a
;colecosnplay.c:70: if (idx != 3) {
	ld	a, c
	sub	a, #0x03
	jr	Z,00104$
;colecosnplay.c:73: row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
	ld	l, -2 (ix)
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	or	a, e
	ld	l, a
	ld	a, h
	or	a, d
	ld	h, a
	add	hl, hl
	push	bc
	ld	de, #0x0009
	push	de
	push	hl
	call	__divsint
	pop	af
	pop	af
	pop	bc
	jr	00105$
00104$:
;colecosnplay.c:76: row = ((row&0x0f00)>>8)*11;
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, de
00105$:
;colecosnplay.c:78: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
	ld	b, l
	pop	hl
	push	hl
	add	hl, hl
	add	hl, hl
	ex	de,hl
	ld	iy, #_gSprite
	ld	a, 0 (iy)
	add	a, e
	ld	e, a
	ld	a, 1 (iy)
	adc	a, d
	ld	d, a
	push	bc
	push	bc
	inc	sp
	push	de
	call	_vdpchar
	pop	af
	inc	sp
	pop	bc
;colecosnplay.c:81: row = songVol[idx]&0xf;
	ld	hl, #_songVol
	ld	b, #0x00
	add	hl, bc
	ld	a, (hl)
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
;colecosnplay.c:82: vchar(7, (idx<<3)+6, 32, row);
	ld	a, c
	add	a, a
	add	a, a
	add	a, a
	add	a, #0x06
	ld	b, a
	push	bc
	push	de
	push	de
	ld	a, #0x20
	push	af
	inc	sp
	push	bc
	inc	sp
	ld	a, #0x07
	push	af
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	de
	pop	bc
;colecosnplay.c:83: vchar(row+7, (idx<<3)+6, 43, 15-row);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, de
	ld	a, e
	add	a, #0x07
	ld	d, a
	push	bc
	push	hl
	ld	a, #0x2b
	push	af
	inc	sp
	ld	c, d
	push	bc
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecosnplay.c:67: for (unsigned char idx=0; idx<4; ++idx) {
	inc	c
	jp	00125$
00106$:
;colecosnplay.c:87: if (!isSNPlaying) {
	ld	hl, (#_songNote + 6)
	bit	0, l
	jp	NZ,00127$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x03
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnplay.c:89: kscan(1);
	ld	a, #0x01
	push	af
	inc	sp
	call	_kscan
	inc	sp
;colecosnplay.c:90: if (KSCAN_KEY == JOY_FIRE) {
	ld	a,(#_KSCAN_KEY + 0)
	sub	a, #0x12
	jp	NZ,00127$
;colecosnplay.c:91: StartSong((unsigned char*)mysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_mysong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnplay.c:92: VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
;colecosnplay.c:97: return 0;
;colecosnplay.c:98: }
	jp	00127$
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

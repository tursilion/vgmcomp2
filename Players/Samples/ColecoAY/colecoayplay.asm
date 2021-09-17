;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module colecoayplay
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
	.globl _ay_SongLoop
	.globl _ay_StartSong
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
;colecoayplay.c:17: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	push	af
;colecoayplay.c:19: set_graphics(VDP_MODE1_SPRMAG);
	ld	a, #0x01
	push	af
	inc	sp
	call	_set_graphics
	inc	sp
;colecoayplay.c:20: charset();
	call	_charset
;colecoayplay.c:21: vdpmemset(gColor, 0xe0, 32);
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
;colecoayplay.c:24: for (unsigned char idx=0; idx<4; ++idx) {
	ld	b, #0x00
00125$:
;colecoayplay.c:25: sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
	ld	a,b
	cp	a,#0x04
	jr	NC, 00101$
	rrca
	rrca
	and	a, #0xc0
	add	a, #0x10
	ld	h, a
	ld	d, b
	inc	d
	inc	d
	inc	d
	inc	d
	ld	a, b
	add	a, #0x31
	push	bc
	push	hl
	inc	sp
	ld	h, #0xa8
	ld	l, d
	push	hl
	push	af
	inc	sp
	push	bc
	inc	sp
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecoayplay.c:24: for (unsigned char idx=0; idx<4; ++idx) {
	inc	b
	jr	00125$
00101$:
;colecoayplay.c:28: VDPWD = 0xd0;
	ld	a, #0xd0
	out	(_VDPWD), a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecoayplay.c:34: ay_StartSong((unsigned char*)mysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_mysong
	push	hl
	call	_ay_StartSong
	pop	af
	inc	sp
00133$:
;colecoayplay.c:38: vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
	call	_vdpwaitvint
;colecoayplay.c:39: CALL_PLAYER_AY;
	call	_ay_SongLoop
;colecoayplay.c:43: VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);
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
;colecoayplay.c:46: for (unsigned char idx=0; idx<4; ++idx) {
	ld	c, #0x00
00128$:
	ld	a, c
	sub	a, #0x04
	jr	NC, 00102$
;colecoayplay.c:47: int row = ay_songNote[idx];
	ld	de, #_ay_songNote+0
	ld	a, c
	ld	h, #0x00
	ld	l, a
	add	hl, hl
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
;colecoayplay.c:13: faster_hexprint(x>>8);
	ld	b, d
	ld	a, b
	rlca
	push	bc
	push	de
	push	bc
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	de
	pop	bc
;colecoayplay.c:14: faster_hexprint(x&0xff);
	ld	a, e
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecoayplay.c:50: VDPWD = ' ';
	ld	a, #0x20
	out	(_VDPWD), a
;colecoayplay.c:51: faster_hexprint(ay_songVol[idx]);
	ld	de, #_ay_songVol+0
	ld	l, c
	ld	h, #0x00
	add	hl, de
	ld	a, (hl)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecoayplay.c:52: VDPWD = ' ';
	ld	a, #0x20
	out	(_VDPWD), a
;colecoayplay.c:46: for (unsigned char idx=0; idx<4; ++idx) {
	inc	c
	jr	00128$
00102$:
;colecoayplay.c:56: for (unsigned char idx=0; idx<3; ++idx) {
	ld	-1 (ix), #0
00131$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jr	NC, 00103$
;colecoayplay.c:58: unsigned int row = ay_songNote[idx] / 6;
	ld	bc, #_ay_songNote+0
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, bc
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	push	de
	ld	hl, #0x0006
	push	hl
	push	bc
	call	__divuint
	pop	af
	pop	af
	pop	de
;colecoayplay.c:59: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
	ld	-2 (ix), l
	ex	de, hl
	add	hl, hl
	add	hl, hl
	ex	de, hl
	ld	hl, (_gSprite)
	add	hl, de
	ld	a, -2 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;colecoayplay.c:62: row = ay_songVol[idx]&0xf;
	ld	bc, #_ay_songVol+0
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	ld	c, #0x00
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
;colecoayplay.c:63: vchar(7, (idx<<3)+6, 32, row);
	ld	a, -1 (ix)
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
	ld	c, #0x07
	push	bc
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	de
	pop	bc
;colecoayplay.c:64: vchar(row+7, (idx<<3)+6, 43, 15-row);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, de
	ld	a, e
	add	a, #0x07
	push	hl
	ld	h, #0x2b
	ld	l, b
	push	hl
	push	af
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
;colecoayplay.c:56: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jp	00131$
00103$:
;colecoayplay.c:68: unsigned int row = ay_songNote[3];
	ld	hl, (#(_ay_songNote + 0x0006) + 0)
;colecoayplay.c:71: row = ((row&0x0f00)>>8)*11;
	ld	c, #0x00
	ld	a, h
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	add	hl, de
	add	hl, hl
	add	hl, de
	ld	b, l
;colecoayplay.c:72: vdpchar(gSprite+(3<<2), row);    // first value in each sprite is row
	ld	hl, (_gSprite)
	ld	de, #0x000c
	add	hl, de
	push	bc
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;colecoayplay.c:75: unsigned char mix = ay_songVol[3];
	ld	a, (#(_ay_songVol + 0x0003) + 0)
;colecoayplay.c:76: if ((mix&0x20) == 0) row = ay_songVol[2];
	bit	5, a
	jr	NZ, 00111$
	ld	a, (#(_ay_songVol + 0x0002) + 0)
	ld	c, a
	jr	00112$
00111$:
;colecoayplay.c:77: else if (0 == (mix&0x10)) row = ay_songVol[1];
	bit	4, a
	jr	NZ, 00108$
	ld	a, (#(_ay_songVol + 0x0001) + 0)
	ld	c, a
	jr	00112$
00108$:
;colecoayplay.c:78: else if (0 == (mix&0x08)) row = ay_songVol[0];
	bit	3, a
	jr	NZ, 00105$
	ld	a, (#_ay_songVol + 0)
	ld	c, a
	jr	00112$
00105$:
;colecoayplay.c:79: else row = 0;
	ld	bc, #0x0000
00112$:
;colecoayplay.c:80: row &= 0xf;
	ld	a, c
	and	a, #0x0f
	ld	c, a
	ld	b, #0x00
;colecoayplay.c:83: vchar(7, (3<<3)+6, 32, row);
	push	bc
	push	bc
	ld	de, #0x201e
	push	de
	ld	a, #0x07
	push	af
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecoayplay.c:84: vchar(row+7, (3<<3)+6, 43, 15-row);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, bc
	ld	a, c
	add	a, #0x07
	push	hl
	ld	h, #0x2b
	push	hl
	inc	sp
	ld	h, #0x1e
	push	hl
	inc	sp
	push	af
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
;colecoayplay.c:88: if (!isAYPlaying) {
	ld	hl, (#(_ay_songNote + 0x0006) + 0)
	bit	0, l
	jp	NZ,00133$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x03
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecoayplay.c:90: kscan(1);
	ld	a, #0x01
	push	af
	inc	sp
	call	_kscan
	inc	sp
;colecoayplay.c:91: if (KSCAN_KEY == JOY_FIRE) {
	ld	a,(#_KSCAN_KEY + 0)
	sub	a, #0x12
	jp	NZ,00133$
;colecoayplay.c:92: ay_StartSong((unsigned char*)mysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_mysong
	push	hl
	call	_ay_StartSong
	pop	af
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecoayplay.c:93: VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
	jp	00133$
;colecoayplay.c:98: return 0;
;colecoayplay.c:99: }
	pop	af
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

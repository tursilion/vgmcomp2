;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module colecosnplay
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _PlayerUnitTest
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
;colecosnplay.c:26: PlayerUnitTest();
	call	_PlayerUnitTest
00121$:
;colecosnplay.c:28: kscan(1);
	ld	a, #0x01
	push	af
	inc	sp
	call	_kscan
	inc	sp
;colecosnplay.c:29: if (KSCAN_KEY == JOY_FIRE) break;
	ld	a,(#_KSCAN_KEY + 0)
	sub	a, #0x12
	jr	NZ, 00121$
;colecosnplay.c:31: vdpmemset(gImage, ' ', 768);
	ld	hl, #0x0300
	push	hl
	ld	a, #0x20
	push	af
	inc	sp
	ld	hl, (_gImage)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;colecosnplay.c:35: for (unsigned char idx=0; idx<4; ++idx) {
	ld	b, #0x00
00124$:
;colecosnplay.c:36: sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*64+16);
	ld	a,b
	cp	a,#0x04
	jr	NC, 00104$
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
;colecosnplay.c:35: for (unsigned char idx=0; idx<4; ++idx) {
	inc	b
	jr	00124$
00104$:
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
00132$:
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
	ld	c, #0x00
00127$:
	ld	a, c
	sub	a, #0x04
	jr	NC, 00105$
;colecosnplay.c:58: int row = songNote[idx];
	ld	de, #_songNote+0
	ld	a, c
	ld	h, #0x00
	ld	l, a
	add	hl, hl
	add	hl, de
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
;colecosnplay.c:14: faster_hexprint(x>>8);
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
	ld	de, #_songVol+0
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
;colecosnplay.c:63: VDPWD = ' ';
	ld	a, #0x20
	out	(_VDPWD), a
;colecosnplay.c:57: for (unsigned char idx=0; idx<4; ++idx) {
	inc	c
	jr	00127$
00105$:
;colecosnplay.c:67: for (unsigned char idx=0; idx<4; ++idx) {
	ld	-1 (ix), #0
00130$:
	ld	a, -1 (ix)
	sub	a, #0x04
	jp	NC, 00109$
;colecosnplay.c:68: int row = songNote[idx];
	ld	bc, #_songNote+0
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, bc
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
;colecosnplay.c:73: row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
	ld	a, h
	and	a, #0x0f
	ld	c, a
	rlca
	sbc	a, a
	ld	b, a
;colecosnplay.c:70: if (idx != 3) {
	ld	a, -1 (ix)
	sub	a, #0x03
	jr	Z, 00107$
;colecosnplay.c:73: row = ((((row&0x0f00)>>8)|((row&0x00ff)<<4))*2) / 9;
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, l
	or	a, c
	ld	l, a
	ld	a, h
	or	a, b
	ld	h, a
	add	hl, hl
	push	de
	ld	bc, #0x0009
	push	bc
	push	hl
	call	__divsint
	pop	af
	pop	af
	pop	de
	jr	00108$
00107$:
;colecosnplay.c:76: row = ((row&0x0f00)>>8)*11;
	ld	l, c
	ld	h, b
	add	hl, hl
	add	hl, hl
	add	hl, bc
	add	hl, hl
	add	hl, bc
00108$:
;colecosnplay.c:78: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
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
;colecosnplay.c:81: row = songVol[idx]&0xf;
	ld	bc, #_songVol+0
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	ld	c, #0x00
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
;colecosnplay.c:82: vchar(7, (idx<<3)+6, 32, row);
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
;colecosnplay.c:83: vchar(row+7, (idx<<3)+6, 43, 15-row);
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
;colecosnplay.c:67: for (unsigned char idx=0; idx<4; ++idx) {
	inc	-1 (ix)
	jp	00130$
00109$:
;colecosnplay.c:87: if (!isSNPlaying) {
	ld	hl, (#(_songNote + 0x0006) + 0)
	bit	0, l
	jp	NZ,00132$
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
	jp	NZ,00132$
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
	jp	00132$
;colecosnplay.c:97: return 0;
;colecosnplay.c:98: }
	pop	af
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

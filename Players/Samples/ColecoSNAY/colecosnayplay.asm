;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module colecosnayplay
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _kscan
	.globl _charset
	.globl _sprite
	.globl _vchar
	.globl _vdpwaitvint
	.globl _vdpchar
	.globl _vdpmemset
	.globl _set_graphics
	.globl _ay_SongLoop
	.globl _ay_StartSong
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
;colecosnayplay.c:19: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	push	af
;colecosnayplay.c:21: set_graphics(VDP_MODE1_SPRMAG);
	ld	a, #0x01
	push	af
	inc	sp
	call	_set_graphics
	inc	sp
;colecosnayplay.c:22: charset();
	call	_charset
;colecosnayplay.c:23: vdpmemset(gColor, 0xe0, 32);
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
;colecosnayplay.c:24: SOUND = 0xff;       // make sure SN noise is muted
	ld	a, #0xff
	out	(_SOUND), a
;colecosnayplay.c:27: for (unsigned char idx=0; idx<6; ++idx) {
	ld	b, #0x00
00113$:
;colecosnayplay.c:28: sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*40+16);
	ld	a,b
	cp	a,#0x06
	jr	NC, 00101$
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, a
	add	a, a
	add	a, a
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
;colecosnayplay.c:27: for (unsigned char idx=0; idx<6; ++idx) {
	inc	b
	jr	00113$
00101$:
;colecosnayplay.c:31: VDPWD = 0xd0;
	ld	a, #0xd0
	out	(_VDPWD), a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnayplay.c:37: ay_StartSong((unsigned char*)aysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_aysong
	push	hl
	call	_ay_StartSong
	pop	af
	inc	sp
;colecosnayplay.c:38: StartSong((unsigned char*)snsong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_snsong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
00121$:
;colecosnayplay.c:42: vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
	call	_vdpwaitvint
;colecosnayplay.c:43: CALL_PLAYER_AY;
	call	_ay_SongLoop
;colecosnayplay.c:44: CALL_PLAYER_SN;
	call	_SongLoop
;colecosnayplay.c:81: for (unsigned char idx=0; idx<3; ++idx) {
	ld	-1 (ix), #0
00116$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jr	NC, 00102$
;colecosnayplay.c:82: unsigned char row = songNote[idx]&0xff; // MSB is in the LSB position
	ld	bc, #_songNote+0
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, bc
	ld	a, (hl)
	inc	hl
	ld	c, (hl)
;colecosnayplay.c:83: row <<= 1;
	add	a, a
	ld	-2 (ix), a
;colecosnayplay.c:84: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
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
;colecosnayplay.c:87: row = songVol[idx]&0xf;
	ld	bc, #_songVol+0
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	and	a, #0x0f
	ld	c, a
;colecosnayplay.c:88: unsigned char col = (idx*5)+4;
	ld	a, -1 (ix)
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, #0x04
	ld	b, a
;colecosnayplay.c:89: vchar(7, col, 32, row);
	ld	e, c
	ld	d, #0x00
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
;colecosnayplay.c:90: vchar(row+7, col, 43, 15-row);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, de
	ld	a, c
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
;colecosnayplay.c:81: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jr	00116$
00102$:
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	ld	-1 (ix), #0
00119$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jp	NC, 00103$
;colecosnayplay.c:95: unsigned char row = ay_songNote[idx] >> 3;
	ld	bc, #_ay_songNote+0
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
	ld	b, #0x03
00173$:
	srl	h
	rr	l
	djnz	00173$
	ld	-2 (ix), l
;colecosnayplay.c:96: vdpchar(gSprite+((idx+3)<<2), row);    // first value in each sprite is row
	inc	de
	inc	de
	inc	de
	ld	a, d
	ld	l, e
	ld	h, a
	add	hl, hl
	add	hl, hl
	ld	c, l
	ld	b, h
	ld	hl, (_gSprite)
	add	hl, bc
	ld	a, -2 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;colecosnayplay.c:99: unsigned char t = 15-(ay_songVol[idx]&0xf);
	ld	bc, #_ay_songVol+0
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, bc
	ld	a, (hl)
	and	a, #0x0f
	ld	c, a
	ld	a, #0x0f
	sub	a, c
	ld	c, a
;colecosnayplay.c:100: unsigned char col = (idx*5)+4+15;
	ld	a, -1 (ix)
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, #0x13
	ld	b, a
;colecosnayplay.c:101: vchar(7, col, 32, t);
	ld	e, c
	ld	d, #0x00
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
;colecosnayplay.c:102: vchar(t+7, col, 43, 15-t);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, de
	ld	a, c
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
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jp	00119$
00103$:
;colecosnayplay.c:106: if (!isAYPlaying) {
	ld	hl, (#(_ay_songNote + 0x0006) + 0)
	bit	0, l
	jp	NZ,00121$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x03
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnayplay.c:108: kscan(1);
	ld	a, #0x01
	push	af
	inc	sp
	call	_kscan
	inc	sp
;colecosnayplay.c:109: if (KSCAN_KEY == JOY_FIRE) {
	ld	a,(#_KSCAN_KEY + 0)
	sub	a, #0x12
	jp	NZ,00121$
;colecosnayplay.c:110: ay_StartSong((unsigned char*)aysong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_aysong
	push	hl
	call	_ay_StartSong
	pop	af
	inc	sp
;colecosnayplay.c:111: StartSong((unsigned char*)snsong, 0);
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_snsong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnayplay.c:112: VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
	jp	00121$
;colecosnayplay.c:117: return 0;
;colecosnayplay.c:118: }
	pop	af
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

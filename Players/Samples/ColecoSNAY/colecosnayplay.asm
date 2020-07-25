;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.0.0 #11528 (MINGW64)
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
	push	af
	dec	sp
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
	ld	c, #0x00
00113$:
;colecosnayplay.c:28: sprite(idx, '1'+idx, COLOR_DKBLUE+idx, 21*8, idx*40+16);
	ld	a,c
	cp	a,#0x06
	jr	NC,00101$
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, a
	add	a, a
	add	a, a
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
;colecosnayplay.c:27: for (unsigned char idx=0; idx<6; ++idx) {
	inc	c
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
	xor	a, a
	ld	-1 (ix), a
00116$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jp	NC, 00102$
;colecosnayplay.c:82: unsigned char row = songNote[idx]&0xff; // MSB is in the LSB position
	ld	a, -1 (ix)
	ld	-5 (ix), a
	xor	a, a
	ld	-4 (ix), a
	ld	a, -5 (ix)
	ld	-3 (ix), a
	ld	a, -4 (ix)
	ld	-2 (ix), a
	sla	-3 (ix)
	rl	-2 (ix)
	ld	a, #<(_songNote)
	add	a, -3 (ix)
	ld	c, a
	ld	a, #>(_songNote)
	adc	a, -2 (ix)
	ld	b, a
	ld	a, (bc)
;colecosnayplay.c:83: row <<= 1;
	add	a, a
	ld	d, a
;colecosnayplay.c:84: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
	pop	hl
	push	hl
	add	hl, hl
	add	hl, hl
	ld	c, l
	ld	b, h
	ld	iy, #_gSprite
	ld	a, 0 (iy)
	add	a, c
	ld	c, a
	ld	a, 1 (iy)
	adc	a, b
	ld	b, a
	push	de
	inc	sp
	push	bc
	call	_vdpchar
	pop	af
	inc	sp
;colecosnayplay.c:87: row = songVol[idx]&0xf;
	ld	a, #<(_songVol)
	add	a, -1 (ix)
	ld	c, a
	ld	a, #>(_songVol)
	adc	a, #0x00
	ld	b, a
	ld	a, (bc)
	and	a, #0x0f
	ld	e, a
;colecosnayplay.c:88: unsigned char col = (idx*5)+4;
	ld	a, -1 (ix)
	ld	c, a
	add	a, a
	add	a, a
	add	a, c
	add	a, #0x04
	ld	d, a
;colecosnayplay.c:89: vchar(7, col, 32, row);
	ld	c, e
	ld	b, #0x00
	push	bc
	push	de
	push	bc
	ld	a, #0x20
	push	af
	inc	sp
	push	de
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
;colecosnayplay.c:90: vchar(row+7, col, 43, 15-row);
	ld	hl, #0x000f
	cp	a, a
	sbc	hl, bc
	ld	a, e
	add	a, #0x07
	ld	b, a
	push	hl
	ld	a, #0x2b
	push	af
	inc	sp
	ld	e, b
	push	de
	call	_vchar
	pop	af
	pop	af
	inc	sp
;colecosnayplay.c:81: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jp	00116$
00102$:
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	ld	c, #0x00
00119$:
	ld	a, c
	sub	a, #0x03
	jp	NC, 00103$
;colecosnayplay.c:95: unsigned char row = ay_songNote[idx] >> 3;
	ld	e, c
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	push	de
	ld	de, #_ay_songNote
	add	hl, de
	pop	de
	ld	b, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, b
	ld	b, #0x03
00175$:
	srl	h
	rr	l
	djnz	00175$
	ld	b, l
;colecosnayplay.c:96: vdpchar(gSprite+((idx+3)<<2), row);    // first value in each sprite is row
	inc	de
	inc	de
	inc	de
	ld	h, d
	ld	l, e
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
;colecosnayplay.c:99: unsigned char t = 15-(ay_songVol[idx]&0xf);
	ld	hl, #_ay_songVol
	ld	b, #0x00
	add	hl, bc
	ld	a, (hl)
	and	a, #0x0f
	ld	b, a
	ld	a, #0x0f
	sub	a, b
	ld	b, a
;colecosnayplay.c:100: unsigned char col = (idx*5)+4+15;
	ld	a, c
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, #0x13
	ld	h, a
;colecosnayplay.c:101: vchar(7, col, 32, t);
	ld	e, b
	ld	d, #0x00
	push	hl
	push	bc
	push	de
	push	de
	ld	a, #0x20
	push	af
	inc	sp
	push	hl
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
	pop	hl
;colecosnayplay.c:102: vchar(t+7, col, 43, 15-t);
	ld	a, #0x0f
	sub	a, e
	ld	e, a
	ld	a, #0x00
	sbc	a, d
	ld	d, a
	ld	a, b
	add	a, #0x07
	ld	b, a
	push	bc
	push	de
	ld	a, #0x2b
	push	af
	inc	sp
	push	hl
	inc	sp
	push	bc
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	inc	c
	jp	00119$
00103$:
;colecosnayplay.c:106: if (!isAYPlaying) {
	ld	hl, (#_ay_songNote + 6)
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
;colecosnayplay.c:117: return 0;
;colecosnayplay.c:118: }
	jp	00121$
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

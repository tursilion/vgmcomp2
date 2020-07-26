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
	.globl _faster_hexprint
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
	ld	c, #0x00
00125$:
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
	jr	00125$
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
	ld	bc, #_aysong+0
	ld	e, c
	ld	d, b
	push	bc
	xor	a, a
	push	af
	inc	sp
	push	de
	call	_ay_StartSong
	pop	af
	inc	sp
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_snsong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
	pop	bc
00139$:
;colecosnayplay.c:42: vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
	push	bc
	call	_vdpwaitvint
	call	_ay_SongLoop
	call	_SongLoop
	pop	bc
;colecosnayplay.c:50: VDP_SET_ADDRESS_WRITE(VDP_SCREEN_POS(23,0)+gImage);
	ld	iy, #_gImage
	ld	a, 0 (iy)
	add	a, #0xe0
	ld	d, a
	ld	a, 1 (iy)
	adc	a, #0x02
	ld	e, a
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, d
	out	(_VDPWA), a
	ld	a, e
	or	a, #0x40
	out	(_VDPWA), a
;colecosnayplay.c:53: for (unsigned char idx=0; idx<3; ++idx) {
	xor	a, a
	ld	-1 (ix), a
00128$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jr	NC,00105$
;colecosnayplay.c:54: int row = songNote[idx];
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, hl
	ld	de, #_songNote
	add	hl, de
	ld	d, (hl)
	inc	hl
	ld	e, (hl)
;colecosnayplay.c:15: faster_hexprint(x>>8);
	ld	h, e
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
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecosnayplay.c:57: unsigned char t = songVol[idx]&0x0f;
	ld	a, #<(_songVol)
	add	a, -1 (ix)
	ld	e, a
	ld	a, #>(_songVol)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	and	a, #0x0f
	ld	d, a
;colecosnayplay.c:59: VDPWD = t + '0' + 7;
	ld	e, d
;colecosnayplay.c:58: if (t > 9) {
	ld	a, #0x09
	sub	a, d
	jr	NC,00103$
;colecosnayplay.c:59: VDPWD = t + '0' + 7;
	ld	a, e
	add	a, #0x37
	out	(_VDPWD), a
	jr	00129$
00103$:
;colecosnayplay.c:61: VDPWD = t + '0';
	ld	a, e
	add	a, #0x30
	out	(_VDPWD), a
00129$:
;colecosnayplay.c:53: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jr	00128$
00105$:
;colecosnayplay.c:66: for (unsigned char idx=0; idx<3; ++idx) {
	xor	a, a
	ld	-1 (ix), a
00131$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jr	NC,00109$
;colecosnayplay.c:67: unsigned int row = ay_songNote[idx];
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, hl
	ld	de, #_ay_songNote
	add	hl, de
	ld	d, (hl)
	inc	hl
	ld	e, (hl)
;colecosnayplay.c:15: faster_hexprint(x>>8);
	ld	h, e
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
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;colecosnayplay.c:70: unsigned char t = ay_songVol[idx]&0x0f;
	ld	a, #<(_ay_songVol)
	add	a, -1 (ix)
	ld	e, a
	ld	a, #>(_ay_songVol)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	and	a, #0x0f
	ld	d, a
;colecosnayplay.c:72: VDPWD = t + '0' + 7;
	ld	e, d
;colecosnayplay.c:71: if (t > 9) {
	ld	a, #0x09
	sub	a, d
	jr	NC,00107$
;colecosnayplay.c:72: VDPWD = t + '0' + 7;
	ld	a, e
	add	a, #0x37
	out	(_VDPWD), a
	jr	00132$
00107$:
;colecosnayplay.c:74: VDPWD = t + '0';
	ld	a, e
	add	a, #0x30
	out	(_VDPWD), a
00132$:
;colecosnayplay.c:66: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jr	00131$
00109$:
;colecosnayplay.c:81: for (unsigned char idx=0; idx<3; ++idx) {
	xor	a, a
	ld	-1 (ix), a
00134$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jp	NC, 00110$
;colecosnayplay.c:82: unsigned char row = songNote[idx]&0xff; // MSB is in the LSB position
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	push	de
	ld	de, #_songNote
	add	hl, de
	pop	de
	ld	a, (hl)
;colecosnayplay.c:83: row <<= 1;
	add	a, a
	ld	-2 (ix), a
;colecosnayplay.c:84: vdpchar(gSprite+(idx<<2), row);    // first value in each sprite is row
	ld	l, e
	ld	h, d
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
	ld	a, -2 (ix)
	push	af
	inc	sp
	push	de
	call	_vdpchar
	pop	af
	inc	sp
	pop	bc
;colecosnayplay.c:87: row = songVol[idx]&0xf;
	ld	a, #<(_songVol)
	add	a, -1 (ix)
	ld	e, a
	ld	a, #>(_songVol)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	and	a, #0x0f
	ld	d, a
;colecosnayplay.c:88: unsigned char col = (idx*5)+4;
	ld	a, -1 (ix)
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, #0x04
	ld	e, a
;colecosnayplay.c:89: vchar(7, col, 32, row);
	ld	l, d
	ld	h, #0x00
	push	hl
	push	bc
	push	de
	push	hl
	ld	d,#0x20
	push	de
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
;colecosnayplay.c:90: vchar(row+7, col, 43, 15-row);
	ld	a, #0x0f
	sub	a, l
	ld	l, a
	ld	a, #0x00
	sbc	a, h
	ld	h, a
	ld	a, d
	add	a, #0x07
	ld	d, a
	push	bc
	push	hl
	ld	a, #0x2b
	push	af
	inc	sp
	ld	a, e
	push	af
	inc	sp
	push	de
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecosnayplay.c:81: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jp	00134$
00110$:
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	xor	a, a
	ld	-1 (ix), a
00137$:
	ld	a, -1 (ix)
	sub	a, #0x03
	jp	NC, 00111$
;colecosnayplay.c:95: unsigned char row = ay_songNote[idx] >> 3;
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	ld	a, #<(_ay_songNote)
	add	a, l
	ld	l, a
	ld	a, #>(_ay_songNote)
	adc	a, h
	ld	h, a
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, #0x03
00223$:
	srl	h
	rr	l
	dec	a
	jr	NZ,00223$
	ld	-2 (ix), l
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
	ld	a, -2 (ix)
	push	af
	inc	sp
	push	de
	call	_vdpchar
	pop	af
	inc	sp
	pop	bc
;colecosnayplay.c:99: unsigned char t = 15-(ay_songVol[idx]&0xf);
	ld	a, #<(_ay_songVol)
	add	a, -1 (ix)
	ld	e, a
	ld	a, #>(_ay_songVol)
	adc	a, #0x00
	ld	d, a
	ld	a, (de)
	and	a, #0x0f
	ld	e, a
	ld	a, #0x0f
	sub	a, e
	ld	d, a
;colecosnayplay.c:100: unsigned char col = (idx*5)+4+15;
	ld	a, -1 (ix)
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	add	a, #0x13
	ld	e, a
;colecosnayplay.c:101: vchar(7, col, 32, t);
	ld	l, d
	ld	h, #0x00
	push	hl
	push	bc
	push	de
	push	hl
	ld	d,#0x20
	push	de
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
	sub	a, l
	ld	l, a
	ld	a, #0x00
	sbc	a, h
	ld	h, a
	ld	a, d
	add	a, #0x07
	ld	d, a
	push	bc
	push	hl
	ld	a, #0x2b
	push	af
	inc	sp
	ld	a, e
	push	af
	inc	sp
	push	de
	inc	sp
	call	_vchar
	pop	af
	pop	af
	inc	sp
	pop	bc
;colecosnayplay.c:93: for (unsigned char idx=0; idx<3; ++idx) {
	inc	-1 (ix)
	jp	00137$
00111$:
;colecosnayplay.c:106: if (!isAYPlaying) {
	ld	hl, (#_ay_songNote + 6)
	bit	0, l
	jp	NZ,00139$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x03
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnayplay.c:108: kscan(1);
	push	bc
	ld	a, #0x01
	push	af
	inc	sp
	call	_kscan
	inc	sp
	pop	bc
;colecosnayplay.c:109: if (KSCAN_KEY == JOY_FIRE) {
	ld	a,(#_KSCAN_KEY + 0)
	sub	a, #0x12
	jp	NZ,00139$
;colecosnayplay.c:110: ay_StartSong((unsigned char*)aysong, 0);
	ld	e, c
	ld	d, b
	push	bc
	xor	a, a
	push	af
	inc	sp
	push	de
	call	_ay_StartSong
	pop	af
	inc	sp
	xor	a, a
	push	af
	inc	sp
	ld	hl, #_snsong
	push	hl
	call	_StartSong
	pop	af
	inc	sp
	pop	bc
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x02
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;colecosnayplay.c:112: VDP_SET_REGISTER(VDP_REG_COL,COLOR_MEDGREEN);
	jp	00139$
;colecosnayplay.c:117: return 0;
;colecosnayplay.c:118: }
	pop	af
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

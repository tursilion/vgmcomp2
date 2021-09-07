;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module blinken
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _drawtext
	.globl _updateRow
	.globl _ballz
	.globl _memcpy
	.globl _ay_SongLoop
	.globl _ay_StartSong
	.globl _SongLoop
	.globl _StartSong
	.globl _charsetlc
	.globl _vdpchar
	.globl _vdpmemcpy
	.globl _vdpmemset
	.globl _set_graphics_raw
	.globl _secondSong
	.globl _firstSong
	.globl _scrnbuf
	.globl _colortable
	.globl _ball
	.globl _flags
	.globl _textout
	.globl _tramp
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
_scrnbuf::
	.ds 8
_firstSong::
	.ds 2
_secondSong::
	.ds 2
_updateRow_scrnpos_65536_50:
	.ds 2
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
;blinken.c:126: static unsigned int scrnpos = 32;
	ld	hl, #0x0020
	ld	(_updateRow_scrnpos_65536_50), hl
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;blinken.c:106: void ballz(unsigned int n) {
;	---------------------------------
; Function ballz
; ---------------------------------
_ballz::
	call	___sdcc_enter_ix
	dec	sp
;blinken.c:110: n = n % 704 + 32;
	ld	hl, #0x02c0
	push	hl
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	push	hl
	call	__moduint
	pop	af
	pop	af
	ld	de, #0x0020
	add	hl, de
	ld	4 (ix), l
	ld	5 (ix), h
;blinken.c:111: vdpchar(n, 64);
	ld	a, #0x40
	push	af
	inc	sp
	ld	l, 4 (ix)
	ld	h, 5 (ix)
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;blinken.c:114: unsigned int row  = n&0x3e0;    // keep the shift
	ld	a, 4 (ix)
	and	a, #0xe0
	ld	c, a
	ld	a, 5 (ix)
	and	a, #0x03
	ld	b, a
;blinken.c:115: unsigned int row24=0x2e0-row;   // address of last row - faster than shifting?
	ld	hl, #0x02e0
	cp	a, a
	sbc	hl, bc
	ex	de, hl
;blinken.c:116: idx_t col  = n&0x01f;
	ld	a, 4 (ix)
	and	a, #0x1f
	ld	l, a
;blinken.c:117: idx_t col32=32-col;
	ld	h, l
	ld	a, #0x20
	sub	a, h
	ld	-1 (ix), a
;blinken.c:119: vdpchar(row24+col,64);
	ld	h, #0x00
	add	hl, de
	push	bc
	push	de
	ld	a, #0x40
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;blinken.c:120: vdpchar(row+col32,64);
	ld	l, -1 (ix)
	ld	h, #0x00
	ld	a, c
	add	a, l
	ld	c, a
	ld	a, b
	adc	a, h
	ld	b, a
	push	hl
	push	de
	ld	a, #0x40
	push	af
	inc	sp
	push	bc
	call	_vdpchar
	pop	af
	inc	sp
	pop	de
	pop	hl
;blinken.c:121: vdpchar(row24+col32,64);
	add	hl, de
	ld	a, #0x40
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;blinken.c:122: }
	inc	sp
	pop	ix
	ret
_tramp:
	.db #0x3a	; 58
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xc3	; 195
	.db #0x00	; 0
	.db #0xc0	; 192
_textout:
	.ascii "~~~~DATAHERE~~~~2"
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
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
_ball:
	.db #0x00	; 0
	.db #0x3c	; 60
	.db #0x7e	; 126
	.db #0x7e	; 126
	.db #0x7e	; 126
	.db #0x3c	; 60
	.db #0x00	; 0
	.db 0x00
_colortable:
	.db #0x41	; 65	'A'
	.db #0x51	; 81	'Q'
	.db #0x61	; 97	'a'
	.db #0x81	; 129
	.db #0x91	; 145
	.db #0xc1	; 193
	.db #0x21	; 33
	.db #0x31	; 49	'1'
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
;blinken.c:124: void updateRow() {
;	---------------------------------
; Function updateRow
; ---------------------------------
_updateRow::
;blinken.c:128: unsigned char *p = scrnbuf;
	ld	bc, #_scrnbuf
;blinken.c:130: VDP_SET_ADDRESS(scrnpos);
	ld	de, (_updateRow_scrnpos_65536_50)
;d:/work/coleco/libti99coleco/vdp.h:57: inline void VDP_SET_ADDRESS(unsigned int x)							{	VDPWA=((x)&0xff); VDPWA=((x)>>8); VDP_SAFE_DELAY();	}
	ld	a, e
	out	(_VDPWA), a
	ld	e, #0x00
	ld	a, d
	out	(_VDPWA), a
;d:/work/coleco/libti99coleco/vdp.h:42: __endasm;
	nop
	nop
	nop
	nop
	nop
;blinken.c:133: *p = VDPRD;
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
	or	a, a
	jr	Z, 00102$
	add	a, #0xfc
	ld	(bc), a
00102$:
;blinken.c:135: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:136: if (*p) *p-=4;
	or	a, a
	jr	Z, 00104$
	add	a, #0xfc
	ld	(bc), a
00104$:
;blinken.c:137: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:138: if (*p) *p-=4;
	or	a, a
	jr	Z, 00106$
	add	a, #0xfc
	ld	(bc), a
00106$:
;blinken.c:139: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:140: if (*p) *p-=4;
	or	a, a
	jr	Z, 00108$
	add	a, #0xfc
	ld	(bc), a
00108$:
;blinken.c:141: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:142: if (*p) *p-=4;
	or	a, a
	jr	Z, 00110$
	add	a, #0xfc
	ld	(bc), a
00110$:
;blinken.c:143: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:144: if (*p) *p-=4;
	or	a, a
	jr	Z, 00112$
	add	a, #0xfc
	ld	(bc), a
00112$:
;blinken.c:145: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:146: if (*p) *p-=4;
	or	a, a
	jr	Z, 00114$
	add	a, #0xfc
	ld	(bc), a
00114$:
;blinken.c:147: *(++p) = VDPRD;
	inc	bc
	in	a, (_VDPRD)
	ld	(bc), a
;blinken.c:134: if (*p) *p-=4;
	ld	a, (bc)
;blinken.c:148: if (*p) *p-=4;
	or	a, a
	jr	Z, 00116$
	add	a, #0xfc
	ld	(bc), a
00116$:
;blinken.c:150: vdpmemcpy(scrnpos, scrnbuf, 8);
	ld	hl, #0x0008
	push	hl
	ld	hl, #_scrnbuf
	push	hl
	ld	hl, (_updateRow_scrnpos_65536_50)
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;blinken.c:152: scrnpos+=8;
	ld	hl, #_updateRow_scrnpos_65536_50
	ld	a, (hl)
	add	a, #0x08
	ld	(hl), a
	jr	NC, 00168$
	inc	hl
	inc	(hl)
00168$:
;blinken.c:153: if (scrnpos >= 736) scrnpos = 32;
	ld	iy, #_updateRow_scrnpos_65536_50
	ld	a, 0 (iy)
	sub	a, #0xe0
	ld	a, 1 (iy)
	sbc	a, #0x02
	ret	C
	ld	hl, #0x0020
	ld	(_updateRow_scrnpos_65536_50), hl
;blinken.c:154: }
	ret
;blinken.c:156: void drawtext(unsigned int scrn, const unsigned char *txt, idx_t cnt) {
;	---------------------------------
; Function drawtext
; ---------------------------------
_drawtext::
	call	___sdcc_enter_ix
;blinken.c:159: VDP_SET_ADDRESS_WRITE(scrn);
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, 4 (ix)
	ld	c, 5 (ix)
	out	(_VDPWA), a
	ld	a, c
	or	a, #0x40
	out	(_VDPWA), a
;blinken.c:160: while (cnt--) {
	ld	c, 8 (ix)
00104$:
	ld	a, c
	dec	c
	or	a, a
	jr	Z, 00108$
;blinken.c:161: if (*txt != 32) {
	ld	e, 6 (ix)
	ld	d, 7 (ix)
	ld	a, (de)
;blinken.c:162: VDPWD = *(txt++);
	inc	de
;blinken.c:161: if (*txt != 32) {
	cp	a, #0x20
	jr	Z, 00102$
;blinken.c:162: VDPWD = *(txt++);
	out	(_VDPWD), a
	ld	6 (ix), e
	ld	7 (ix), d
	jr	00104$
00102$:
;blinken.c:164: VDPWD = 0;
	ld	a, #0x00
	out	(_VDPWD), a
;blinken.c:165: ++txt;
	ld	6 (ix), e
	ld	7 (ix), d
	jr	00104$
00108$:
;blinken.c:168: }
	pop	ix
	ret
;blinken.c:170: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	push	af
	dec	sp
;blinken.c:174: unsigned char x = set_graphics_raw(VDP_SPR_8x8);	// set graphics mode with 8x8 sprites
	xor	a, a
	push	af
	inc	sp
	call	_set_graphics_raw
	ld	-2 (ix), l
	inc	sp
;blinken.c:175: vdpchar(gSprite, 0xd0);					// all sprites disabled
	ld	a, #0xd0
	push	af
	inc	sp
	ld	hl, (_gSprite)
	push	hl
	call	_vdpchar
;blinken.c:176: vdpmemset(gImage, 0, 768);				// clear screen to char 0
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
;blinken.c:178: vdpmemcpy(gColor, colortable, 16);      // color table
	inc	sp
	ld	hl,#0x0010
	ex	(sp),hl
	ld	hl, #_colortable
	push	hl
	ld	hl, (_gColor)
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;blinken.c:180: charsetlc();
	call	_charsetlc
;blinken.c:181: vdpmemset(gPattern, 0, 8);				// char 0 is blank
	ld	hl, #0x0008
	push	hl
	xor	a, a
	push	af
	inc	sp
	ld	hl, (_gPattern)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
;blinken.c:183: for (idx=4; idx<65; idx+=4) {
	ld	-1 (ix), #0x04
	inc	sp
00131$:
;blinken.c:184: vdpmemcpy(gPattern+(int)(idx)*8, ball, 8);
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	c, l
	ld	b, h
	ld	hl, (_gPattern)
	add	hl, bc
	ld	de, #0x0008
	push	de
	ld	de, #_ball
	push	de
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;blinken.c:183: for (idx=4; idx<65; idx+=4) {
	ld	a, -1 (ix)
	add	a, #0x04
	ld	-1 (ix), a
	sub	a, #0x41
	jr	C, 00131$
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x01
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
	ld	a, -2 (ix)
	out	(_VDPWA), a
	ld	a, #0x81
	out	(_VDPWA), a
;blinken.c:194: drawtext(gImage, textout, 32);
	ld	a, #0x20
	push	af
	inc	sp
	ld	hl, #_textout
	push	hl
	ld	hl, (_gImage)
	push	hl
	call	_drawtext
	pop	af
	pop	af
	inc	sp
;blinken.c:195: drawtext(gImage+(23*32), textout+32, 32);
	ld	bc, #_textout + 32
	ld	hl, (_gImage)
	ld	de, #0x02e0
	add	hl, de
	ld	a, #0x20
	push	af
	inc	sp
	push	bc
	push	hl
	call	_drawtext
	pop	af
	pop	af
	inc	sp
;blinken.c:198: firstSong = *((unsigned int*)&flags[6]);
	ld	hl, #(_flags + 0x0006)
	ld	a, (hl)
	ld	(_firstSong+0), a
	inc	hl
	ld	a, (hl)
	ld	(_firstSong+1), a
;blinken.c:199: secondSong = *((unsigned int*)&flags[8]);
	ld	hl, #(_flags + 0x0008)
	ld	a, (hl)
	ld	(_secondSong+0), a
	inc	hl
	ld	a, (hl)
	ld	(_secondSong+1), a
;blinken.c:212: volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];
;blinken.c:214: do {
00126$:
;blinken.c:216: if (0 != firstSong) {
	ld	iy, #_firstSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00103$
;blinken.c:217: StartSong((unsigned char*)firstSong, 0);
	ld	hl, (_firstSong)
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_StartSong
	pop	af
	inc	sp
00103$:
;blinken.c:219: if (0 != secondSong) {
	ld	iy, #_secondSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00105$
;blinken.c:224: ay_StartSong((unsigned char*)secondSong, 0);
	ld	hl, (_secondSong)
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_ay_StartSong
	pop	af
	inc	sp
00105$:
;blinken.c:229: done = 0;
	ld	-3 (ix), #0
;blinken.c:230: while (!done) {
00121$:
	ld	a, -3 (ix)
	or	a, a
	jp	NZ, 00123$
;blinken.c:231: done = 1;
	ld	-3 (ix), #0x01
;blinken.c:241: while ((vdpLimi&0x80) == 0) {
00106$:
	ld	a,(#_vdpLimi + 0)
	rlca
	jr	C, 00108$
;blinken.c:242: updateRow();
	call	_updateRow
	jr	00106$
00108$:
;blinken.c:244: VDP_CLEAR_VBLANK;
	ld	hl, #_vdpLimi
	ld	(hl), #0x00
	in	a, (_VDPST)
	ld	(_VDP_STATUS_MIRROR+0), a
;blinken.c:246: if (0 != firstSong) {
	ld	iy, #_firstSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00112$
;blinken.c:247: if (isSNPlaying) {
	ld	hl, #(_songNote + 0x0006)
	ld	a, (hl)
	ld	-2 (ix), a
	inc	hl
	ld	a, (hl)
	ld	-1 (ix), a
	bit	0, -2 (ix)
	jr	Z, 00112$
;blinken.c:248: CALL_PLAYER_SN;
	call	_SongLoop
;blinken.c:249: done = 0;
	ld	-3 (ix), #0
00112$:
;blinken.c:252: if (0 != secondSong) {
	ld	iy, #_secondSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00116$
;blinken.c:260: if (isAYPlaying) {
	ld	hl, (#(_ay_songNote + 0x0006) + 0)
	bit	0, l
	jr	Z, 00116$
;blinken.c:261: CALL_PLAYER_AY;
	call	_ay_SongLoop
;blinken.c:262: done = 0;
	ld	-3 (ix), #0
00116$:
;blinken.c:269: if (0 != firstSong) {
	ld	iy, #_firstSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jp	Z, 00118$
;blinken.c:272: ballz(((songNote[0]&0xff)<<8)|((songNote[0]&0x0f00)>>4)|(songVol[0]&0xf));
	ld	bc, (#_songNote + 0)
	ld	e, #0x00
	ld	hl, (#_songNote + 0)
	ld	l, #0x00
	ld	a, h
	and	a, #0x0f
	ld	h, a
	ld	b, #0x04
00224$:
	srl	h
	rr	l
	djnz	00224$
	ld	a, e
	or	a, l
	ld	e, a
	ld	a, c
	or	a, h
	ld	d, a
	ld	a, (#_songVol + 0)
	and	a, #0x0f
	ld	c, #0x00
	or	a, e
	ld	e, a
	ld	a, c
	or	a, d
	ld	d, a
	push	de
	call	_ballz
	pop	af
;blinken.c:273: ballz(((songNote[1]&0xff)<<8)|((songNote[1]&0x0f00)>>4)|(songVol[1]&0xf));
	ld	bc, (#(_songNote + 0x0002) + 0)
	ld	e, #0x00
	ld	hl, (#(_songNote + 0x0002) + 0)
	ld	l, #0x00
	ld	a, h
	and	a, #0x0f
	ld	h, a
	ld	b, #0x04
00225$:
	srl	h
	rr	l
	djnz	00225$
	ld	a, e
	or	a, l
	ld	e, a
	ld	a, c
	or	a, h
	ld	d, a
	ld	a, (#(_songVol + 0x0001) + 0)
	and	a, #0x0f
	ld	c, #0x00
	or	a, e
	ld	e, a
	ld	a, c
	or	a, d
	ld	d, a
	push	de
	call	_ballz
	pop	af
;blinken.c:274: ballz(((songNote[2]&0xff)<<8)|((songNote[2]&0x0f00)>>4)|(songVol[2]&0xf));
	ld	bc, (#(_songNote + 0x0004) + 0)
	ld	e, #0x00
	ld	hl, (#(_songNote + 0x0004) + 0)
	ld	l, #0x00
	ld	a, h
	and	a, #0x0f
	ld	h, a
	ld	b, #0x04
00226$:
	srl	h
	rr	l
	djnz	00226$
	ld	a, e
	or	a, l
	ld	e, a
	ld	a, c
	or	a, h
	ld	d, a
	ld	a, (#(_songVol + 0x0002) + 0)
	and	a, #0x0f
	ld	c, #0x00
	or	a, e
	ld	e, a
	ld	a, c
	or	a, d
	ld	d, a
	push	de
	call	_ballz
	pop	af
;blinken.c:276: ballz((songNote[3]&0xff00)|(songVol[3]&0xf));
	ld	bc, (#(_songNote + 0x0006) + 0)
	ld	c, #0x00
	ld	a, (#(_songVol + 0x0003) + 0)
	and	a, #0x0f
	ld	e, #0x00
	or	a, c
	ld	c, a
	ld	a, e
	or	a, b
	ld	b, a
	push	bc
	call	_ballz
	pop	af
00118$:
;blinken.c:278: if (0 != secondSong) {
	ld	iy, #_secondSong
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jp	Z, 00121$
;blinken.c:291: ballz(((ay_songNote[0]&0xff)>>4)|((ay_songNote[0]&0xf)<<12)|(ay_songVol[0]>>4));
	ld	de, (#_ay_songNote + 0)
	ld	d, #0x00
	ld	b, #0x04
00227$:
	srl	d
	rr	e
	djnz	00227$
	ld	hl, (#_ay_songNote + 0)
	ld	a, l
	and	a, #0x0f
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
	ld	a, #0x00
	or	a, e
	ld	c, a
	ld	a, b
	or	a, d
	ld	b, a
	ld	a, (#_ay_songVol + 0)
	rlca
	rlca
	rlca
	rlca
	and	a, #0x0f
	ld	e, #0x00
	or	a, c
	ld	c, a
	ld	a, e
	or	a, b
	ld	b, a
	push	bc
	call	_ballz
	pop	af
;blinken.c:292: ballz(((ay_songNote[1]&0xff)>>4)|((ay_songNote[1]&0xf)<<12)|(ay_songVol[1]>>4));
	ld	de, (#(_ay_songNote + 0x0002) + 0)
	ld	d, #0x00
	ld	b, #0x04
00228$:
	srl	d
	rr	e
	djnz	00228$
	ld	hl, (#(_ay_songNote + 0x0002) + 0)
	ld	a, l
	and	a, #0x0f
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
	ld	a, #0x00
	or	a, e
	ld	c, a
	ld	a, b
	or	a, d
	ld	b, a
	ld	a, (#(_ay_songVol + 0x0001) + 0)
	rlca
	rlca
	rlca
	rlca
	and	a, #0x0f
	ld	e, #0x00
	or	a, c
	ld	c, a
	ld	a, e
	or	a, b
	ld	b, a
	push	bc
	call	_ballz
	pop	af
;blinken.c:293: ballz(((ay_songNote[2]&0xff)>>4)|((ay_songNote[2]&0xf)<<12)|(ay_songVol[2]>>4));
	ld	de, (#(_ay_songNote + 0x0004) + 0)
	ld	d, #0x00
	ld	b, #0x04
00229$:
	srl	d
	rr	e
	djnz	00229$
	ld	hl, (#(_ay_songNote + 0x0004) + 0)
	ld	a, l
	and	a, #0x0f
	add	a, a
	add	a, a
	add	a, a
	add	a, a
	ld	b, a
	ld	a, #0x00
	or	a, e
	ld	c, a
	ld	a, b
	or	a, d
	ld	b, a
	ld	a, (#(_ay_songVol + 0x0002) + 0)
	rlca
	rlca
	rlca
	rlca
	and	a, #0x0f
	ld	e, #0x00
	or	a, c
	ld	c, a
	ld	a, e
	or	a, b
	ld	b, a
	push	bc
	call	_ballz
	pop	af
	jp	00121$
00123$:
;blinken.c:300: chain = (unsigned int)(*((volatile unsigned int*)(&flags[14])));
	ld	hl, (#(_flags + 0x000e) + 0)
	ld	c, l
;blinken.c:301: if (chain) {
	ld	a,h
	ld	b,a
	or	a, l
	jr	Z, 00127$
;blinken.c:310: memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
	push	bc
	ld	hl, #0x0006
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
;blinken.c:311: *((unsigned int*)0x7001) = chain;       // patch the pointer, chained should be on the stack
	ld	(0x7001), bc
;blinken.c:312: ((void(*)())0x7000)();                  // call the function, never return
	call	0x7000
00127$:
;blinken.c:315: } while (*pLoop);
	ld	a, (#(_flags + 0x000d) + 0)
	or	a, a
	jp	NZ, 00126$
;blinken.c:326: __endasm;
	rst	0x00
;blinken.c:330: return 2;
	ld	hl, #0x0002
;blinken.c:332: }
	ld	sp, ix
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

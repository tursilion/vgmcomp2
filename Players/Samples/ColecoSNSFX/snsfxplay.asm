;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.0.0 #11528 (MINGW64)
;--------------------------------------------------------
	.module snsfxplay
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _joystfast
	.globl _kscanfast
	.globl _StartSfx
	.globl _sfx_SongLoop
	.globl _sfx_StopSong
	.globl _SongLoop
	.globl _StopSong
	.globl _StartSong
	.globl _charsetlc
	.globl _faster_hexprint
	.globl _putstring
	.globl _vdpwaitvint
	.globl _set_text_raw
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
;snsfxplay.c:12: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	dec	sp
;snsfxplay.c:13: unsigned char oldkey = 0;					// prevent key repeat
	ld	c, #0x00
;snsfxplay.c:14: unsigned char isPlaying = 0;				// remember if we are playing a song so we can call mute on end
	xor	a, a
	ld	-1 (ix), a
;snsfxplay.c:16: int x = set_text_raw(); 					// good old fashioned text mode
	push	bc
	call	_set_text_raw
	ld	a, l
	pop	bc
	ld	b, a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0xf4
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;snsfxplay.c:20: charsetlc();
	push	bc
	call	_charsetlc
	ld	hl, #___str_0
	push	hl
	call	_putstring
	ld	hl, #___str_1
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_2
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_3
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_4
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_5
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_6
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_7
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_8
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_9
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_10
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_11
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_12
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_13
	ex	(sp),hl
	call	_putstring
	ld	hl, #___str_14
	ex	(sp),hl
	call	_putstring
	pop	af
	pop	bc
;snsfxplay.c:39: VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display
	ld	a, b
	out	(_VDPWA), a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x81
	out	(_VDPWA), a
;d:/work/coleco/libti99coleco/sound.h:50: inline void MUTE_SOUND()					{ SOUND=TONE1_VOL|0x0f; SOUND=TONE2_VOL|0x0f; SOUND=TONE3_VOL|0x0f; SOUND=NOISE_VOL|0x0f; }
	ld	a, #0x9f
	out	(_SOUND), a
	ld	a, #0xbf
	out	(_SOUND), a
	ld	a, #0xdf
	out	(_SOUND), a
	ld	a, #0xff
	out	(_SOUND), a
;snsfxplay.c:42: MUTE_SOUND();	// a good idea to also mute the sound registers
00159$:
;snsfxplay.c:49: vdpwaitvint();      // wait for an interrupt with ints enabled - console clears it
	push	bc
	call	_vdpwaitvint
	pop	bc
;snsfxplay.c:50: CALL_PLAYER_SFX;    // must be first
	ld	de, (#(_sfx_songNote + 0x0006) + 0)
	bit	0, e
	jp	Z,00113$
	ld	hl, #(_songNote + 0x0006) + 0
	ld	b, (hl)
	ld	a, e
	and	a, #0x01
	ld	e, a
	ld	((_sfx_songNote + 0x0006)), de
	push	bc
	call	_sfx_SongLoop
	pop	bc
	ld	de, (#(_sfx_songNote + 0x0006) + 0)
	bit	0, e
	jr	NZ,00110$
	bit	7, b
	jr	Z,00102$
	ld	de, (#_songNote + 0)
	ld	a, d
	out	(_SOUND), a
	ld	a, e
	out	(_SOUND), a
	ld	a, (#_songVol+0)
	out	(_SOUND), a
00102$:
	bit	6, b
	jr	Z,00104$
	ld	de, (#_songNote + 2)
	ld	a, d
	out	(_SOUND), a
	ld	a, e
	out	(_SOUND), a
	ld	a, (#_songVol+1)
	out	(_SOUND), a
00104$:
	bit	5, b
	jr	Z,00106$
	ld	de, (#_songNote + 4)
	ld	a, d
	out	(_SOUND), a
	ld	a, e
	out	(_SOUND), a
	ld	a, (#_songVol+2)
	out	(_SOUND), a
00106$:
	bit	4, b
	jr	Z,00108$
	ld	a,(#(_songNote + 0x0006) + 0)
	out	(_SOUND), a
	ld	a, (#_songVol+3)
	out	(_SOUND), a
00108$:
	ld	hl, (#(_songNote + 0x0006) + 0)
	ld	e, #0x00
	ld	d, h
	ld	a, l
	and	a, #0x01
	ld	b, #0x00
	or	a, e
	ld	e, a
	ld	a, b
	or	a, d
	ld	d, a
	ld	((_songNote + 0x0006)), de
	jr	00113$
00110$:
	ld	hl, (#(_songNote + 0x0006) + 0)
	ld	a, #0x00
	ld	b, h
	res	0, e
	ld	d, #0x00
	or	a, e
	ld	e, a
	ld	a, b
	or	a, d
	ld	d, a
	ld	a, l
	and	a, #0x01
	ld	l, a
	ld	h, #0x00
	ld	a, e
	or	a, l
	ld	e, a
	ld	a, d
	or	a, h
	ld	d, a
	ld	((_songNote + 0x0006)), de
00113$:
;snsfxplay.c:51: CALL_PLAYER_SN;
	push	bc
	call	_SongLoop
	pop	bc
;snsfxplay.c:58: if (isPlaying) {
	ld	a, -1 (ix)
	or	a, a
	jr	Z,00117$
;snsfxplay.c:59: if (!isSNPlaying) {
	ld	hl, (#(_songNote + 0x0006) + 0)
	bit	0, l
	jr	NZ,00117$
;snsfxplay.c:60: isPlaying=0;
	xor	a, a
	ld	-1 (ix), a
;d:/work/coleco/libti99coleco/sound.h:50: inline void MUTE_SOUND()					{ SOUND=TONE1_VOL|0x0f; SOUND=TONE2_VOL|0x0f; SOUND=TONE3_VOL|0x0f; SOUND=NOISE_VOL|0x0f; }
	ld	a, #0x9f
	out	(_SOUND), a
	ld	a, #0xbf
	out	(_SOUND), a
	ld	a, #0xdf
	out	(_SOUND), a
	ld	a, #0xff
	out	(_SOUND), a
;snsfxplay.c:61: MUTE_SOUND();
00117$:
;snsfxplay.c:66: KSCAN_KEY = 0xff;
	ld	hl,#_KSCAN_KEY + 0
	ld	(hl), #0xff
;snsfxplay.c:68: joystfast(KSCAN_MODE_LEFT);
	push	bc
	ld	a, #0x01
	push	af
	inc	sp
	call	_joystfast
	inc	sp
	pop	bc
;snsfxplay.c:69: if (KSCAN_JOYX == JOY_LEFT) {
	ld	a,(#_KSCAN_JOYX + 0)
	sub	a, #0xfc
	jr	NZ,00130$
;snsfxplay.c:70: KSCAN_KEY = 'C';
	ld	iy, #_KSCAN_KEY
	ld	0 (iy), #0x43
	jr	00131$
00130$:
;snsfxplay.c:71: } else if (KSCAN_JOYX == JOY_RIGHT) {
	ld	a,(#_KSCAN_JOYX + 0)
	sub	a, #0x04
	jr	NZ,00127$
;snsfxplay.c:72: KSCAN_KEY = 'D';
	ld	iy, #_KSCAN_KEY
	ld	0 (iy), #0x44
	jr	00131$
00127$:
;snsfxplay.c:73: } else if (KSCAN_JOYY == JOY_UP) {
	ld	a,(#_KSCAN_JOYY + 0)
	sub	a, #0x04
	jr	NZ,00124$
;snsfxplay.c:74: KSCAN_KEY = 'A';
	ld	iy, #_KSCAN_KEY
	ld	0 (iy), #0x41
	jr	00131$
00124$:
;snsfxplay.c:75: } else if (KSCAN_JOYY == JOY_DOWN) {
	ld	a,(#_KSCAN_JOYY + 0)
	sub	a, #0xfc
	jr	NZ,00121$
;snsfxplay.c:76: KSCAN_KEY = 'B';
	ld	iy, #_KSCAN_KEY
	ld	0 (iy), #0x42
	jr	00131$
00121$:
;snsfxplay.c:79: kscanfast(0);
	push	bc
	xor	a, a
	push	af
	inc	sp
	call	_kscanfast
	inc	sp
	pop	bc
;snsfxplay.c:80: if (KSCAN_KEY == JOY_FIRE) KSCAN_KEY = 'E';
	ld	iy, #_KSCAN_KEY
	ld	a, 0 (iy)
	sub	a, #0x12
	jr	NZ,00131$
	ld	0 (iy), #0x45
00131$:
;snsfxplay.c:83: if (KSCAN_KEY != oldkey) {
	ld	iy, #_KSCAN_KEY
	ld	a, 0 (iy)
	sub	a, c
	jp	Z,00148$
;snsfxplay.c:84: switch(KSCAN_KEY) {
	ld	a, 0 (iy)
	cp	a, #0x31
	jr	Z,00139$
	cp	a, #0x32
	jr	Z,00139$
	cp	a, #0x33
	jr	Z,00139$
	cp	a, #0x34
	jr	Z,00142$
	cp	a, #0x35
	jr	Z,00142$
	cp	a, #0x36
	jr	Z,00142$
	cp	a, #0x37
	jr	Z,00143$
	cp	a, #0x38
	jp	Z,00144$
	cp	a, #0x39
	jp	Z,00145$
	cp	a, #0x41
	jr	Z,00136$
	cp	a, #0x42
	jr	Z,00136$
	cp	a, #0x43
	jr	Z,00136$
	cp	a, #0x44
	jr	Z,00136$
	sub	a, #0x45
	jp	NZ,00146$
;snsfxplay.c:89: case 'E':
00136$:
;d:/work/coleco/libti99coleco/sound.h:50: inline void MUTE_SOUND()					{ SOUND=TONE1_VOL|0x0f; SOUND=TONE2_VOL|0x0f; SOUND=TONE3_VOL|0x0f; SOUND=NOISE_VOL|0x0f; }
	ld	a, #0x9f
	out	(_SOUND), a
	ld	a, #0xbf
	out	(_SOUND), a
	ld	a, #0xdf
	out	(_SOUND), a
	ld	a, #0xff
	out	(_SOUND), a
;snsfxplay.c:91: StartSong((unsigned char*)music, KSCAN_KEY-'A');
	ld	a,(#_KSCAN_KEY + 0)
	add	a, #0xbf
	ld	b, a
	push	bc
	inc	sp
	ld	hl, #_music
	push	hl
	call	_StartSong
	pop	af
	inc	sp
;snsfxplay.c:92: isPlaying = 1;
	ld	-1 (ix), #0x01
;snsfxplay.c:93: break;
	jr	00146$
;snsfxplay.c:97: case '3':
00139$:
;snsfxplay.c:99: StartSfx((unsigned char*)music, KSCAN_KEY-'1'+5, 1);
	ld	a,(#_KSCAN_KEY + 0)
	add	a, #0xd4
	ld	b, a
	ld	a, #0x01
	push	af
	inc	sp
	push	bc
	inc	sp
	ld	hl, #_music
	push	hl
	call	_StartSfx
	pop	af
	pop	af
;snsfxplay.c:100: break;
	jr	00146$
;snsfxplay.c:104: case '6':
00142$:
;snsfxplay.c:106: StartSfx((unsigned char*)music, KSCAN_KEY-'4'+5, 127);
	ld	a,(#_KSCAN_KEY + 0)
	add	a, #0xd1
	ld	b, a
	ld	a, #0x7f
	push	af
	inc	sp
	push	bc
	inc	sp
	ld	hl, #_music
	push	hl
	call	_StartSfx
	pop	af
	pop	af
;snsfxplay.c:107: break;
	jr	00146$
;snsfxplay.c:109: case '7':
00143$:
;snsfxplay.c:110: StopSong();
	call	_StopSong
;d:/work/coleco/libti99coleco/sound.h:50: inline void MUTE_SOUND()					{ SOUND=TONE1_VOL|0x0f; SOUND=TONE2_VOL|0x0f; SOUND=TONE3_VOL|0x0f; SOUND=NOISE_VOL|0x0f; }
	ld	a, #0x9f
	out	(_SOUND), a
	ld	a, #0xbf
	out	(_SOUND), a
	ld	a, #0xdf
	out	(_SOUND), a
	ld	a, #0xff
	out	(_SOUND), a
;snsfxplay.c:112: break;
	jr	00146$
;snsfxplay.c:114: case '8':
00144$:
;snsfxplay.c:115: sfx_StopSong();
	call	_sfx_StopSong
;snsfxplay.c:116: break;
	jr	00146$
;snsfxplay.c:118: case '9':
00145$:
;snsfxplay.c:119: StopSong();     // if we stop song first, stopSfx won't waste time restoring sound channels
	call	_StopSong
;snsfxplay.c:120: sfx_StopSong();
	call	_sfx_StopSong
;d:/work/coleco/libti99coleco/sound.h:50: inline void MUTE_SOUND()					{ SOUND=TONE1_VOL|0x0f; SOUND=TONE2_VOL|0x0f; SOUND=TONE3_VOL|0x0f; SOUND=NOISE_VOL|0x0f; }
	ld	a, #0x9f
	out	(_SOUND), a
	ld	a, #0xbf
	out	(_SOUND), a
	ld	a, #0xdf
	out	(_SOUND), a
	ld	a, #0xff
	out	(_SOUND), a
;snsfxplay.c:125: }
00146$:
;snsfxplay.c:126: oldkey=KSCAN_KEY;
	ld	iy, #_KSCAN_KEY
	ld	c, 0 (iy)
00148$:
;snsfxplay.c:132: VDP_SET_ADDRESS(gImage);
	ld	de, (_gImage)
;d:/work/coleco/libti99coleco/vdp.h:57: inline void VDP_SET_ADDRESS(unsigned int x)							{	VDPWA=((x)&0xff); VDPWA=((x)>>8); VDP_SAFE_DELAY();	}
	ld	a, e
	out	(_VDPWA), a
	ld	b, #0x00
	ld	a, d
	out	(_VDPWA), a
;d:/work/coleco/libti99coleco/vdp.h:42: __endasm;
	nop
	nop
	nop
	nop
	nop
;snsfxplay.c:133: faster_hexprint(songNote[0]>>8);
	ld	hl, (#_songNote + 0)
	ld	b, #0x00
	push	bc
	push	hl
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:134: faster_hexprint(songNote[0]&0xff);	// song channel 1 tone
	ld	a, (#_songNote + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:135: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:136: faster_hexprint(songVol[0]);		// song channel 1 volume
	ld	a, (#_songVol + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:137: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:139: faster_hexprint(songNote[1]>>8);
	ld	hl, (#(_songNote + 0x0002) + 0)
	ld	b, #0x00
	push	bc
	push	hl
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:140: faster_hexprint(songNote[1]&0xff);	// song channel 2 tone
	ld	a, (#(_songNote + 0x0002) + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:141: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:142: faster_hexprint(songVol[1]);		// song channel 2 volume
	ld	a, (#_songVol + 1)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:143: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:145: faster_hexprint(songNote[2]>>8);
	ld	hl, (#(_songNote + 0x0004) + 0)
	ld	b, #0x00
	push	bc
	push	hl
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:146: faster_hexprint(songNote[2]&0xff);	// song channel 3 tone
	ld	a, (#(_songNote + 0x0004) + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:147: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:148: faster_hexprint(songVol[2]);		// song channel 3 volume
	ld	a, (#_songVol + 2)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:149: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:151: faster_hexprint(songNote[3]>>8);	// song noise channel
	ld	hl, (#(_songNote + 0x0006) + 0)
	ld	b, #0x00
	push	bc
	push	hl
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:152: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:153: faster_hexprint(songVol[3]);     	// song noise volume
	ld	a, (#_songVol + 3)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:154: VDPWD=' ';
	ld	a, #0x20
	out	(_VDPWD), a
;snsfxplay.c:156: faster_hexprint(songNote[3]&0xff);	// song playing status
	ld	a, (#(_songNote + 0x0006) + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
;snsfxplay.c:157: faster_hexprint(sfx_songNote[3]&0xff);	// sfx playing status
	ld	a, (#(_sfx_songNote + 0x0006) + 0)
	push	bc
	push	af
	inc	sp
	call	_faster_hexprint
	inc	sp
	pop	bc
	jp	00159$
;snsfxplay.c:159: }
	inc	sp
	pop	ix
	ret
___str_0:
	.ascii "Music:"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_1:
	.ascii " U - Antarctic Adventure"
	.db 0x0a
	.db 0x00
___str_2:
	.ascii " D - California Games Title"
	.db 0x0a
	.db 0x00
___str_3:
	.ascii " L - Double Dragon"
	.db 0x0a
	.db 0x00
___str_4:
	.ascii " R - Moonwalker"
	.db 0x0a
	.db 0x00
___str_5:
	.ascii " F - Alf"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_6:
	.ascii "Low Priority SFX:"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_7:
	.ascii " 1 - Flag"
	.db 0x0a
	.db 0x00
___str_8:
	.ascii " 2 - Hole"
	.db 0x0a
	.db 0x00
___str_9:
	.ascii " 3 - Jump"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_10:
	.ascii "High Priority SFX:"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_11:
	.ascii " 4 - Flag"
	.db 0x0a
	.db 0x00
___str_12:
	.ascii " 5 - Hole"
	.db 0x0a
	.db 0x00
___str_13:
	.ascii " 6 - Jump"
	.db 0x0a
	.db 0x0a
	.db 0x00
___str_14:
	.ascii "7-stop music  8-stop sfx  9-stop all"
	.db 0x0a
	.db 0x00
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

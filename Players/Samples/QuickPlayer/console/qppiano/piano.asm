;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module piano
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _drawkey
	.globl _pianoinit
	.globl _memcpy
	.globl _SongLoop
	.globl _StartSong
	.globl _charsetlc
	.globl _delsprite
	.globl _sprite
	.globl _vdpwaitvint
	.globl _vdpchar
	.globl _vdpmemcpy
	.globl _vdpmemset
	.globl _set_graphics_raw
	.globl _firstSong
	.globl _Color
	.globl _Freqs
	.globl _KeyPos
	.globl _KeySprites
	.globl _KeyTypes
	.globl _Patterns
	.globl _Screen
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
_firstSong::
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
;--------------------------------------------------------
; Home
;--------------------------------------------------------
	.area _HOME
	.area _HOME
;--------------------------------------------------------
; code
;--------------------------------------------------------
	.area _CODE
;piano.c:294: void pianoinit() {
;	---------------------------------
; Function pianoinit
; ---------------------------------
_pianoinit::
	dec	sp
;piano.c:295: unsigned char x = set_graphics_raw(VDP_SPR_8x8);		// set graphics mode
	xor	a, a
	push	af
	inc	sp
	call	_set_graphics_raw
	ld	a, l
	inc	sp
	ld	iy, #0
	add	iy, sp
	ld	0 (iy), a
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x04
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;piano.c:297: charsetlc();								// load default character set with lowercase
	call	_charsetlc
;piano.c:300: vdpmemset(gImage, 32, 768);
	ld	hl, #0x0300
	push	hl
	ld	a, #0x20
	push	af
	inc	sp
	ld	hl, (_gImage)
	push	hl
	call	_vdpmemset
	pop	af
;piano.c:303: vdpmemset(gColor, 0xf0, 32);
	inc	sp
	ld	hl,#0x0020
	ex	(sp),hl
	ld	a, #0xf0
	push	af
	inc	sp
	ld	hl, (_gColor)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;piano.c:304: vdpmemset(gColor+0x11, 0x1e, 9);
	ld	hl, (_gColor)
	ld	de, #0x0011
	add	hl, de
	ld	de, #0x0009
	push	de
	ld	a, #0x1e
	push	af
	inc	sp
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;piano.c:307: vdpmemcpy(gImage+0x60, Screen, 32*9);
	ld	bc, #_Screen+0
	ld	hl, (_gImage)
	ld	de, #0x0060
	add	hl, de
	ld	de, #0x0120
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
;piano.c:310: vdpmemset(gSprite, 0xd1, 16*4);
	ld	hl, #0x0040
	ex	(sp),hl
	ld	a, #0xd1
	push	af
	inc	sp
	ld	hl, (_gSprite)
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;piano.c:311: vdpchar(gSprite+16*4, 0xd0);		// turn off the rest
	ld	hl, (_gSprite)
	ld	de, #0x0040
	add	hl, de
	ld	a, #0xd0
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
;piano.c:316: vdpmemcpy(gPattern+0x440, Patterns, 560);
	ld	bc, #_Patterns+0
	ld	hl, (_gPattern)
	ld	de, #0x0440
	add	hl, de
	ld	de, #0x0230
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	iy, #0
	add	iy, sp
	ld	a, 0 (iy)
	out	(_VDPWA), a
	ld	a, #0x81
	out	(_VDPWA), a
;piano.c:318: VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display
;piano.c:319: }
	inc	sp
	ret
_tramp:
	.db #0x3a	; 58
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xc3	; 195
	.db #0x00	; 0
	.db #0xc0	; 192
_textout:
	.ascii "~~~~DATAHERE~~~~4"
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
	.db 0x00
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
_Screen:
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xc0	; 192
	.db #0xc1	; 193
	.db #0xc2	; 194
	.db #0xc3	; 195
	.db #0xc4	; 196
	.db #0xc5	; 197
	.db #0xc6	; 198
	.db #0xc7	; 199
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0xc8	; 200
	.db #0xc9	; 201
	.db #0xca	; 202
	.db #0xcb	; 203
	.db #0xcc	; 204
	.db #0xcd	; 205
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x89	; 137
	.db #0x8a	; 138
	.db #0x8b	; 139
	.db #0x8c	; 140
	.db #0x8d	; 141
	.db #0x8e	; 142
	.db #0x8c	; 140
	.db #0x8d	; 141
	.db #0x8e	; 142
	.db #0x8f	; 143
	.db #0x90	; 144
	.db #0x8e	; 142
	.db #0x8f	; 143
	.db #0x90	; 144
	.db #0x91	; 145
	.db #0x92	; 146
	.db #0x90	; 144
	.db #0x91	; 145
	.db #0x92	; 146
	.db #0x8a	; 138
	.db #0x8b	; 139
	.db #0x8c	; 140
	.db #0x8a	; 138
	.db #0x8b	; 139
	.db #0x8c	; 140
	.db #0x8d	; 141
	.db #0x8e	; 142
	.db #0x8c	; 140
	.db #0x8d	; 141
	.db #0x8e	; 142
	.db #0x8f	; 143
	.db #0x93	; 147
	.db #0x94	; 148
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9b	; 155
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9b	; 155
	.db #0x9c	; 156
	.db #0x9d	; 157
	.db #0x9b	; 155
	.db #0x9c	; 156
	.db #0x9d	; 157
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9e	; 158
	.db #0x94	; 148
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9b	; 155
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9b	; 155
	.db #0x9c	; 156
	.db #0x9d	; 157
	.db #0x9b	; 155
	.db #0x9c	; 156
	.db #0x9d	; 157
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x95	; 149
	.db #0x96	; 150
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x97	; 151
	.db #0x98	; 152
	.db #0x99	; 153
	.db #0x9a	; 154
	.db #0x9e	; 158
	.db #0x94	; 148
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9f	; 159
	.db #0xa0	; 160
	.db #0xa1	; 161
	.db #0x9e	; 158
	.db #0xa2	; 162
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa3	; 163
	.db #0xa4	; 164
	.db #0xa5	; 165
	.db #0xa6	; 166
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
	.db #0x88	; 136
_Patterns:
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0xff	; 255
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0xff	; 255
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0xff	; 255
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xff	; 255
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0xff	; 255
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0xff	; 255
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xff	; 255
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0xff	; 255
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0xff	; 255
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xff	; 255
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0xff	; 255
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0x78	; 120	'x'
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0xc7	; 199
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x9e	; 158
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0xe7	; 231
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x8c	; 140
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0x79	; 121	'y'
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0xe3	; 227
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x07	; 7
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0x30	; 48	'0'
	.db #0xff	; 255
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xc3	; 195
	.db #0xff	; 255
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0xff	; 255
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0xe0	; 224
	.db #0x00	; 0
	.db #0x3c	; 60
	.db #0x40	; 64
	.db #0x40	; 64
	.db #0x5c	; 92
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0x38	; 56	'8'
	.db #0x00	; 0
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0x7c	; 124
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0x00	; 0
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0xf0	; 240
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0xf0	; 240
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0xf0	; 240
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xf0	; 240
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0xe0	; 224
	.db #0x00	; 0
	.db #0x78	; 120	'x'
	.db #0x44	; 68	'D'
	.db #0x44	; 68	'D'
	.db #0x78	; 120	'x'
	.db #0x50	; 80	'P'
	.db #0x48	; 72	'H'
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x38	; 56	'8'
	.db #0x44	; 68	'D'
	.db #0x40	; 64
	.db #0x38	; 56	'8'
	.db #0x04	; 4
	.db #0x44	; 68	'D'
	.db #0x38	; 56	'8'
	.db #0x1c	; 28
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x00	; 0
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x1c	; 28
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x22	; 34
	.db #0x22	; 34
	.db #0x1c	; 28
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x10	; 16
	.db #0x28	; 40
	.db #0x44	; 68	'D'
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
	.db #0x80	; 128
	.db #0x02	; 2
	.db #0xce	; 206
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x28	; 40
	.db #0x18	; 24
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x86	; 134
	.db #0x7c	; 124
	.db #0x04	; 4
	.db #0x1a	; 26
	.db #0x12	; 18
	.db #0x06	; 6
	.db #0x1a	; 26
	.db #0x12	; 18
	.db #0x92	; 146
	.db #0x76	; 118	'v'
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x7e	; 126
	.db #0x84	; 132
	.db #0xfa	; 250
	.db #0x02	; 2
	.db #0x86	; 134
	.db #0x7c	; 124
	.db #0x08	; 8
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x2c	; 44
	.db #0x18	; 24
	.db #0x12	; 18
	.db #0x16	; 22
	.db #0x0c	; 12
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x12	; 18
	.db #0x92	; 146
	.db #0x7e	; 126
	.db #0x02	; 2
	.db #0x0e	; 14
	.db #0x18	; 24
	.db #0x02	; 2
	.db #0x1e	; 30
	.db #0x08	; 8
	.db #0x82	; 130
	.db #0x7e	; 126
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x1a	; 26
	.db #0x86	; 134
	.db #0x44	; 68	'D'
	.db #0x04	; 4
	.db #0x3c	; 60
	.db #0x04	; 4
	.db #0x1a	; 26
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x06	; 6
	.db #0xfc	; 252
	.db #0x04	; 4
	.db #0x1a	; 26
	.db #0x12	; 18
	.db #0x06	; 6
	.db #0x1a	; 26
	.db #0x12	; 18
	.db #0x92	; 146
	.db #0x76	; 118	'v'
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x86	; 134
	.db #0x7c	; 124
	.db #0x04	; 4
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0xaa	; 170
	.db #0x7e	; 126
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x1c	; 28
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
	.db #0xff	; 255
_KeyTypes:
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x00	; 0
_KeySprites:
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x04	; 4
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x08	; 8
	.db #0x08	; 8
	.db #0x05	; 5
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0xff	; 255
	.db #0xff	; 255
_KeyPos:
	.db #0x06	; 6
	.db #0x09	; 9
	.db #0x0c	; 12
	.db #0x12	; 18
	.db #0x15	; 21
	.db #0x18	; 24
	.db #0x1b	; 27
	.db #0x1e	; 30
	.db #0x24	; 36
	.db #0x27	; 39
	.db #0x2a	; 42
	.db #0x2d	; 45
	.db #0x30	; 48	'0'
	.db #0x33	; 51	'3'
	.db #0x36	; 54	'6'
	.db #0x3c	; 60
	.db #0x3f	; 63
	.db #0x42	; 66	'B'
	.db #0x45	; 69	'E'
	.db #0x48	; 72	'H'
	.db #0x4e	; 78	'N'
	.db #0x51	; 81	'Q'
	.db #0x54	; 84	'T'
	.db #0x57	; 87	'W'
	.db #0x5a	; 90	'Z'
	.db #0x5d	; 93
	.db #0x60	; 96
	.db #0x66	; 102	'f'
	.db #0x69	; 105	'i'
	.db #0x6c	; 108	'l'
	.db #0x6f	; 111	'o'
	.db #0x72	; 114	'r'
	.db #0x78	; 120	'x'
	.db #0x7b	; 123
	.db #0x7e	; 126
	.db #0x81	; 129
	.db #0x84	; 132
	.db #0x87	; 135
	.db #0x8a	; 138
	.db #0x90	; 144
	.db #0x93	; 147
	.db #0x96	; 150
	.db #0x99	; 153
	.db #0x9c	; 156
	.db #0xa2	; 162
	.db #0xa5	; 165
	.db #0xa8	; 168
	.db #0xab	; 171
	.db #0xae	; 174
	.db #0xb1	; 177
	.db #0xb4	; 180
	.db #0xba	; 186
	.db #0xbd	; 189
	.db #0xc0	; 192
	.db #0xc3	; 195
	.db #0xc6	; 198
	.db #0xcc	; 204
	.db #0xcf	; 207
	.db #0xd2	; 210
	.db #0xd5	; 213
	.db #0xd8	; 216
	.db #0xdb	; 219
	.db #0xde	; 222
	.db #0xe4	; 228
	.db #0xe7	; 231
	.db #0xea	; 234
	.db #0xed	; 237
	.db #0xf0	; 240
	.db #0xf6	; 246
_Freqs:
	.dw #0x03f9
	.dw #0x03c0
	.dw #0x038a
	.dw #0x0357
	.dw #0x0327
	.dw #0x02fa
	.dw #0x02cf
	.dw #0x02a7
	.dw #0x0281
	.dw #0x025d
	.dw #0x023b
	.dw #0x021b
	.dw #0x01fc
	.dw #0x01e0
	.dw #0x01c5
	.dw #0x01ac
	.dw #0x0194
	.dw #0x017d
	.dw #0x0168
	.dw #0x0153
	.dw #0x0140
	.dw #0x012e
	.dw #0x011d
	.dw #0x010d
	.dw #0x00fe
	.dw #0x00f0
	.dw #0x00e2
	.dw #0x00d6
	.dw #0x00ca
	.dw #0x00be
	.dw #0x00b4
	.dw #0x00aa
	.dw #0x00a0
	.dw #0x0097
	.dw #0x008f
	.dw #0x0087
	.dw #0x007f
	.dw #0x0078
	.dw #0x0071
	.dw #0x006b
	.dw #0x0065
	.dw #0x005f
	.dw #0x005a
	.dw #0x0055
	.dw #0x0050
	.dw #0x004c
	.dw #0x0047
	.dw #0x0043
	.dw #0x0040
	.dw #0x003c
	.dw #0x0039
	.dw #0x0035
	.dw #0x0032
	.dw #0x0030
	.dw #0x002d
	.dw #0x002a
	.dw #0x0028
	.dw #0x0026
	.dw #0x0024
	.dw #0x0022
	.dw #0x0020
	.dw #0x001e
	.dw #0x001c
	.dw #0x001b
	.dw #0x0019
	.dw #0x0018
	.dw #0x0016
	.dw #0x0015
	.dw #0x0014
	.dw #0x0000
_Color:
	.db #0x08	; 8
	.db #0x02	; 2
	.db #0x05	; 5
;piano.c:321: void drawkey(idx_t voice, idx_t key) {
;	---------------------------------
; Function drawkey
; ---------------------------------
_drawkey::
	call	___sdcc_enter_ix
	push	af
	push	af
	push	af
	dec	sp
;piano.c:322: idx_t spr=voice*5;
	ld	b, 4 (ix)
	ld	a, b
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	ld	c, a
;piano.c:323: idx_t type=KeyTypes[key];
	ld	de, #_KeyTypes+0
	ld	l, 5 (ix)
	ld	h, #0x00
	add	hl, de
	ld	a, (hl)
	ld	-1 (ix), a
;piano.c:324: idx_t cc=KeyPos[key];
	ld	de, #_KeyPos+0
	ld	l, 5 (ix)
	ld	h, #0x00
	add	hl, de
	ld	a, (hl)
	ld	-7 (ix), a
;piano.c:325: sprite(spr++, KeySprites[type][0]+169, Color[voice], 48, cc);		// sprite offset character is 169
	ld	a, #<(_Color)
	add	a, b
	ld	-6 (ix), a
	ld	a, #>(_Color)
	adc	a, #0x00
	ld	-5 (ix), a
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	ld	b, (hl)
	ld	e, -1 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	add	hl, de
	ld	-4 (ix), l
	ld	-3 (ix), h
	ld	a, #<(_KeySprites)
	add	a, -4 (ix)
	ld	e, a
	ld	a, #>(_KeySprites)
	adc	a, -3 (ix)
	ld	d, a
	ld	a, (de)
	add	a, #0xa9
	ld	l, a
	ld	a, c
	inc	c
	push	bc
	push	de
	ld	h, -7 (ix)
	push	hl
	inc	sp
	ld	h, #0x30
	push	hl
	inc	sp
	push	bc
	inc	sp
	ld	h, l
	push	hl
	inc	sp
	push	af
	inc	sp
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	de
	pop	bc
;piano.c:326: sprite(spr++, KeySprites[type][1]+169, Color[voice], 56, cc);
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	ld	b, (hl)
	ld	l, e
	ld	h, d
	inc	hl
	ld	a, (hl)
	add	a, #0xa9
	ld	l, a
	ld	a, c
	inc	c
	push	bc
	push	de
	ld	h, -7 (ix)
	push	hl
	inc	sp
	ld	h, #0x38
	push	hl
	inc	sp
	push	bc
	inc	sp
	ld	h, l
	push	hl
	inc	sp
	push	af
	inc	sp
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	de
	pop	bc
;piano.c:327: sprite(spr++, KeySprites[type][2]+169, Color[voice], 64, cc);
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	ld	b, (hl)
	inc	de
	inc	de
	ld	a, (de)
	add	a, #0xa9
	ld	d, a
	ld	a, c
	inc	c
	push	bc
	ld	h, -7 (ix)
	push	hl
	inc	sp
	ld	h, #0x40
	ld	l, b
	push	hl
	push	de
	inc	sp
	push	af
	inc	sp
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	bc
;piano.c:325: sprite(spr++, KeySprites[type][0]+169, Color[voice], 48, cc);		// sprite offset character is 169
	ld	e, c
	inc	e
;piano.c:328: if (type == 4) {
	ld	a, -1 (ix)
	sub	a, #0x04
	jr	NZ, 00102$
;piano.c:329: delsprite(spr++);
	ld	a, c
	push	de
	push	af
	inc	sp
	call	_delsprite
	inc	sp
	pop	de
;piano.c:330: delsprite(spr++);
	ld	a, e
	push	af
	inc	sp
	call	_delsprite
	jr	00104$
00102$:
;piano.c:332: sprite(spr++, KeySprites[type][3]+169, Color[voice], 72, cc);
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	ld	b, (hl)
	ld	a, #<(_KeySprites)
	add	a, -4 (ix)
	ld	-2 (ix), a
	ld	a, #>(_KeySprites)
	adc	a, -3 (ix)
	ld	-1 (ix), a
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	add	a, #0xa9
	push	de
	ld	h, -7 (ix)
	push	hl
	inc	sp
	ld	h, #0x48
	ld	l, b
	push	hl
	ld	b, a
	push	bc
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	de
;piano.c:333: sprite(spr++, KeySprites[type][4]+169, Color[voice], 80, cc);
	ld	l, -6 (ix)
	ld	h, -5 (ix)
	ld	c, (hl)
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	inc	hl
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	add	a, #0xa9
	ld	h, -7 (ix)
	push	hl
	inc	sp
	ld	h, #0x50
	push	hl
	inc	sp
	ld	h, c
	push	hl
	inc	sp
	ld	d,a
	push	de
	call	_sprite
	pop	af
	pop	af
00104$:
;piano.c:335: }
	ld	sp, ix
	pop	ix
	ret
;piano.c:337: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	dec	sp
;piano.c:341: pianoinit();
	call	_pianoinit
;piano.c:344: vdpmemcpy(17*32, textout, 32*4);
	ld	hl, #0x0080
	push	hl
	ld	hl, #_textout
	push	hl
	ld	hl, #0x0220
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;piano.c:347: firstSong = *((unsigned int*)&flags[6]);
	ld	hl, #(_flags + 0x0006)
	ld	a, (hl)
	ld	(_firstSong+0), a
	inc	hl
	ld	a, (hl)
	ld	(_firstSong+1), a
;piano.c:351: volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];
;piano.c:353: do {
00123$:
;piano.c:354: StartSong((unsigned char*)firstSong,0);
	ld	hl, (_firstSong)
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_StartSong
	pop	af
	inc	sp
;piano.c:357: done = 0;
	ld	c, #0x00
;piano.c:358: while (!done) {
00116$:
	ld	a, c
	or	a, a
	jp	NZ, 00118$
;piano.c:359: done = 1;
	ld	c, #0x01
;piano.c:360: vdpwaitvint();
	push	bc
	call	_vdpwaitvint
	pop	bc
;piano.c:362: if (isSNPlaying) {
	ld	hl, (#(_songNote + 0x0006) + 0)
	bit	0, l
	jr	Z, 00143$
;piano.c:363: CALL_PLAYER_SN;
	call	_SongLoop
;piano.c:364: done = 0;
	ld	c, #0x00
;piano.c:368: for (idx_t idx=0; idx<3; idx++) {
00143$:
	ld	b, #0x00
00130$:
	ld	a, b
	sub	a, #0x03
	jp	NC, 00112$
;piano.c:369: if ((songVol[idx]&0x0f) < 0x0f) {
	ld	de, #_songVol+0
	ld	l, b
	ld	h, #0x00
	add	hl, de
	ld	a, (hl)
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
	jr	NC, 00110$
;piano.c:370: unsigned int note=songNote[idx];		// note: mangled! 0x8312 == 0x0123
	ld	de, #_songNote+0
	ld	a, b
	ld	h, #0x00
	ld	l, a
	add	hl, hl
	add	hl, de
	ld	a, (hl)
	inc	hl
	ld	e, (hl)
;piano.c:371: note=((note&0xff)<<4)|((note&0x0f00)>>8);	// unmangle
	ld	l, a
	ld	h, #0x00
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, e
	and	a, #0x0f
	ld	e, a
	ld	d, #0x00
	ld	a, e
	or	a, l
	ld	e, a
	ld	a, d
	or	a, h
	ld	d, a
;piano.c:375: while (Freqs[k] > note) k++;
	ld	-1 (ix), #0
00103$:
	ld	l, -1 (ix)
	ld	h, #0x00
	add	hl, hl
	push	de
	ld	de, #_Freqs
	add	hl, de
	pop	de
	ld	a, (hl)
	inc	hl
	ld	h, (hl)
	ld	l, a
	ld	a, e
	sub	a, l
	ld	a, d
	sbc	a, h
	jr	NC, 00150$
	inc	-1 (ix)
	jr	00103$
00150$:
	ld	d, -1 (ix)
;piano.c:376: if (k > NUM_KEYS-1) k=NUM_KEYS-1;
	ld	a, #0x44
	sub	a, -1 (ix)
	jr	NC, 00107$
	ld	d, #0x44
00107$:
;piano.c:377: drawkey(idx, k);
	push	bc
	ld	e, b
	push	de
	call	_drawkey
	pop	af
	pop	bc
	jr	00131$
00110$:
;piano.c:379: idx_t spr=idx*5;
	ld	a, b
	ld	e, a
	add	a, a
	add	a, a
	add	a, e
	ld	e, a
;piano.c:380: for (idx_t idx2=0; idx2<5; idx2++) {
	ld	d, #0x00
00127$:
	ld	a, d
	sub	a, #0x05
	jr	NC, 00131$
;piano.c:381: delsprite(spr+idx2);
	ld	a, e
	add	a, d
	push	bc
	push	de
	push	af
	inc	sp
	call	_delsprite
	inc	sp
	pop	de
	pop	bc
;piano.c:380: for (idx_t idx2=0; idx2<5; idx2++) {
	inc	d
	jr	00127$
00131$:
;piano.c:368: for (idx_t idx=0; idx<3; idx++) {
	inc	b
	jp	00130$
00112$:
;piano.c:388: idx_t nVol = songVol[3]&0x0f;
	ld	a, (#(_songVol + 0x0003) + 0)
	and	a, #0x0f
;piano.c:389: if (nVol == 0x0f) {
	sub	a, #0x0f
	jr	NZ, 00114$
;piano.c:391: vdpchar(gSprite+15*4+3, 0x00);
	ld	hl, (_gSprite)
	ld	de, #0x003f
	add	hl, de
	push	bc
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_vdpchar
	pop	af
	inc	sp
	pop	bc
	jp	00116$
00114$:
;piano.c:394: idx_t note=(songNote[3]&0x0f00)>>8;
	ld	hl, (#(_songNote + 0x0006) + 0)
	ld	a, h
	and	a, #0x0f
	ld	b, #0x00
;piano.c:395: note+=181;	// start at character '1', not 0 (which is 180)
	add	a, #0xb5
;piano.c:396: sprite(15, note, COLOR_MEDRED, 32, 232);
	push	bc
	ld	h, #0xe8
	push	hl
	inc	sp
	ld	h, #0x20
	push	hl
	inc	sp
	ld	h, #0x08
	push	hl
	inc	sp
	ld	d,a
	ld	e,#0x0f
	push	de
	call	_sprite
	pop	af
	pop	af
	inc	sp
	pop	bc
	jp	00116$
00118$:
;piano.c:402: SOUND=0x9F;
	ld	a, #0x9f
	out	(_SOUND), a
;piano.c:403: SOUND=0xBF;
	ld	a, #0xbf
	out	(_SOUND), a
;piano.c:404: SOUND=0xDF;
	ld	a, #0xdf
	out	(_SOUND), a
;piano.c:405: SOUND=0xFF;
	ld	a, #0xff
	out	(_SOUND), a
;piano.c:409: chain = (unsigned int *)(*((volatile unsigned int*)(&flags[14])));
	ld	bc, (#(_flags + 0x000e) + 0)
	ld	l, c
;piano.c:410: if (chain) {
	ld	a,b
	ld	h,a
	or	a, c
	jr	Z, 00124$
;piano.c:412: unsigned int chained = *chain;
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	c, e
;piano.c:413: if (chained) {
	ld	a,d
	ld	b,a
	or	a, e
	jr	Z, 00124$
;piano.c:422: memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
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
;piano.c:423: *((unsigned int*)0x7001) = chained;     // patch the pointer, chained should be on the stack
	ld	(0x7001), bc
;piano.c:424: ((void(*)())0x7000)();                  // call the function, never return
	call	0x7000
00124$:
;piano.c:429: } while (*pLoop);
	ld	a, (#(_flags + 0x000d) + 0)
	or	a, a
	jp	NZ, 00123$
;piano.c:440: __endasm;
	rst	0x00
;piano.c:444: return 2;
	ld	hl, #0x0002
;piano.c:445: }
	inc	sp
	pop	ix
	ret
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

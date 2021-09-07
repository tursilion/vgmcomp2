;--------------------------------------------------------
; File Created by SDCC : free open source ANSI-C Compiler
; Version 4.1.0 #12072 (MINGW64)
;--------------------------------------------------------
	.module chuckrock
	.optsdcc -mz80
	
;--------------------------------------------------------
; Public variables in this module
;--------------------------------------------------------
	.globl _main
	.globl _processList
	.globl _chuckinit
	.globl _unpackchar
	.globl _writeCompressedByte8
	.globl _memset
	.globl _memcpy
	.globl _getCompressedByte
	.globl _SongLoop
	.globl _StartSong
	.globl _vdpwaitvint
	.globl _vdpscreenchar
	.globl _vdpwritescreeninc
	.globl _vdpmemcpy
	.globl _vdpmemset
	.globl _set_bitmap
	.globl _nOldVoice
	.globl _nState
	.globl _nOldVol
	.globl _chucktune
	.globl _strDat
	.globl _chuckf2dat
	.globl _chuckf1dat
	.globl _opheliaf2dat
	.globl _opheliaf1dat
	.globl _garyf2dat
	.globl _garyf1dat
	.globl _dinof2dat
	.globl _dinof1dat
	.globl _filter
	.globl _flags
	.globl _textout
	.globl _tramp
	.globl _chuckpat
	.globl _chuckcol
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
_strDat::
	.ds 16
_chucktune::
	.ds 2
_nOldVol::
	.ds 4
_nState::
	.ds 4
_nOldVoice::
	.ds 8
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
;chuckrock.c:154: void writeCompressedByte8(STREAM *str) {
;	---------------------------------
; Function writeCompressedByte8
; ---------------------------------
_writeCompressedByte8::
;chuckrock.c:155: for (idx_t idx = 0; idx<8; ++idx) {
	ld	c, #0x00
00103$:
	ld	a, c
	sub	a, #0x08
	ret	NC
;chuckrock.c:156: VDPWD = getCompressedByte(str);
	push	bc
	ld	hl, #4
	add	hl, sp
	ld	c, (hl)
	inc	hl
	ld	b, (hl)
	push	bc
	call	_getCompressedByte
	pop	af
	ld	a, l
	pop	bc
	out	(_VDPWD), a
;chuckrock.c:155: for (idx_t idx = 0; idx<8; ++idx) {
	inc	c
;chuckrock.c:158: }
	jr	00103$
_chuckcol:
	.db #0x45	; 69	'E'
	.db #0x00	; 0
	.db #0x4c	; 76	'L'
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0xc1	; 193
	.db #0x4a	; 74	'J'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x16	; 22
	.db #0xf1	; 241
	.db #0xf1	; 241
	.db #0x43	; 67	'C'
	.db #0x11	; 17
	.db #0x01	; 1
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x4b	; 75	'K'
	.db #0x11	; 17
	.db #0x09	; 9
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0xe1	; 225
	.db #0x51	; 81	'Q'
	.db #0xc1	; 193
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x0c	; 12
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0xf1	; 241
	.db #0xe1	; 225
	.db #0xf1	; 241
	.db #0xb1	; 177
	.db #0x1c	; 28
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x1b	; 27
	.db #0x19	; 25
	.db #0x44	; 68	'D'
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0xc1	; 193
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0xbc	; 188
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x61	; 97	'a'
	.db #0x1a	; 26
	.db #0x1b	; 27
	.db #0x44	; 68	'D'
	.db #0x16	; 22
	.db #0x00	; 0
	.db #0xc1	; 193
	.db #0x44	; 68	'D'
	.db #0x16	; 22
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x4b	; 75	'K'
	.db #0x11	; 17
	.db #0x0c	; 12
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x19	; 25
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x46	; 70	'F'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0xc1	; 193
	.db #0x16	; 22
	.db #0x1b	; 27
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x80	; 128
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x15	; 21
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0xa1	; 161
	.db #0xa6	; 166
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0xa6	; 166
	.db #0xa6	; 166
	.db #0x61	; 97	'a'
	.db #0x1a	; 26
	.db #0x19	; 25
	.db #0x1a	; 26
	.db #0x1a	; 26
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x40	; 64
	.db #0x1e	; 30
	.db #0x0a	; 10
	.db #0x1a	; 26
	.db #0xa1	; 161
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x1c	; 28
	.db #0x41	; 65	'A'
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x31	; 49	'1'
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0xb1	; 177
	.db #0x91	; 145
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x19	; 25
	.db #0x11	; 17
	.db #0x44	; 68	'D'
	.db #0x61	; 97	'a'
	.db #0x03	; 3
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0xa0	; 160
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x02	; 2
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x43	; 67	'C'
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0xbc	; 188
	.db #0xb6	; 182
	.db #0x91	; 145
	.db #0xa1	; 161
	.db #0xa6	; 166
	.db #0xa6	; 166
	.db #0x19	; 25
	.db #0x40	; 64
	.db #0x1a	; 26
	.db #0x10	; 16
	.db #0x16	; 22
	.db #0x6a	; 106	'j'
	.db #0x66	; 102	'f'
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0xa1	; 161
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0xb1	; 177
	.db #0x21	; 33
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0xbc	; 188
	.db #0x41	; 65	'A'
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x11	; 17
	.db #0x80	; 128
	.db #0x61	; 97	'a'
	.db #0x1c	; 28
	.db #0x05	; 5
	.db #0x61	; 97	'a'
	.db #0x6c	; 108	'l'
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0x16	; 22
	.db #0x18	; 24
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0xc1	; 193
	.db #0xff	; 255
	.db #0x6e	; 110	'n'
	.db #0x08	; 8
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x0d	; 13
	.db #0x91	; 145
	.db #0xf1	; 241
	.db #0x1e	; 30
	.db #0x31	; 49	'1'
	.db #0xe1	; 225
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0xc1	; 193
	.db #0xe1	; 225
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0xe1	; 225
	.db #0xb1	; 177
	.db #0xb1	; 177
	.db #0x40	; 64
	.db #0x1e	; 30
	.db #0x40	; 64
	.db #0xe1	; 225
	.db #0x1c	; 28
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x1e	; 30
	.db #0x61	; 97	'a'
	.db #0x15	; 21
	.db #0x1e	; 30
	.db #0x11	; 17
	.db #0x51	; 81	'Q'
	.db #0xe1	; 225
	.db #0x31	; 49	'1'
	.db #0x51	; 81	'Q'
	.db #0x16	; 22
	.db #0xe1	; 225
	.db #0x31	; 49	'1'
	.db #0x11	; 17
	.db #0x51	; 81	'Q'
	.db #0xec	; 236
	.db #0x1e	; 30
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x13	; 19
	.db #0x15	; 21
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x1a	; 26
	.db #0x13	; 19
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0xe1	; 225
	.db #0x0e	; 14
	.db #0xec	; 236
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0xec	; 236
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x1c	; 28
	.db #0x11	; 17
	.db #0x1d	; 29
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0x43	; 67	'C'
	.db #0x61	; 97	'a'
	.db #0x01	; 1
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x44	; 68	'D'
	.db #0x11	; 17
	.db #0x01	; 1
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x80	; 128
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x01	; 1
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x05	; 5
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0xb8	; 184
	.db #0x31	; 49	'1'
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x43	; 67	'C'
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x44	; 68	'D'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x0f	; 15
	.db #0xb6	; 182
	.db #0x6b	; 107	'k'
	.db #0x6a	; 106	'j'
	.db #0x1a	; 26
	.db #0x1b	; 27
	.db #0xa1	; 161
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x81	; 129
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x19	; 25
	.db #0x91	; 145
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x44	; 68	'D'
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0xc1	; 193
	.db #0x49	; 73	'I'
	.db #0x11	; 17
	.db #0x44	; 68	'D'
	.db #0xc1	; 193
	.db #0x04	; 4
	.db #0xbc	; 188
	.db #0xec	; 236
	.db #0xcb	; 203
	.db #0xce	; 206
	.db #0xac	; 172
	.db #0x42	; 66	'B'
	.db #0x1c	; 28
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0x45	; 69	'E'
	.db #0x61	; 97	'a'
	.db #0x03	; 3
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x0a	; 10
	.db #0xc1	; 193
	.db #0x31	; 49	'1'
	.db #0x15	; 21
	.db #0x13	; 19
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x1c	; 28
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x0c	; 12
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x15	; 21
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x04	; 4
	.db #0x13	; 19
	.db #0x5c	; 92
	.db #0x13	; 19
	.db #0x1e	; 30
	.db #0x1c	; 28
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x06	; 6
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x14	; 20
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x01	; 1
	.db #0x31	; 49	'1'
	.db #0x14	; 20
	.db #0x46	; 70	'F'
	.db #0x61	; 97	'a'
	.db #0xc0	; 192
	.db #0xfe	; 254
	.db #0x95	; 149
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x81	; 129
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x18	; 24
	.db #0x1e	; 30
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0xb1	; 177
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x19	; 25
	.db #0x1a	; 26
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x6a	; 106	'j'
	.db #0xc9	; 201
	.db #0xe1	; 225
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x6a	; 106	'j'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0xc6	; 198
	.db #0x91	; 145
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x16	; 22
	.db #0x13	; 19
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0xa1	; 161
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0xa1	; 161
	.db #0x16	; 22
	.db #0xc6	; 198
	.db #0x1a	; 26
	.db #0xc6	; 198
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x91	; 145
	.db #0xc1	; 193
	.db #0x61	; 97	'a'
	.db #0xa1	; 161
	.db #0x6c	; 108	'l'
	.db #0x6c	; 108	'l'
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x13	; 19
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x31	; 49	'1'
	.db #0x40	; 64
	.db #0xe1	; 225
	.db #0x0a	; 10
	.db #0xc1	; 193
	.db #0x14	; 20
	.db #0x61	; 97	'a'
	.db #0x31	; 49	'1'
	.db #0xe1	; 225
	.db #0xec	; 236
	.db #0xe1	; 225
	.db #0x31	; 49	'1'
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x21	; 33
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x07	; 7
	.db #0x1c	; 28
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0x01	; 1
	.db #0x51	; 81	'Q'
	.db #0x1c	; 28
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0x08	; 8
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0xcc	; 204
	.db #0xcc	; 204
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0x01	; 1
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x44	; 68	'D'
	.db #0x61	; 97	'a'
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x18	; 24
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x46	; 70	'F'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x1e	; 30
	.db #0xc1	; 193
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0xf6	; 246
	.db #0xe6	; 230
	.db #0xb1	; 177
	.db #0xb6	; 182
	.db #0xe4	; 228
	.db #0xfe	; 254
	.db #0x9b	; 155
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0xb6	; 182
	.db #0xe6	; 230
	.db #0xe6	; 230
	.db #0xae	; 174
	.db #0xfb	; 251
	.db #0xb9	; 185
	.db #0x16	; 22
	.db #0x6e	; 110	'n'
	.db #0x6f	; 111	'o'
	.db #0x16	; 22
	.db #0x81	; 129
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x6f	; 111	'o'
	.db #0xb6	; 182
	.db #0xf1	; 241
	.db #0xf6	; 246
	.db #0xb1	; 177
	.db #0xf6	; 246
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x0a	; 10
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x91	; 145
	.db #0xa1	; 161
	.db #0x61	; 97	'a'
	.db #0x81	; 129
	.db #0x80	; 128
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x02	; 2
	.db #0x91	; 145
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x42	; 66	'B'
	.db #0x61	; 97	'a'
	.db #0xc0	; 192
	.db #0xff	; 255
	.db #0xe8	; 232
	.db #0x07	; 7
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x41	; 65	'A'
	.db #0x16	; 22
	.db #0x10	; 16
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0xa6	; 166
	.db #0x6a	; 106	'j'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x6c	; 108	'l'
	.db #0x61	; 97	'a'
	.db #0xc6	; 198
	.db #0x16	; 22
	.db #0x13	; 19
	.db #0x16	; 22
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x01	; 1
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x80	; 128
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0xc0	; 192
	.db #0xfd	; 253
	.db #0xa1	; 161
	.db #0x14	; 20
	.db #0x81	; 129
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0x15	; 21
	.db #0x6b	; 107	'k'
	.db #0xb6	; 182
	.db #0xb6	; 182
	.db #0x6c	; 108	'l'
	.db #0xe1	; 225
	.db #0xc1	; 193
	.db #0x4c	; 76	'L'
	.db #0x4a	; 74	'J'
	.db #0xce	; 206
	.db #0x14	; 20
	.db #0xc4	; 196
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0xb1	; 177
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x02	; 2
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0xc0	; 192
	.db #0xfd	; 253
	.db #0xfb	; 251
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0xe1	; 225
	.db #0xc1	; 193
	.db #0x80	; 128
	.db #0xc1	; 193
	.db #0x11	; 17
	.db #0x18	; 24
	.db #0x11	; 17
	.db #0xe1	; 225
	.db #0x4c	; 76	'L'
	.db #0xcd	; 205
	.db #0xc5	; 197
	.db #0x4c	; 76	'L'
	.db #0xc1	; 193
	.db #0x51	; 81	'Q'
	.db #0x51	; 81	'Q'
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x14	; 20
	.db #0xbc	; 188
	.db #0xec	; 236
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0xe1	; 225
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x14	; 20
	.db #0x1c	; 28
	.db #0x44	; 68	'D'
	.db #0x61	; 97	'a'
	.db #0x43	; 67	'C'
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0x16	; 22
	.db #0x43	; 67	'C'
	.db #0x61	; 97	'a'
	.db #0x0a	; 10
	.db #0x81	; 129
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0xb6	; 182
	.db #0xb6	; 182
	.db #0x6c	; 108	'l'
	.db #0x6e	; 110	'n'
	.db #0x6b	; 107	'k'
	.db #0xe6	; 230
	.db #0xf6	; 246
	.db #0x40	; 64
	.db #0xe6	; 230
	.db #0x81	; 129
	.db #0xf6	; 246
	.db #0xe6	; 230
	.db #0x01	; 1
	.db #0x61	; 97	'a'
	.db #0x18	; 24
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x02	; 2
	.db #0x61	; 97	'a'
	.db #0xb1	; 177
	.db #0xf6	; 246
	.db #0xa0	; 160
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x1b	; 27
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x43	; 67	'C'
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x05	; 5
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x42	; 66	'B'
	.db #0x61	; 97	'a'
	.db #0x03	; 3
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x43	; 67	'C'
	.db #0x61	; 97	'a'
	.db #0x0b	; 11
	.db #0xc6	; 198
	.db #0xc6	; 198
	.db #0xa4	; 164
	.db #0x61	; 97	'a'
	.db #0x71	; 113	'q'
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0xf6	; 246
	.db #0x16	; 22
	.db #0x64	; 100	'd'
	.db #0xa6	; 166
	.db #0xb6	; 182
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x02	; 2
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x20	; 32
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x11	; 17
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0xb1	; 177
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x51	; 81	'Q'
	.db #0xe1	; 225
	.db #0xa6	; 166
	.db #0xc6	; 198
	.db #0x1a	; 26
	.db #0x5b	; 91
	.db #0x4b	; 75	'K'
	.db #0xcb	; 203
	.db #0x4a	; 74	'J'
	.db #0x1e	; 30
	.db #0xe1	; 225
	.db #0x15	; 21
	.db #0xec	; 236
	.db #0xb4	; 180
	.db #0xbc	; 188
	.db #0x4e	; 78	'N'
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x15	; 21
	.db #0x13	; 19
	.db #0x1c	; 28
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0xc1	; 193
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x19	; 25
	.db #0x31	; 49	'1'
	.db #0x15	; 21
	.db #0xbc	; 188
	.db #0xb4	; 180
	.db #0xec	; 236
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xb4	; 180
	.db #0xbe	; 190
	.db #0xb4	; 180
	.db #0xb1	; 177
	.db #0x4b	; 75	'K'
	.db #0x1e	; 30
	.db #0xb4	; 180
	.db #0xec	; 236
	.db #0x15	; 21
	.db #0x1a	; 26
	.db #0xce	; 206
	.db #0x4a	; 74	'J'
	.db #0xc4	; 196
	.db #0x4c	; 76	'L'
	.db #0xdc	; 220
	.db #0xc4	; 196
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0xc0	; 192
	.db #0xfc	; 252
	.db #0xfd	; 253
	.db #0x01	; 1
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x00	; 0
	.db #0x16	; 22
	.db #0x45	; 69	'E'
	.db #0x61	; 97	'a'
	.db #0x1c	; 28
	.db #0xe6	; 230
	.db #0xb6	; 182
	.db #0xe6	; 230
	.db #0x61	; 97	'a'
	.db #0xe6	; 230
	.db #0x6e	; 110	'n'
	.db #0x6e	; 110	'n'
	.db #0xe6	; 230
	.db #0xe6	; 230
	.db #0xf6	; 246
	.db #0xb6	; 182
	.db #0xe6	; 230
	.db #0xf6	; 246
	.db #0xe6	; 230
	.db #0x1b	; 27
	.db #0x6e	; 110	'n'
	.db #0xe1	; 225
	.db #0xe6	; 230
	.db #0xf6	; 246
	.db #0xe6	; 230
	.db #0xe6	; 230
	.db #0xb1	; 177
	.db #0xf6	; 246
	.db #0x1e	; 30
	.db #0xf6	; 246
	.db #0x1b	; 27
	.db #0xb1	; 177
	.db #0xb6	; 182
	.db #0x6e	; 110	'n'
	.db #0x80	; 128
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0xc0	; 192
	.db #0xfe	; 254
	.db #0xac	; 172
	.db #0x04	; 4
	.db #0xa1	; 161
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0xc0	; 192
	.db #0xff	; 255
	.db #0xfa	; 250
	.db #0x06	; 6
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x81	; 129
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x61	; 97	'a'
	.db #0x81	; 129
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x0e	; 14
	.db #0x16	; 22
	.db #0xb6	; 182
	.db #0xb6	; 182
	.db #0x18	; 24
	.db #0x81	; 129
	.db #0x19	; 25
	.db #0xe6	; 230
	.db #0xe6	; 230
	.db #0xa6	; 166
	.db #0x6e	; 110	'n'
	.db #0x6b	; 107	'k'
	.db #0x1e	; 30
	.db #0xbc	; 188
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x42	; 66	'B'
	.db #0x16	; 22
	.db #0x00	; 0
	.db #0xa1	; 161
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0xa0	; 160
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x03	; 3
	.db #0x41	; 65	'A'
	.db #0xe1	; 225
	.db #0x31	; 49	'1'
	.db #0x51	; 81	'Q'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0xb4	; 180
	.db #0xdc	; 220
	.db #0xc4	; 196
	.db #0x1c	; 28
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x07	; 7
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0xa1	; 161
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x01	; 1
	.db #0xc1	; 193
	.db #0x21	; 33
	.db #0x43	; 67	'C'
	.db #0xc1	; 193
	.db #0x12	; 18
	.db #0x1c	; 28
	.db #0x1e	; 30
	.db #0x1c	; 28
	.db #0xe4	; 228
	.db #0xa1	; 161
	.db #0xe1	; 225
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xe1	; 225
	.db #0x6c	; 108	'l'
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x15	; 21
	.db #0x1b	; 27
	.db #0x15	; 21
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x45	; 69	'E'
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x01	; 1
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x43	; 67	'C'
	.db #0x11	; 17
	.db #0x13	; 19
	.db #0xf6	; 246
	.db #0xf6	; 246
	.db #0xb1	; 177
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0xb1	; 177
	.db #0xb1	; 177
	.db #0xe1	; 225
	.db #0x1e	; 30
	.db #0xf6	; 246
	.db #0x19	; 25
	.db #0x19	; 25
	.db #0xe6	; 230
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x6b	; 107	'k'
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x0c	; 12
	.db #0xb6	; 182
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0xb6	; 182
	.db #0xb1	; 177
	.db #0xb1	; 177
	.db #0x1b	; 27
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x00	; 0
	.db #0x11	; 17
	.db #0x43	; 67	'C'
	.db #0x61	; 97	'a'
	.db #0x02	; 2
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x81	; 129
	.db #0x05	; 5
	.db #0x61	; 97	'a'
	.db #0xb1	; 177
	.db #0xe1	; 225
	.db #0xe6	; 230
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x07	; 7
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0x48	; 72	'H'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x06	; 6
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0xe1	; 225
	.db #0xa1	; 161
	.db #0xb1	; 177
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1f	; 31
	.db #0x1b	; 27
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0xe1	; 225
	.db #0x00	; 0
	.db #0xb1	; 177
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x80	; 128
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x12	; 18
	.db #0x12	; 18
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x69	; 105	'i'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0xc0	; 192
	.db #0xff	; 255
	.db #0x74	; 116	't'
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x6f	; 111	'o'
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0xf6	; 246
	.db #0xef	; 239
	.db #0x4e	; 78	'N'
	.db #0x6a	; 106	'j'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x19	; 25
	.db #0x19	; 25
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x61	; 97	'a'
	.db #0x11	; 17
	.db #0xa6	; 166
	.db #0x66	; 102	'f'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0x91	; 145
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x49	; 73	'I'
	.db #0x11	; 17
	.db #0x06	; 6
	.db #0xb1	; 177
	.db #0xe1	; 225
	.db #0xb1	; 177
	.db #0xf1	; 241
	.db #0xb1	; 177
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x45	; 69	'E'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0xe1	; 225
	.db #0x61	; 97	'a'
	.db #0xa1	; 161
	.db #0x80	; 128
	.db #0xa1	; 161
	.db #0x91	; 145
	.db #0x00	; 0
	.db #0xe1	; 225
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x13	; 19
	.db #0x1c	; 28
	.db #0x44	; 68	'D'
	.db #0x11	; 17
	.db #0x16	; 22
	.db #0x31	; 49	'1'
	.db #0x1c	; 28
	.db #0x51	; 81	'Q'
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x14	; 20
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x15	; 21
	.db #0x11	; 17
	.db #0xc1	; 193
	.db #0x41	; 65	'A'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x40	; 64
	.db #0x1e	; 30
	.db #0x07	; 7
	.db #0x14	; 20
	.db #0xc1	; 193
	.db #0x51	; 81	'Q'
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x1e	; 30
	.db #0xe1	; 225
	.db #0x14	; 20
	.db #0x45	; 69	'E'
	.db #0x61	; 97	'a'
	.db #0xc1	; 193
	.db #0xff	; 255
	.db #0x9e	; 158
	.db #0xc1	; 193
	.db #0xfd	; 253
	.db #0x23	; 35
	.db #0xc0	; 192
	.db #0xfb	; 251
	.db #0x55	; 85	'U'
	.db #0xc0	; 192
	.db #0xfb	; 251
	.db #0x97	; 151
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x1e	; 30
	.db #0x13	; 19
	.db #0x1f	; 31
	.db #0x1e	; 30
	.db #0xe1	; 225
	.db #0xec	; 236
	.db #0xe1	; 225
	.db #0x11	; 17
	.db #0x41	; 65	'A'
	.db #0x31	; 49	'1'
	.db #0x51	; 81	'Q'
	.db #0xc1	; 193
	.db #0x61	; 97	'a'
	.db #0x14	; 20
	.db #0x1c	; 28
	.db #0x11	; 17
	.db #0x51	; 81	'Q'
	.db #0xe1	; 225
	.db #0x31	; 49	'1'
	.db #0x51	; 81	'Q'
	.db #0x15	; 21
	.db #0x13	; 19
	.db #0x1e	; 30
	.db #0xe1	; 225
	.db #0x1b	; 27
	.db #0x1f	; 31
	.db #0x1e	; 30
	.db #0x15	; 21
	.db #0x19	; 25
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0xe1	; 225
	.db #0x04	; 4
	.db #0xec	; 236
	.db #0x11	; 17
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0xc2	; 194
	.db #0xfb	; 251
	.db #0xc8	; 200
	.db #0x01	; 1
	.db #0x16	; 22
	.db #0x19	; 25
	.db #0x41	; 65	'A'
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x08	; 8
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x18	; 24
	.db #0x1b	; 27
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0xb1	; 177
	.db #0xb6	; 182
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0xc6	; 198
	.db #0xfc	; 252
	.db #0x41	; 65	'A'
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x02	; 2
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x13	; 19
	.db #0x40	; 64
	.db #0xc1	; 193
	.db #0xc0	; 192
	.db #0xfc	; 252
	.db #0x65	; 101	'e'
	.db #0x40	; 64
	.db #0xe1	; 225
	.db #0x02	; 2
	.db #0xc1	; 193
	.db #0x51	; 81	'Q'
	.db #0xc1	; 193
	.db #0x40	; 64
	.db #0xe1	; 225
	.db #0x04	; 4
	.db #0x31	; 49	'1'
	.db #0xe1	; 225
	.db #0xb1	; 177
	.db #0xf1	; 241
	.db #0xc1	; 193
	.db #0x40	; 64
	.db #0x11	; 17
	.db #0x04	; 4
	.db #0x12	; 18
	.db #0x13	; 19
	.db #0x1e	; 30
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x45	; 69	'E'
	.db #0x11	; 17
	.db #0x42	; 66	'B'
	.db #0x61	; 97	'a'
	.db #0x09	; 9
	.db #0x16	; 22
	.db #0xa6	; 166
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x81	; 129
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x05	; 5
	.db #0x66	; 102	'f'
	.db #0xa6	; 166
	.db #0x6a	; 106	'j'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0xc1	; 193
	.db #0xfc	; 252
	.db #0xc5	; 197
	.db #0x04	; 4
	.db #0x91	; 145
	.db #0x61	; 97	'a'
	.db #0x91	; 145
	.db #0xa1	; 161
	.db #0x91	; 145
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0xc0	; 192
	.db #0xfc	; 252
	.db #0xcc	; 204
	.db #0x10	; 16
	.db #0x51	; 81	'Q'
	.db #0xbc	; 188
	.db #0xb4	; 180
	.db #0xe6	; 230
	.db #0x6c	; 108	'l'
	.db #0xe1	; 225
	.db #0xc1	; 193
	.db #0xc4	; 196
	.db #0x4a	; 74	'J'
	.db #0xce	; 206
	.db #0x14	; 20
	.db #0x4c	; 76	'L'
	.db #0x91	; 145
	.db #0xa1	; 161
	.db #0xe1	; 225
	.db #0x1a	; 26
	.db #0x16	; 22
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x41	; 65	'A'
	.db #0x16	; 22
	.db #0x41	; 65	'A'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x21	; 33
	.db #0x42	; 66	'B'
	.db #0x11	; 17
	.db #0x02	; 2
	.db #0xe1	; 225
	.db #0x1e	; 30
	.db #0xec	; 236
	.db #0x42	; 66	'B'
	.db #0xc1	; 193
	.db #0x18	; 24
	.db #0xe1	; 225
	.db #0x1c	; 28
	.db #0x4e	; 78	'N'
	.db #0xbc	; 188
	.db #0x4e	; 78	'N'
	.db #0xc1	; 193
	.db #0x51	; 81	'Q'
	.db #0x31	; 49	'1'
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1e	; 30
	.db #0x4b	; 75	'K'
	.db #0xcb	; 203
	.db #0xdc	; 220
	.db #0xce	; 206
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0xc1	; 193
	.db #0x21	; 33
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x16	; 22
	.db #0x42	; 66	'B'
	.db #0x61	; 97	'a'
	.db #0x0c	; 12
	.db #0x11	; 17
	.db #0x11	; 17
	.db #0xc6	; 198
	.db #0xc6	; 198
	.db #0xa4	; 164
	.db #0x61	; 97	'a'
	.db #0x31	; 49	'1'
	.db #0x76	; 118	'v'
	.db #0x16	; 22
	.db #0xf6	; 246
	.db #0x16	; 22
	.db #0x64	; 100	'd'
	.db #0xa6	; 166
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x39	; 57	'9'
	.db #0xa1	; 161
	.db #0x16	; 22
	.db #0xa1	; 161
	.db #0xc1	; 193
	.db #0xd1	; 209
	.db #0xa1	; 161
	.db #0xe1	; 225
	.db #0xe1	; 225
	.db #0x51	; 81	'Q'
	.db #0xe1	; 225
	.db #0x56	; 86	'V'
	.db #0xa6	; 166
	.db #0xb6	; 182
	.db #0xe6	; 230
	.db #0xc6	; 198
	.db #0xc6	; 198
	.db #0x1e	; 30
	.db #0xce	; 206
	.db #0xe1	; 225
	.db #0x51	; 81	'Q'
	.db #0xec	; 236
	.db #0xb4	; 180
	.db #0xbc	; 188
	.db #0x4e	; 78	'N'
	.db #0x1c	; 28
	.db #0x41	; 65	'A'
	.db #0x51	; 81	'Q'
	.db #0xec	; 236
	.db #0xb4	; 180
	.db #0xb4	; 180
	.db #0xec	; 236
	.db #0x31	; 49	'1'
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xce	; 206
	.db #0xce	; 206
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x15	; 21
	.db #0x4b	; 75	'K'
	.db #0xdc	; 220
	.db #0xb4	; 180
	.db #0x1c	; 28
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xc4	; 196
	.db #0x4c	; 76	'L'
	.db #0xc5	; 197
	.db #0xdc	; 220
	.db #0x4c	; 76	'L'
	.db #0xb6	; 182
	.db #0xb1	; 177
	.db #0xb1	; 177
	.db #0xe6	; 230
	.db #0x6e	; 110	'n'
	.db #0x6f	; 111	'o'
	.db #0x16	; 22
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0x16	; 22
	.db #0x09	; 9
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x11	; 17
	.db #0x21	; 33
	.db #0x6c	; 108	'l'
	.db #0xb4	; 180
	.db #0xdc	; 220
	.db #0xc4	; 196
	.db #0x1c	; 28
	.db #0x1a	; 26
	.db #0x40	; 64
	.db #0x61	; 97	'a'
	.db #0x0f	; 15
	.db #0xec	; 236
	.db #0xd1	; 209
	.db #0xb1	; 177
	.db #0x41	; 65	'A'
	.db #0xc1	; 193
	.db #0xe1	; 225
	.db #0x6c	; 108	'l'
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x15	; 21
	.db #0x1b	; 27
	.db #0x15	; 21
	.db #0x1c	; 28
	.db #0xc1	; 193
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0x81	; 129
	.db #0x00	; 0
	.db #0x61	; 97	'a'
	.db #0x40	; 64
	.db #0xa1	; 161
	.db #0xc0	; 192
	.db #0xfb	; 251
	.db #0x45	; 69	'E'
	.db #0x08	; 8
	.db #0x16	; 22
	.db #0x1e	; 30
	.db #0x16	; 22
	.db #0x1f	; 31
	.db #0x21	; 33
	.db #0x61	; 97	'a'
	.db #0x16	; 22
	.db #0x16	; 22
	.db #0x11	; 17
	.db #0xc0	; 192
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x00	; 0
_chuckpat:
	.db #0x54	; 84	'T'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x4a	; 74	'J'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x20	; 32
	.db #0x43	; 67	'C'
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x08	; 8
	.db #0x64	; 100	'd'
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x18	; 24
	.db #0x4b	; 75	'K'
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x6f	; 111	'o'
	.db #0x36	; 54	'6'
	.db #0x70	; 112	'p'
	.db #0x74	; 116	't'
	.db #0x1f	; 31
	.db #0x08	; 8
	.db #0x63	; 99	'c'
	.db #0x4c	; 76	'L'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x0c	; 12
	.db #0x7e	; 126
	.db #0x3e	; 62
	.db #0x11	; 17
	.db #0x66	; 102	'f'
	.db #0x37	; 55	'7'
	.db #0x10	; 16
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x7f	; 127
	.db #0x7e	; 126
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x03	; 3
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x0f	; 15
	.db #0x1f	; 31
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x12	; 18
	.db #0x40	; 64
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x3f	; 63
	.db #0x7e	; 126
	.db #0x4d	; 77	'M'
	.db #0x0d	; 13
	.db #0x7b	; 123
	.db #0x7f	; 127
	.db #0x37	; 55	'7'
	.db #0x17	; 23
	.db #0x73	; 115	's'
	.db #0x31	; 49	'1'
	.db #0x39	; 57	'9'
	.db #0x6c	; 108	'l'
	.db #0x6c	; 108	'l'
	.db #0x6e	; 110	'n'
	.db #0x4e	; 78	'N'
	.db #0x69	; 105	'i'
	.db #0x4b	; 75	'K'
	.db #0x00	; 0
	.db #0x0c	; 12
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x1e	; 30
	.db #0x3c	; 60
	.db #0x07	; 7
	.db #0x1f	; 31
	.db #0x3f	; 63
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x7f	; 127
	.db #0x46	; 70	'F'
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x0e	; 14
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x27	; 39
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x5f	; 95
	.db #0x37	; 55	'7'
	.db #0x0e	; 14
	.db #0x60	; 96
	.db #0x7f	; 127
	.db #0x08	; 8
	.db #0x16	; 22
	.db #0x1e	; 30
	.db #0x78	; 120	'x'
	.db #0x7c	; 124
	.db #0x7f	; 127
	.db #0x05	; 5
	.db #0x27	; 39
	.db #0x27	; 39
	.db #0x17	; 23
	.db #0x17	; 23
	.db #0x07	; 7
	.db #0x63	; 99	'c'
	.db #0x0c	; 12
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x3e	; 62
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x6e	; 110	'n'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x06	; 6
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x3c	; 60
	.db #0x1e	; 30
	.db #0x0f	; 15
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x1a	; 26
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x1f	; 31
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x0b	; 11
	.db #0x1a	; 26
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x78	; 120	'x'
	.db #0x19	; 25
	.db #0x50	; 80	'P'
	.db #0x3e	; 62
	.db #0x7e	; 126
	.db #0x47	; 71	'G'
	.db #0x7c	; 124
	.db #0x00	; 0
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x67	; 103	'g'
	.db #0x47	; 71	'G'
	.db #0x3c	; 60
	.db #0x04	; 4
	.db #0x43	; 67	'C'
	.db #0x00	; 0
	.db #0x12	; 18
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x18	; 24
	.db #0x7b	; 123
	.db #0x77	; 119	'w'
	.db #0x66	; 102	'f'
	.db #0x28	; 40
	.db #0x07	; 7
	.db #0x0f	; 15
	.db #0x01	; 1
	.db #0x19	; 25
	.db #0x08	; 8
	.db #0x4e	; 78	'N'
	.db #0x00	; 0
	.db #0x0e	; 14
	.db #0x78	; 120	'x'
	.db #0x3e	; 62
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x03	; 3
	.db #0x26	; 38
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x0f	; 15
	.db #0x63	; 99	'c'
	.db #0x40	; 64
	.db #0x05	; 5
	.db #0x01	; 1
	.db #0x23	; 35
	.db #0x0b	; 11
	.db #0x00	; 0
	.db #0x49	; 73	'I'
	.db #0x3a	; 58
	.db #0x4c	; 76	'L'
	.db #0x7d	; 125
	.db #0x64	; 100	'd'
	.db #0x1c	; 28
	.db #0x18	; 24
	.db #0x17	; 23
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x3f	; 63
	.db #0x1f	; 31
	.db #0x20	; 32
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x0f	; 15
	.db #0x1e	; 30
	.db #0x18	; 24
	.db #0x30	; 48	'0'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x42	; 66	'B'
	.db #0x41	; 65	'A'
	.db #0x20	; 32
	.db #0x2f	; 47
	.db #0x60	; 96
	.db #0x3f	; 63
	.db #0x1c	; 28
	.db #0x16	; 22
	.db #0x21	; 33
	.db #0x63	; 99	'c'
	.db #0x59	; 89	'Y'
	.db #0x23	; 35
	.db #0x01	; 1
	.db #0x7e	; 126
	.db #0x18	; 24
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x7d	; 125
	.db #0x7c	; 124
	.db #0x7e	; 126
	.db #0x26	; 38
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x7d	; 125
	.db #0x1f	; 31
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x00	; 0
	.db #0x39	; 57	'9'
	.db #0x79	; 121	'y'
	.db #0x59	; 89	'Y'
	.db #0x79	; 121	'y'
	.db #0x0c	; 12
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x00	; 0
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x71	; 113	'q'
	.db #0x61	; 97	'a'
	.db #0x0c	; 12
	.db #0x19	; 25
	.db #0x19	; 25
	.db #0x04	; 4
	.db #0x73	; 115	's'
	.db #0x33	; 51	'3'
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x1a	; 26
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x39	; 57	'9'
	.db #0x38	; 56	'8'
	.db #0x1b	; 27
	.db #0x39	; 57	'9'
	.db #0x78	; 120	'x'
	.db #0x6c	; 108	'l'
	.db #0x3a	; 58
	.db #0x45	; 69	'E'
	.db #0x35	; 53	'5'
	.db #0x3b	; 59
	.db #0x00	; 0
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x3f	; 63
	.db #0x42	; 66	'B'
	.db #0x40	; 64
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x78	; 120	'x'
	.db #0x38	; 56	'8'
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x1a	; 26
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x24	; 36
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x19	; 25
	.db #0x31	; 49	'1'
	.db #0x44	; 68	'D'
	.db #0x4e	; 78	'N'
	.db #0x7c	; 124
	.db #0x7c	; 124
	.db #0x1f	; 31
	.db #0x71	; 113	'q'
	.db #0x7c	; 124
	.db #0x63	; 99	'c'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x7b	; 123
	.db #0x77	; 119	'w'
	.db #0x37	; 55	'7'
	.db #0x3b	; 59
	.db #0x1b	; 27
	.db #0x39	; 57	'9'
	.db #0x06	; 6
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x0f	; 15
	.db #0x33	; 51	'3'
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x06	; 6
	.db #0x07	; 7
	.db #0x7c	; 124
	.db #0x7f	; 127
	.db #0x62	; 98	'b'
	.db #0x11	; 17
	.db #0x68	; 104	'h'
	.db #0x6d	; 109	'm'
	.db #0x19	; 25
	.db #0x19	; 25
	.db #0x03	; 3
	.db #0x1f	; 31
	.db #0x03	; 3
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x70	; 112	'p'
	.db #0x49	; 73	'I'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x03	; 3
	.db #0x08	; 8
	.db #0x07	; 7
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x17	; 23
	.db #0x30	; 48	'0'
	.db #0x78	; 120	'x'
	.db #0x07	; 7
	.db #0x1f	; 31
	.db #0x40	; 64
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x09	; 9
	.db #0x3f	; 63
	.db #0x5f	; 95
	.db #0x38	; 56	'8'
	.db #0x38	; 56	'8'
	.db #0x7c	; 124
	.db #0x30	; 48	'0'
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x10	; 16
	.db #0x10	; 16
	.db #0x40	; 64
	.db #0x18	; 24
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x0e	; 14
	.db #0x0c	; 12
	.db #0x01	; 1
	.db #0x31	; 49	'1'
	.db #0x70	; 112	'p'
	.db #0x38	; 56	'8'
	.db #0x60	; 96
	.db #0x20	; 32
	.db #0x38	; 56	'8'
	.db #0x09	; 9
	.db #0x1b	; 27
	.db #0x4c	; 76	'L'
	.db #0x18	; 24
	.db #0x4f	; 79	'O'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x02	; 2
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x14	; 20
	.db #0x01	; 1
	.db #0x0e	; 14
	.db #0x59	; 89	'Y'
	.db #0x69	; 105	'i'
	.db #0x51	; 81	'Q'
	.db #0x00	; 0
	.db #0x07	; 7
	.db #0x5b	; 91
	.db #0x0f	; 15
	.db #0x2f	; 47
	.db #0x71	; 113	'q'
	.db #0x69	; 105	'i'
	.db #0x6d	; 109	'm'
	.db #0x45	; 69	'E'
	.db #0x19	; 25
	.db #0x43	; 67	'C'
	.db #0x43	; 67	'C'
	.db #0x7d	; 125
	.db #0x7e	; 126
	.db #0x7e	; 126
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x06	; 6
	.db #0x3f	; 63
	.db #0x30	; 48	'0'
	.db #0x61	; 97	'a'
	.db #0x61	; 97	'a'
	.db #0x6a	; 106	'j'
	.db #0x70	; 112	'p'
	.db #0x38	; 56	'8'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x0b	; 11
	.db #0x60	; 96
	.db #0x5f	; 95
	.db #0x10	; 16
	.db #0x20	; 32
	.db #0x28	; 40
	.db #0x18	; 24
	.db #0x1c	; 28
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x04	; 4
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x08	; 8
	.db #0x00	; 0
	.db #0x0c	; 12
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x42	; 66	'B'
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x3f	; 63
	.db #0x19	; 25
	.db #0x1f	; 31
	.db #0x79	; 121	'y'
	.db #0x06	; 6
	.db #0x78	; 120	'x'
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x0a	; 10
	.db #0x03	; 3
	.db #0x76	; 118	'v'
	.db #0x61	; 97	'a'
	.db #0x3b	; 59
	.db #0x3e	; 62
	.db #0x70	; 112	'p'
	.db #0x30	; 48	'0'
	.db #0x78	; 120	'x'
	.db #0x0e	; 14
	.db #0x7b	; 123
	.db #0x64	; 100	'd'
	.db #0x7e	; 126
	.db #0x26	; 38
	.db #0x3f	; 63
	.db #0x04	; 4
	.db #0x0c	; 12
	.db #0x48	; 72	'H'
	.db #0x29	; 41
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x7f	; 127
	.db #0x10	; 16
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x0f	; 15
	.db #0x1f	; 31
	.db #0x1c	; 28
	.db #0x3c	; 60
	.db #0x1c	; 28
	.db #0x0c	; 12
	.db #0x5f	; 95
	.db #0x3f	; 63
	.db #0x7c	; 124
	.db #0x3f	; 63
	.db #0x2b	; 43
	.db #0x2f	; 47
	.db #0x20	; 32
	.db #0x37	; 55	'7'
	.db #0x7e	; 126
	.db #0x40	; 64
	.db #0x3e	; 62
	.db #0x1b	; 27
	.db #0x3f	; 63
	.db #0x3f	; 63
	.db #0x37	; 55	'7'
	.db #0x23	; 35
	.db #0x2f	; 47
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x78	; 120	'x'
	.db #0x30	; 48	'0'
	.db #0x60	; 96
	.db #0x3f	; 63
	.db #0x7d	; 125
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x0e	; 14
	.db #0x1e	; 30
	.db #0x3e	; 62
	.db #0x7c	; 124
	.db #0x07	; 7
	.db #0x30	; 48	'0'
	.db #0x60	; 96
	.db #0x70	; 112	'p'
	.db #0x70	; 112	'p'
	.db #0x27	; 39
	.db #0x0f	; 15
	.db #0x0e	; 14
	.db #0x0a	; 10
	.db #0x0e	; 14
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x26	; 38
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x60	; 96
	.db #0x70	; 112	'p'
	.db #0x07	; 7
	.db #0x06	; 6
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x67	; 103	'g'
	.db #0x78	; 120	'x'
	.db #0x0a	; 10
	.db #0x2b	; 43
	.db #0x08	; 8
	.db #0x5a	; 90	'Z'
	.db #0x04	; 4
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x1c	; 28
	.db #0x1e	; 30
	.db #0x71	; 113	'q'
	.db #0x71	; 113	'q'
	.db #0x46	; 70	'F'
	.db #0x47	; 71	'G'
	.db #0x27	; 39
	.db #0x10	; 16
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x01	; 1
	.db #0x0d	; 13
	.db #0x05	; 5
	.db #0x05	; 5
	.db #0x07	; 7
	.db #0x40	; 64
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x46	; 70	'F'
	.db #0x00	; 0
	.db #0x21	; 33
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x1e	; 30
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x0f	; 15
	.db #0x1c	; 28
	.db #0x38	; 56	'8'
	.db #0x30	; 48	'0'
	.db #0x1f	; 31
	.db #0x1f	; 31
	.db #0x09	; 9
	.db #0x0e	; 14
	.db #0x40	; 64
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x78	; 120	'x'
	.db #0x1b	; 27
	.db #0x6e	; 110	'n'
	.db #0x06	; 6
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x45	; 69	'E'
	.db #0x7c	; 124
	.db #0x63	; 99	'c'
	.db #0x3b	; 59
	.db #0x0f	; 15
	.db #0x78	; 120	'x'
	.db #0x1c	; 28
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x0e	; 14
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x30	; 48	'0'
	.db #0x6f	; 111	'o'
	.db #0x0c	; 12
	.db #0x04	; 4
	.db #0x06	; 6
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x10	; 16
	.db #0x01	; 1
	.db #0x6f	; 111	'o'
	.db #0x6f	; 111	'o'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x42	; 66	'B'
	.db #0x01	; 1
	.db #0x20	; 32
	.db #0x1f	; 31
	.db #0x7c	; 124
	.db #0x38	; 56	'8'
	.db #0x47	; 71	'G'
	.db #0x4e	; 78	'N'
	.db #0x7e	; 126
	.db #0x03	; 3
	.db #0x70	; 112	'p'
	.db #0x62	; 98	'b'
	.db #0x67	; 103	'g'
	.db #0x18	; 24
	.db #0x38	; 56	'8'
	.db #0x24	; 36
	.db #0x32	; 50	'2'
	.db #0x73	; 115	's'
	.db #0x79	; 121	'y'
	.db #0x4a	; 74	'J'
	.db #0x7f	; 127
	.db #0x2e	; 46
	.db #0x20	; 32
	.db #0x7f	; 127
	.db #0x03	; 3
	.db #0x61	; 97	'a'
	.db #0x0f	; 15
	.db #0x11	; 17
	.db #0x5c	; 92
	.db #0x3c	; 60
	.db #0x07	; 7
	.db #0x0a	; 10
	.db #0x60	; 96
	.db #0x6e	; 110	'n'
	.db #0x7e	; 126
	.db #0x7f	; 127
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x25	; 37
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x0c	; 12
	.db #0x0e	; 14
	.db #0x06	; 6
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x10	; 16
	.db #0x14	; 20
	.db #0x17	; 23
	.db #0x0e	; 14
	.db #0x68	; 104	'h'
	.db #0x6c	; 108	'l'
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x79	; 121	'y'
	.db #0x3c	; 60
	.db #0x7c	; 124
	.db #0x55	; 85	'U'
	.db #0x3e	; 62
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x56	; 86	'V'
	.db #0x02	; 2
	.db #0x13	; 19
	.db #0x73	; 115	's'
	.db #0x1c	; 28
	.db #0x79	; 121	'y'
	.db #0x08	; 8
	.db #0x78	; 120	'x'
	.db #0x60	; 96
	.db #0x30	; 48	'0'
	.db #0x1f	; 31
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x06	; 6
	.db #0x01	; 1
	.db #0x43	; 67	'C'
	.db #0x00	; 0
	.db #0x26	; 38
	.db #0x61	; 97	'a'
	.db #0x0f	; 15
	.db #0x77	; 119	'w'
	.db #0x11	; 17
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x3c	; 60
	.db #0x2d	; 45
	.db #0x5d	; 93
	.db #0x69	; 105	'i'
	.db #0x6a	; 106	'j'
	.db #0x79	; 121	'y'
	.db #0x3d	; 61
	.db #0x39	; 57	'9'
	.db #0x1f	; 31
	.db #0x1f	; 31
	.db #0x0e	; 14
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x0f	; 15
	.db #0x1c	; 28
	.db #0x07	; 7
	.db #0x7c	; 124
	.db #0x27	; 39
	.db #0x27	; 39
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x10	; 16
	.db #0x5f	; 95
	.db #0x17	; 23
	.db #0x05	; 5
	.db #0x03	; 3
	.db #0x06	; 6
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x43	; 67	'C'
	.db #0x00	; 0
	.db #0x2d	; 45
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x40	; 64
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x0c	; 12
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x06	; 6
	.db #0x30	; 48	'0'
	.db #0x60	; 96
	.db #0x7f	; 127
	.db #0x7c	; 124
	.db #0x7c	; 124
	.db #0x09	; 9
	.db #0x08	; 8
	.db #0x02	; 2
	.db #0x06	; 6
	.db #0x0c	; 12
	.db #0x08	; 8
	.db #0x18	; 24
	.db #0x10	; 16
	.db #0x30	; 48	'0'
	.db #0x20	; 32
	.db #0x60	; 96
	.db #0x7d	; 125
	.db #0x19	; 25
	.db #0x15	; 21
	.db #0x0a	; 10
	.db #0x65	; 101	'e'
	.db #0x65	; 101	'e'
	.db #0x03	; 3
	.db #0x32	; 50	'2'
	.db #0x53	; 83	'S'
	.db #0x56	; 86	'V'
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0x55	; 85	'U'
	.db #0x2a	; 42
	.db #0x25	; 37
	.db #0x0f	; 15
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x0f	; 15
	.db #0x80	; 128
	.db #0x6f	; 111	'o'
	.db #0x2f	; 47
	.db #0x00	; 0
	.db #0x6f	; 111	'o'
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x2d	; 45
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x2f	; 47
	.db #0x2f	; 47
	.db #0x6f	; 111	'o'
	.db #0x30	; 48	'0'
	.db #0x5f	; 95
	.db #0x20	; 32
	.db #0x20	; 32
	.db #0x00	; 0
	.db #0x0e	; 14
	.db #0x78	; 120	'x'
	.db #0x1c	; 28
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x31	; 49	'1'
	.db #0x31	; 49	'1'
	.db #0x6e	; 110	'n'
	.db #0x5d	; 93
	.db #0x62	; 98	'b'
	.db #0x55	; 85	'U'
	.db #0x2a	; 42
	.db #0x35	; 53	'5'
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x78	; 120	'x'
	.db #0x5b	; 91
	.db #0x0e	; 14
	.db #0x1c	; 28
	.db #0x0e	; 14
	.db #0x60	; 96
	.db #0x6c	; 108	'l'
	.db #0x7f	; 127
	.db #0x24	; 36
	.db #0x0c	; 12
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x0e	; 14
	.db #0x49	; 73	'I'
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x06	; 6
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x20	; 32
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x80	; 128
	.db #0x0f	; 15
	.db #0x0b	; 11
	.db #0x15	; 21
	.db #0x21	; 33
	.db #0x0e	; 14
	.db #0x50	; 80	'P'
	.db #0x61	; 97	'a'
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x0e	; 14
	.db #0x18	; 24
	.db #0x18	; 24
	.db #0x23	; 35
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x3e	; 62
	.db #0x0b	; 11
	.db #0x3b	; 59
	.db #0x67	; 103	'g'
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x19	; 25
	.db #0x3c	; 60
	.db #0x05	; 5
	.db #0x78	; 120	'x'
	.db #0x7e	; 126
	.db #0x27	; 39
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x1a	; 26
	.db #0x4f	; 79	'O'
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x70	; 112	'p'
	.db #0x38	; 56	'8'
	.db #0x60	; 96
	.db #0x30	; 48	'0'
	.db #0x1b	; 27
	.db #0x3b	; 59
	.db #0x3f	; 63
	.db #0x7d	; 125
	.db #0x55	; 85	'U'
	.db #0x55	; 85	'U'
	.db #0x4d	; 77	'M'
	.db #0x4b	; 75	'K'
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x20	; 32
	.db #0x40	; 64
	.db #0x30	; 48	'0'
	.db #0x05	; 5
	.db #0x18	; 24
	.db #0x18	; 24
	.db #0x63	; 99	'c'
	.db #0x06	; 6
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x42	; 66	'B'
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x02	; 2
	.db #0x24	; 36
	.db #0x7e	; 126
	.db #0x02	; 2
	.db #0x7d	; 125
	.db #0x7c	; 124
	.db #0x03	; 3
	.db #0x6c	; 108	'l'
	.db #0x40	; 64
	.db #0x40	; 64
	.db #0x0a	; 10
	.db #0x1e	; 30
	.db #0x70	; 112	'p'
	.db #0x0f	; 15
	.db #0x7e	; 126
	.db #0x7a	; 122	'z'
	.db #0x42	; 66	'B'
	.db #0x4a	; 74	'J'
	.db #0x32	; 50	'2'
	.db #0x21	; 33
	.db #0x7c	; 124
	.db #0x60	; 96
	.db #0x0f	; 15
	.db #0x70	; 112	'p'
	.db #0x0f	; 15
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x1f	; 31
	.db #0x5f	; 95
	.db #0x55	; 85	'U'
	.db #0x54	; 84	'T'
	.db #0x58	; 88	'X'
	.db #0x4f	; 79	'O'
	.db #0x4f	; 79	'O'
	.db #0x0f	; 15
	.db #0x30	; 48	'0'
	.db #0x70	; 112	'p'
	.db #0x0f	; 15
	.db #0x70	; 112	'p'
	.db #0x40	; 64
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x06	; 6
	.db #0x40	; 64
	.db #0x04	; 4
	.db #0x00	; 0
	.db #0x0c	; 12
	.db #0x43	; 67	'C'
	.db #0x40	; 64
	.db #0x01	; 1
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x1d	; 29
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x3a	; 58
	.db #0x3a	; 58
	.db #0x72	; 114	'r'
	.db #0x1a	; 26
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x0e	; 14
	.db #0x79	; 121	'y'
	.db #0x0b	; 11
	.db #0x60	; 96
	.db #0x70	; 112	'p'
	.db #0x73	; 115	's'
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x70	; 112	'p'
	.db #0x1e	; 30
	.db #0x7e	; 126
	.db #0x03	; 3
	.db #0x6f	; 111	'o'
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x07	; 7
	.db #0x3e	; 62
	.db #0x20	; 32
	.db #0x60	; 96
	.db #0xa0	; 160
	.db #0x60	; 96
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x06	; 6
	.db #0x07	; 7
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x14	; 20
	.db #0x54	; 84	'T'
	.db #0x07	; 7
	.db #0x38	; 56	'8'
	.db #0x17	; 23
	.db #0x0b	; 11
	.db #0x1f	; 31
	.db #0x78	; 120	'x'
	.db #0x0e	; 14
	.db #0x0e	; 14
	.db #0x21	; 33
	.db #0x63	; 99	'c'
	.db #0x43	; 67	'C'
	.db #0x38	; 56	'8'
	.db #0x30	; 48	'0'
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x1a	; 26
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x1e	; 30
	.db #0x0f	; 15
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x78	; 120	'x'
	.db #0x03	; 3
	.db #0x1f	; 31
	.db #0x06	; 6
	.db #0x1c	; 28
	.db #0x3f	; 63
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x11	; 17
	.db #0x78	; 120	'x'
	.db #0x20	; 32
	.db #0x38	; 56	'8'
	.db #0x3e	; 62
	.db #0x01	; 1
	.db #0x0b	; 11
	.db #0x07	; 7
	.db #0x17	; 23
	.db #0x02	; 2
	.db #0x78	; 120	'x'
	.db #0x10	; 16
	.db #0x45	; 69	'E'
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x73	; 115	's'
	.db #0x0c	; 12
	.db #0x33	; 51	'3'
	.db #0x3b	; 59
	.db #0x06	; 6
	.db #0x46	; 70	'F'
	.db #0x02	; 2
	.db #0x23	; 35
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x43	; 67	'C'
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x01	; 1
	.db #0x41	; 65	'A'
	.db #0x61	; 97	'a'
	.db #0x3d	; 61
	.db #0x1c	; 28
	.db #0x1e	; 30
	.db #0x17	; 23
	.db #0x03	; 3
	.db #0x78	; 120	'x'
	.db #0x0c	; 12
	.db #0x63	; 99	'c'
	.db #0x0f	; 15
	.db #0x60	; 96
	.db #0x7d	; 125
	.db #0x06	; 6
	.db #0x7f	; 127
	.db #0x49	; 73	'I'
	.db #0x75	; 117	'u'
	.db #0x15	; 21
	.db #0x2a	; 42
	.db #0x2a	; 42
	.db #0x55	; 85	'U'
	.db #0x29	; 41
	.db #0x01	; 1
	.db #0x2a	; 42
	.db #0x29	; 41
	.db #0x2c	; 44
	.db #0x03	; 3
	.db #0x0e	; 14
	.db #0x1c	; 28
	.db #0x03	; 3
	.db #0x0f	; 15
	.db #0x40	; 64
	.db #0x6f	; 111	'o'
	.db #0x0d	; 13
	.db #0x4f	; 79	'O'
	.db #0x20	; 32
	.db #0x60	; 96
	.db #0x60	; 96
	.db #0x00	; 0
	.db #0x18	; 24
	.db #0x18	; 24
	.db #0x11	; 17
	.db #0x31	; 49	'1'
	.db #0x60	; 96
	.db #0x61	; 97	'a'
	.db #0x3e	; 62
	.db #0x7d	; 125
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x3c	; 60
	.db #0x1d	; 29
	.db #0x1e	; 30
	.db #0x0e	; 14
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x79	; 121	'y'
	.db #0x3d	; 61
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x65	; 101	'e'
	.db #0x55	; 85	'U'
	.db #0x54	; 84	'T'
	.db #0x23	; 35
	.db #0x3c	; 60
	.db #0x23	; 35
	.db #0x2f	; 47
	.db #0x60	; 96
	.db #0x0c	; 12
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x1f	; 31
	.db #0x1e	; 30
	.db #0x1e	; 30
	.db #0x0e	; 14
	.db #0x0d	; 13
	.db #0x0e	; 14
	.db #0x06	; 6
	.db #0x40	; 64
	.db #0x60	; 96
	.db #0x05	; 5
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x41	; 65	'A'
	.db #0x06	; 6
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x0e	; 14
	.db #0x33	; 51	'3'
	.db #0x2c	; 44
	.db #0x3e	; 62
	.db #0x3e	; 62
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x70	; 112	'p'
	.db #0x78	; 120	'x'
	.db #0x39	; 57	'9'
	.db #0x7b	; 123
	.db #0x39	; 57	'9'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x67	; 103	'g'
	.db #0x5e	; 94
	.db #0x21	; 33
	.db #0x5f	; 95
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x1f	; 31
	.db #0x70	; 112	'p'
	.db #0x10	; 16
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x07	; 7
	.db #0x05	; 5
	.db #0x04	; 4
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x21	; 33
	.db #0x7f	; 127
	.db #0x10	; 16
	.db #0x78	; 120	'x'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x03	; 3
	.db #0x09	; 9
	.db #0x02	; 2
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x07	; 7
	.db #0x7f	; 127
	.db #0x1f	; 31
	.db #0x60	; 96
	.db #0x0d	; 13
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x03	; 3
	.db #0x4b	; 75	'K'
	.db #0x09	; 9
	.db #0x1f	; 31
	.db #0x40	; 64
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x17	; 23
	.db #0x6f	; 111	'o'
	.db #0x10	; 16
	.db #0x7f	; 127
	.db #0x7f	; 127
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x1c	; 28
	.db #0x00	; 0
	.db #0x73	; 115	's'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x7d	; 125
	.db #0x7d	; 125
	.db #0x7b	; 123
	.db #0x14	; 20
	.db #0x6e	; 110	'n'
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x0f	; 15
	.db #0x2f	; 47
	.db #0x6f	; 111	'o'
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x1f	; 31
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x7b	; 123
	.db #0x5e	; 94
	.db #0x33	; 51	'3'
	.db #0x7b	; 123
	.db #0x73	; 115	's'
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x7f	; 127
	.db #0x4f	; 79	'O'
	.db #0x07	; 7
	.db #0x60	; 96
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x04	; 4
	.db #0x0c	; 12
	.db #0x1e	; 30
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x49	; 73	'I'
	.db #0x00	; 0
	.db #0xa0	; 160
	.db #0x10	; 16
	.db #0x18	; 24
	.db #0x18	; 24
	.db #0x00	; 0
	.db #0x08	; 8
	.db #0x45	; 69	'E'
	.db #0x00	; 0
	.db #0x07	; 7
	.db #0x0c	; 12
	.db #0x3c	; 60
	.db #0x7c	; 124
	.db #0x7e	; 126
	.db #0x7c	; 124
	.db #0x7c	; 124
	.db #0x1e	; 30
	.db #0x06	; 6
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x44	; 68	'D'
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x60	; 96
	.db #0x07	; 7
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x02	; 2
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x2c	; 44
	.db #0x3f	; 63
	.db #0x3f	; 63
	.db #0x5f	; 95
	.db #0x37	; 55	'7'
	.db #0x71	; 113	'q'
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x06	; 6
	.db #0x0c	; 12
	.db #0x3f	; 63
	.db #0x0e	; 14
	.db #0x07	; 7
	.db #0x17	; 23
	.db #0x30	; 48	'0'
	.db #0x38	; 56	'8'
	.db #0x72	; 114	'r'
	.db #0x76	; 118	'v'
	.db #0x03	; 3
	.db #0x1c	; 28
	.db #0x7b	; 123
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x04	; 4
	.db #0x09	; 9
	.db #0x0b	; 11
	.db #0x1a	; 26
	.db #0x1c	; 28
	.db #0x24	; 36
	.db #0x78	; 120	'x'
	.db #0x19	; 25
	.db #0x50	; 80	'P'
	.db #0x3e	; 62
	.db #0x7e	; 126
	.db #0x47	; 71	'G'
	.db #0x7c	; 124
	.db #0x7c	; 124
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x47	; 71	'G'
	.db #0x47	; 71	'G'
	.db #0x3c	; 60
	.db #0x04	; 4
	.db #0x77	; 119	'w'
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x40	; 64
	.db #0x7f	; 127
	.db #0x37	; 55	'7'
	.db #0x26	; 38
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x00	; 0
	.db #0x12	; 18
	.db #0x13	; 19
	.db #0x13	; 19
	.db #0x0a	; 10
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x00	; 0
	.db #0x39	; 57	'9'
	.db #0x79	; 121	'y'
	.db #0x59	; 89	'Y'
	.db #0x79	; 121	'y'
	.db #0x0d	; 13
	.db #0x1f	; 31
	.db #0x1f	; 31
	.db #0x07	; 7
	.db #0x73	; 115	's'
	.db #0x67	; 103	'g'
	.db #0x0f	; 15
	.db #0x3f	; 63
	.db #0x1f	; 31
	.db #0x03	; 3
	.db #0x0f	; 15
	.db #0x1c	; 28
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x39	; 57	'9'
	.db #0x38	; 56	'8'
	.db #0x00	; 0
	.db #0x09	; 9
	.db #0x19	; 25
	.db #0x31	; 49	'1'
	.db #0x04	; 4
	.db #0x46	; 70	'F'
	.db #0x4c	; 76	'L'
	.db #0x71	; 113	'q'
	.db #0x1f	; 31
	.db #0x71	; 113	'q'
	.db #0x7c	; 124
	.db #0x63	; 99	'c'
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x7b	; 123
	.db #0x61	; 97	'a'
	.db #0x37	; 55	'7'
	.db #0x3b	; 59
	.db #0x1b	; 27
	.db #0x39	; 57	'9'
	.db #0x02	; 2
	.db #0x02	; 2
	.db #0x04	; 4
	.db #0x64	; 100	'd'
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x41	; 65	'A'
	.db #0x3f	; 63
	.db #0x30	; 48	'0'
	.db #0x5f	; 95
	.db #0x79	; 121	'y'
	.db #0x07	; 7
	.db #0x78	; 120	'x'
	.db #0x70	; 112	'p'
	.db #0x06	; 6
	.db #0x0b	; 11
	.db #0x01	; 1
	.db #0x76	; 118	'v'
	.db #0x39	; 57	'9'
	.db #0x3e	; 62
	.db #0x70	; 112	'p'
	.db #0x30	; 48	'0'
	.db #0x78	; 120	'x'
	.db #0x0e	; 14
	.db #0x7b	; 123
	.db #0x4a	; 74	'J'
	.db #0x7f	; 127
	.db #0x26	; 38
	.db #0x3f	; 63
	.db #0x06	; 6
	.db #0x0c	; 12
	.db #0x73	; 115	's'
	.db #0x2b	; 43
	.db #0x1b	; 27
	.db #0x2f	; 47
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x78	; 120	'x'
	.db #0x30	; 48	'0'
	.db #0x62	; 98	'b'
	.db #0x3c	; 60
	.db #0x78	; 120	'x'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x0e	; 14
	.db #0x1e	; 30
	.db #0x3e	; 62
	.db #0x7c	; 124
	.db #0x78	; 120	'x'
	.db #0x60	; 96
	.db #0x40	; 64
	.db #0x70	; 112	'p'
	.db #0x78	; 120	'x'
	.db #0x7c	; 124
	.db #0x0d	; 13
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x03	; 3
	.db #0x40	; 64
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x3f	; 63
	.db #0x1f	; 31
	.db #0x40	; 64
	.db #0x3f	; 63
	.db #0x45	; 69	'E'
	.db #0x00	; 0
	.db #0x21	; 33
	.db #0x70	; 112	'p'
	.db #0x78	; 120	'x'
	.db #0x1c	; 28
	.db #0x3c	; 60
	.db #0x61	; 97	'a'
	.db #0x39	; 57	'9'
	.db #0x48	; 72	'H'
	.db #0x03	; 3
	.db #0x63	; 99	'c'
	.db #0x67	; 103	'g'
	.db #0x18	; 24
	.db #0x38	; 56	'8'
	.db #0x24	; 36
	.db #0x32	; 50	'2'
	.db #0x0c	; 12
	.db #0x79	; 121	'y'
	.db #0x7f	; 127
	.db #0x2e	; 46
	.db #0x00	; 0
	.db #0x20	; 32
	.db #0x7f	; 127
	.db #0x03	; 3
	.db #0x61	; 97	'a'
	.db #0x0f	; 15
	.db #0x6e	; 110	'n'
	.db #0x3c	; 60
	.db #0x3c	; 60
	.db #0x07	; 7
	.db #0x0a	; 10
	.db #0x60	; 96
	.db #0x6e	; 110	'n'
	.db #0x7e	; 126
	.db #0x0c	; 12
	.db #0x0e	; 14
	.db #0x40	; 64
	.db #0x06	; 6
	.db #0x1e	; 30
	.db #0x07	; 7
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x10	; 16
	.db #0x14	; 20
	.db #0x17	; 23
	.db #0x0d	; 13
	.db #0x17	; 23
	.db #0x0f	; 15
	.db #0x1f	; 31
	.db #0x0f	; 15
	.db #0x79	; 121	'y'
	.db #0x3c	; 60
	.db #0x7c	; 124
	.db #0x4a	; 74	'J'
	.db #0x3e	; 62
	.db #0x3f	; 63
	.db #0x40	; 64
	.db #0x52	; 82	'R'
	.db #0x0f	; 15
	.db #0x19	; 25
	.db #0x71	; 113	'q'
	.db #0x1c	; 28
	.db #0x79	; 121	'y'
	.db #0x1c	; 28
	.db #0x70	; 112	'p'
	.db #0x60	; 96
	.db #0x7f	; 127
	.db #0x3f	; 63
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x41	; 65	'A'
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x42	; 66	'B'
	.db #0x00	; 0
	.db #0x38	; 56	'8'
	.db #0x0c	; 12
	.db #0x70	; 112	'p'
	.db #0x03	; 3
	.db #0x31	; 49	'1'
	.db #0x04	; 4
	.db #0x06	; 6
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x3c	; 60
	.db #0x50	; 80	'P'
	.db #0x7f	; 127
	.db #0x03	; 3
	.db #0x7e	; 126
	.db #0x3d	; 61
	.db #0x3a	; 58
	.db #0x3a	; 58
	.db #0x1f	; 31
	.db #0x1b	; 27
	.db #0x0e	; 14
	.db #0x1b	; 27
	.db #0x07	; 7
	.db #0x0f	; 15
	.db #0x60	; 96
	.db #0x3b	; 59
	.db #0x43	; 67	'C'
	.db #0x38	; 56	'8'
	.db #0x70	; 112	'p'
	.db #0x1c	; 28
	.db #0x3c	; 60
	.db #0x70	; 112	'p'
	.db #0x2f	; 47
	.db #0x77	; 119	'w'
	.db #0x0e	; 14
	.db #0x68	; 104	'h'
	.db #0x1c	; 28
	.db #0x1c	; 28
	.db #0x0f	; 15
	.db #0x06	; 6
	.db #0x00	; 0
	.db #0x00	; 0
	.db #0x03	; 3
	.db #0x07	; 7
	.db #0x78	; 120	'x'
	.db #0x5f	; 95
	.db #0x0e	; 14
	.db #0x0c	; 12
	.db #0x06	; 6
	.db #0x60	; 96
	.db #0x6c	; 108	'l'
	.db #0x7f	; 127
	.db #0x20	; 32
	.db #0x11	; 17
	.db #0x03	; 3
	.db #0x43	; 67	'C'
	.db #0x78	; 120	'x'
	.db #0x0e	; 14
	.db #0x01	; 1
	.db #0x40	; 64
	.db #0x07	; 7
	.db #0x26	; 38
	.db #0x0b	; 11
	.db #0x0b	; 11
	.db #0x0f	; 15
	.db #0x0b	; 11
	.db #0x07	; 7
	.db #0x43	; 67	'C'
	.db #0x71	; 113	'q'
	.db #0x02	; 2
	.db #0x03	; 3
	.db #0x06	; 6
	.db #0x3c	; 60
	.db #0x1c	; 28
	.db #0x18	; 24
	.db #0x5c	; 92
	.db #0x0c	; 12
	.db #0x0c	; 12
	.db #0x3e	; 62
	.db #0x0b	; 11
	.db #0x3b	; 59
	.db #0x18	; 24
	.db #0x3a	; 58
	.db #0x30	; 48	'0'
	.db #0x78	; 120	'x'
	.db #0x7e	; 126
	.db #0x27	; 39
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x1a	; 26
	.db #0x3b	; 59
	.db #0x7b	; 123
	.db #0x21	; 33
	.db #0x76	; 118	'v'
	.db #0x1f	; 31
	.db #0x3c	; 60
	.db #0x63	; 99	'c'
	.db #0x30	; 48	'0'
	.db #0x77	; 119	'w'
	.db #0x16	; 22
	.db #0x2e	; 46
	.db #0x40	; 64
	.db #0x55	; 85	'U'
	.db #0x02	; 2
	.db #0x4d	; 77	'M'
	.db #0x54	; 84	'T'
	.db #0x70	; 112	'p'
	.db #0x40	; 64
	.db #0x7c	; 124
	.db #0x3b	; 59
	.db #0x0f	; 15
	.db #0x7f	; 127
	.db #0x23	; 35
	.db #0x03	; 3
	.db #0x0d	; 13
	.db #0x19	; 25
	.db #0x3b	; 59
	.db #0x63	; 99	'c'
	.db #0x0c	; 12
	.db #0x00	; 0
	.db #0x02	; 2
	.db #0x70	; 112	'p'
	.db #0x06	; 6
	.db #0x0c	; 12
	.db #0x2c	; 44
	.db #0x07	; 7
	.db #0x38	; 56	'8'
	.db #0x17	; 23
	.db #0x0b	; 11
	.db #0x1f	; 31
	.db #0x1c	; 28
	.db #0x3f	; 63
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x11	; 17
	.db #0x78	; 120	'x'
	.db #0x20	; 32
	.db #0x38	; 56	'8'
	.db #0x5e	; 94
	.db #0x01	; 1
	.db #0x0b	; 11
	.db #0x07	; 7
	.db #0x17	; 23
	.db #0x02	; 2
	.db #0x78	; 120	'x'
	.db #0x10	; 16
	.db #0x3c	; 60
	.db #0x1d	; 29
	.db #0x1e	; 30
	.db #0x0f	; 15
	.db #0x0f	; 15
	.db #0x07	; 7
	.db #0x03	; 3
	.db #0x01	; 1
	.db #0x78	; 120	'x'
	.db #0x71	; 113	'q'
	.db #0x33	; 51	'3'
	.db #0x07	; 7
	.db #0x57	; 87	'W'
	.db #0x55	; 85	'U'
	.db #0x4a	; 74	'J'
	.db #0x51	; 81	'Q'
	.db #0x3f	; 63
	.db #0x0f	; 15
	.db #0x7f	; 127
	.db #0x0c	; 12
	.db #0x60	; 96
	.db #0x3f	; 63
	.db #0x7f	; 127
	.db #0x00	; 0
	.db #0xc0	; 192
	.db #0x00	; 0
	.db #0x00	; 0
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
_filter:
	.ascii "00000000021102000000000000000000"
	.db 0x00
	.ascii "00110000021112200000000000011100"
	.db 0x00
	.ascii "00111011122112211100022200011100"
	.db 0x00
	.ascii "01111011122212211110022210011110"
	.db 0x00
	.ascii "10111001111111111001222210011122"
	.db 0x00
	.ascii "22111001111111111002222210022222"
	.db 0x00
	.ascii "22222001111111111000212210022210"
	.db 0x00
	.ascii "01222001111111111000112210012100"
	.db 0x00
	.ascii "01122100111111111000022200001100"
	.db 0x00
	.ascii "01111101111111111000011100001100"
	.db 0x00
;chuckrock.c:163: void unpackchar(int vdpOff) {
;	---------------------------------
; Function unpackchar
; ---------------------------------
_unpackchar::
	call	___sdcc_enter_ix
;chuckrock.c:164: VDP_SET_ADDRESS_WRITE(gPattern+vdpOff);
	ld	c, 4 (ix)
	ld	b, 5 (ix)
	ld	hl, (_gPattern)
	add	hl, bc
	ex	de,hl
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, e
	out	(_VDPWA), a
	ld	a, d
	or	a, #0x40
	out	(_VDPWA), a
;chuckrock.c:165: writeCompressedByte8(&strDat[0]);
	push	bc
	ld	hl, #_strDat
	push	hl
	call	_writeCompressedByte8
	pop	af
	pop	bc
;chuckrock.c:167: VDP_SET_ADDRESS_WRITE(gColor+vdpOff);
	ld	hl, (_gColor)
	add	hl, bc
	ld	c, l
	ld	b, h
;d:/work/coleco/libti99coleco/vdp.h:64: inline void VDP_SET_ADDRESS_WRITE(unsigned int x)					{	VDPWA=((x)&0xff); VDPWA=(((x)>>8)|0x40); }
	ld	a, c
	out	(_VDPWA), a
	ld	a, b
	or	a, #0x40
	out	(_VDPWA), a
;chuckrock.c:168: writeCompressedByte8(&strDat[1]);
	ld	hl, #(_strDat + 0x0008)
	push	hl
	call	_writeCompressedByte8
	pop	af
;chuckrock.c:169: }
	pop	ix
	ret
;chuckrock.c:171: void chuckinit() {
;	---------------------------------
; Function chuckinit
; ---------------------------------
_chuckinit::
	call	___sdcc_enter_ix
	ld	hl, #-9
	add	hl, sp
	ld	sp, hl
;chuckrock.c:172: unsigned char x = set_bitmap_raw(VDP_SPR_8x8);		// set graphics mode
	xor	a, a
	push	af
	inc	sp
	call	_set_bitmap
	ld	-9 (ix), l
	inc	sp
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, #0x01
	out	(_VDPWA), a
	ld	a, #0x87
	out	(_VDPWA), a
;chuckrock.c:177: vdpmemset(0, 0, 512);
	ld	hl, #0x0200
	push	hl
	xor	a, a
	push	af
	inc	sp
	ld	h, l
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;chuckrock.c:180: vdpmemset(0+512, 32, 256);
	ld	hl, #0x0100
	push	hl
	ld	a, #0x20
	push	af
	inc	sp
	ld	h, #0x02
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;chuckrock.c:183: vdpmemcpy(gPattern+0x1300, TrueLowerCase, 216);
	ld	bc, #_TrueLowerCase+0
	ld	hl, (_gPattern)
	ld	de, #0x1300
	add	hl, de
	ld	de, #0x00d8
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;chuckrock.c:189: vdpmemcpy(gPattern+0x1100, COLECO_FONT, 64*8);
	ld	hl, (_gPattern)
	ld	de, #0x1100
	add	hl, de
	ld	de, #0x0200
	push	de
	ld	de, #0x15a3
	push	de
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;chuckrock.c:192: vdpmemset(gColor+0x1000, 0xf0, 0x800);
	ld	hl, (_gColor)
	ld	de, #0x1000
	add	hl, de
	ld	de, #0x0800
	push	de
	ld	a, #0xf0
	push	af
	inc	sp
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
;chuckrock.c:197: unsigned char nChar=0;		// displayed char (will pre-increment)
	ld	c, #0x00
;chuckrock.c:198: int nVRAMOff=8;		// VRAM offset
	ld	de, #0x0008
;chuckrock.c:201: memset(strDat, 0, sizeof(strDat));
	push	bc
	push	de
	ld	hl, #0x0010
	push	hl
	ld	l, h
	push	hl
	ld	hl, #_strDat
	push	hl
	call	_memset
	pop	af
	pop	af
	pop	af
	pop	de
	pop	bc
;chuckrock.c:202: strDat[0].mainPtr = (unsigned char*)chuckpat;
	ld	hl, #_chuckpat
	ld	((_strDat + 0x0002)), hl
;chuckrock.c:203: strDat[1].mainPtr = (unsigned char*)chuckcol;
	ld	hl, #_chuckcol
	ld	((_strDat + 0x000a)), hl
;chuckrock.c:206: unpackchar(0);
	push	bc
	push	de
	ld	hl, #0x0000
	push	hl
	call	_unpackchar
	pop	af
	pop	de
	pop	bc
;chuckrock.c:208: for (idx_t nFrame=0; nFrame<2; nFrame++) {
	ld	-3 (ix), #0
00123$:
	ld	a, -3 (ix)
	sub	a, #0x02
	jp	NC, 00113$
;chuckrock.c:209: for (idx_t idx=0; idx<10; idx++) {
	ld	a, -3 (ix)
	dec	a
	ld	a, #0x01
	jr	Z, 00181$
	xor	a, a
00181$:
	ld	-8 (ix), a
	ld	-2 (ix), #0
00120$:
	ld	a, -2 (ix)
	sub	a, #0x0a
	jp	NC, 00112$
;chuckrock.c:210: for (idx_t c=0; c<32; c++) {
	push	de
	ld	e, -2 (ix)
	ld	d, #0x00
	ld	l, e
	ld	h, d
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, de
	pop	de
	ld	a, #<(_filter)
	add	a, l
	ld	-7 (ix), a
	ld	a, #>(_filter)
	adc	a, h
	ld	-6 (ix), a
	ld	a, #<(_filter)
	add	a, l
	ld	-5 (ix), a
	ld	a, #>(_filter)
	adc	a, h
	ld	-4 (ix), a
	ld	-1 (ix), c
	ld	c, #0x00
00117$:
	ld	a, c
	sub	a, #0x20
	jr	NC, 00137$
;chuckrock.c:211: if (((nFrame==0)&&(filter[idx][c] > '0')) || ((nFrame==1)&&(filter[idx][c]=='2'))) {
	ld	a, -3 (ix)
	or	a, a
	jr	NZ, 00105$
	ld	l, -7 (ix)
	ld	h, -6 (ix)
	ld	b, #0x00
	add	hl, bc
	ld	b, (hl)
	ld	a, #0x30
	sub	a, b
	jp	PO, 00182$
	xor	a, #0x80
00182$:
	jp	M, 00101$
00105$:
	ld	a, -8 (ix)
	or	a, a
	jr	Z, 00118$
	ld	l, -5 (ix)
	ld	h, -4 (ix)
	ld	b, #0x00
	add	hl, bc
	ld	a, (hl)
	sub	a, #0x32
	jr	NZ, 00118$
00101$:
;chuckrock.c:213: ++nChar;			// increment the character index
	inc	-1 (ix)
;chuckrock.c:214: unpackchar(nVRAMOff);		// extract the data
	push	bc
	push	de
	push	de
	call	_unpackchar
	pop	af
	pop	de
	pop	bc
;chuckrock.c:215: vdpscreenchar(((idx+3)<<5)+c,nChar);	// put the char on screen
	ld	l, -2 (ix)
	ld	h, #0x00
	inc	hl
	inc	hl
	inc	hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	add	hl, hl
	ld	a, c
	ld	b, #0x00
	add	a, l
	ld	l, a
	ld	a, b
	adc	a, h
	ld	h, a
	push	bc
	push	de
	ld	a, -1 (ix)
	push	af
	inc	sp
	push	hl
	call	_vdpscreenchar
	pop	af
	inc	sp
	pop	de
	pop	bc
;chuckrock.c:216: nVRAMOff+=8;
	ld	hl, #0x0008
	add	hl, de
	ex	de, hl
00118$:
;chuckrock.c:210: for (idx_t c=0; c<32; c++) {
	inc	c
	jr	00117$
00137$:
	ld	c, -1 (ix)
;chuckrock.c:219: if (idx==4) {
	ld	a, -2 (ix)
	sub	a, #0x04
	jr	NZ, 00121$
;chuckrock.c:220: if (nFrame) {
	ld	a, -3 (ix)
	or	a, a
	jr	Z, 00108$
;chuckrock.c:221: nVRAMOff=0xB80;		// restart on the next character set
	ld	de, #0x0b80
;chuckrock.c:222: nChar=111;
	ld	c, #0x6f
	jr	00121$
00108$:
;chuckrock.c:224: nVRAMOff=0x808;		// restart on the next character set
	ld	de, #0x0808
;chuckrock.c:225: vdpmemset(gPattern+0x800, 0, 8);
	ld	hl, (_gPattern)
	ld	bc, #0x0800
	add	hl, bc
	push	de
	ld	bc, #0x0008
	push	bc
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
	pop	de
;chuckrock.c:226: vdpmemset(gColor+0x800, 0, 8);
	ld	hl, (_gColor)
	ld	bc, #0x0800
	add	hl, bc
	push	de
	ld	bc, #0x0008
	push	bc
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_vdpmemset
	pop	af
	pop	af
	inc	sp
	pop	de
;chuckrock.c:227: nChar=0;
	ld	c, #0x00
00121$:
;chuckrock.c:209: for (idx_t idx=0; idx<10; idx++) {
	inc	-2 (ix)
	jp	00120$
00112$:
;chuckrock.c:233: nChar=86;			// displayed char (will pre-increment)
	ld	c, #0x56
;chuckrock.c:234: nVRAMOff=0x2B8;		// VRAM offset
	ld	de, #0x02b8
;chuckrock.c:208: for (idx_t nFrame=0; nFrame<2; nFrame++) {
	inc	-3 (ix)
	jp	00123$
00113$:
;d:/work/coleco/libti99coleco/vdp.h:71: inline void VDP_SET_REGISTER(unsigned char r, unsigned char v)		{	VDPWA=(v); VDPWA=(0x80|(r)); }
	ld	a, -9 (ix)
	out	(_VDPWA), a
	ld	a, #0x81
	out	(_VDPWA), a
;chuckrock.c:237: VDP_SET_REGISTER(VDP_REG_MODE1,x);		// enable the display
;chuckrock.c:238: }
	ld	sp, ix
	pop	ix
	ret
;chuckrock.c:243: void processList(const idx_t *pList) {
;	---------------------------------
; Function processList
; ---------------------------------
_processList::
	call	___sdcc_enter_ix
	push	af
;chuckrock.c:250: while (*((const unsigned int*)pList)) {
	ld	c, 4 (ix)
	ld	b, 5 (ix)
00101$:
	ld	l, c
	ld	h, b
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	a, d
	or	a, e
	jr	Z, 00104$
;chuckrock.c:253: vdpwritescreeninc(*((const unsigned int*)pList), *(pList+2), *(pList+3));
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	inc	hl
	ld	a, (hl)
	ld	-2 (ix), a
	ld	-1 (ix), #0
	ld	l, c
	ld	h, b
	inc	hl
	inc	hl
	ld	a, (hl)
	push	bc
	ld	l, -2 (ix)
	ld	h, -1 (ix)
	push	hl
	push	af
	inc	sp
	push	de
	call	_vdpwritescreeninc
	pop	af
	pop	af
	inc	sp
	pop	bc
;chuckrock.c:254: pList+=4;	// 4 bytes
	inc	bc
	inc	bc
	inc	bc
	inc	bc
	jr	00101$
00104$:
;chuckrock.c:257: }
	pop	af
	pop	ix
	ret
;chuckrock.c:381: int main() {
;	---------------------------------
; Function main
; ---------------------------------
_main::
	call	___sdcc_enter_ix
	dec	sp
;chuckrock.c:383: chuckinit();
	call	_chuckinit
;chuckrock.c:385: for (idx_t idx=0; idx<4; idx++) {
	ld	c, #0x00
00147$:
	ld	a, c
	sub	a, #0x04
	jr	NC, 00101$
;chuckrock.c:386: nOldVol[idx]=0xff;
	ld	hl, #_nOldVol
	ld	b, #0x00
	add	hl, bc
	ld	(hl), #0xff
;chuckrock.c:387: nOldVoice[idx]=0;
	ld	a, c
	ld	h, #0x00
	ld	l, a
	add	hl, hl
	ld	de, #_nOldVoice
	add	hl, de
	xor	a, a
	ld	(hl), a
	inc	hl
	ld	(hl), a
;chuckrock.c:388: nState[idx]=0;
	ld	hl, #_nState
	ld	b, #0x00
	add	hl, bc
	ld	(hl), #0x00
;chuckrock.c:385: for (idx_t idx=0; idx<4; idx++) {
	inc	c
	jr	00147$
00101$:
;chuckrock.c:392: vdpmemcpy(gImage+(17*32+0), textout, 32*4);
	ld	bc, #_textout+0
	ld	hl, (_gImage)
	ld	de, #0x0220
	add	hl, de
	ld	de, #0x0080
	push	de
	push	bc
	push	hl
	call	_vdpmemcpy
	pop	af
	pop	af
	pop	af
;chuckrock.c:395: chucktune = *((unsigned int*)&flags[6]);
	ld	hl, #(_flags + 0x0006)
	ld	a, (hl)
	ld	(_chucktune+0), a
	inc	hl
	ld	a, (hl)
	ld	(_chucktune+1), a
;chuckrock.c:399: volatile unsigned char *pLoop = (volatile unsigned char *)&flags[13];
;chuckrock.c:401: do {
00135$:
;chuckrock.c:405: if (0 != chucktune) {
	ld	iy, #_chucktune
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00103$
;chuckrock.c:406: StartSong((unsigned char*)chucktune, 0);
	ld	hl, (_chucktune)
	xor	a, a
	push	af
	inc	sp
	push	hl
	call	_StartSong
	pop	af
	inc	sp
00103$:
;chuckrock.c:410: done = 0;
	ld	-1 (ix), #0
;chuckrock.c:411: while (!done) {
00128$:
	ld	a, -1 (ix)
	or	a, a
	jp	NZ, 00130$
;chuckrock.c:412: done = 1;
	ld	-1 (ix), #0x01
;chuckrock.c:413: vdpwaitvint();
	call	_vdpwaitvint
;chuckrock.c:414: if (0 != chucktune) {
	ld	iy, #_chucktune
	ld	a, 1 (iy)
	or	a, 0 (iy)
	jr	Z, 00107$
;chuckrock.c:415: if (isSNPlaying) {
	ld	hl, (#(_songNote + 0x0006) + 0)
	bit	0, l
	jr	Z, 00107$
;chuckrock.c:416: CALL_PLAYER_SN;
	call	_SongLoop
;chuckrock.c:417: done = 0;
	xor	a, a
	ld	-1 (ix), a
00107$:
;chuckrock.c:421: if (songNote[0] != nOldVoice[0]) {
	ld	bc, (#_songNote + 0)
	ld	hl, (#_nOldVoice + 0)
	cp	a, a
	sbc	hl, bc
	jr	Z, 00112$
;chuckrock.c:423: nOldVol[0]=songVol[0];
	ld	a, (#_songVol + 0)
	ld	hl, #_nOldVol
	ld	(hl), a
;chuckrock.c:424: nOldVoice[0]=songNote[0];
	ld	bc, (#_songNote + 0)
	ld	(_nOldVoice), bc
;chuckrock.c:425: nState[0]=!nState[0];
	ld	a, (#_nState + 0)
	sub	a,#0x01
	ld	a, #0x00
	rla
	ld	(#_nState),a
;chuckrock.c:426: if (nState[0]) {
	or	a, a
	jr	Z, 00109$
;chuckrock.c:374: processList(chuckf2dat);
	ld	hl, #_chuckf2dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:427: chuckf2();
	jr	00112$
00109$:
;chuckrock.c:358: processList(chuckf1dat);
	ld	hl, #_chuckf1dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:429: chuckf1();
00112$:
;chuckrock.c:432: if (songNote[1] != nOldVoice[1]) {
	ld	bc, (#(_songNote + 0x0002) + 0)
	ld	hl, (#(_nOldVoice + 0x0002) + 0)
	cp	a, a
	sbc	hl, bc
	jr	Z, 00117$
;chuckrock.c:434: nOldVol[1]=songVol[1];
	ld	bc, #_nOldVol + 1
	ld	a, (#(_songVol + 0x0001) + 0)
	ld	(bc), a
;chuckrock.c:435: nOldVoice[1]=songNote[1];
	ld	bc, (#(_songNote + 0x0002) + 0)
	ld	((_nOldVoice + 0x0002)), bc
;chuckrock.c:436: nState[1]=!nState[1];
	ld	bc, #_nState + 1
	ld	a, (bc)
	sub	a,#0x01
	ld	a, #0x00
	rla
	ld	(bc), a
;chuckrock.c:437: if (nState[1]) {
	or	a, a
	jr	Z, 00114$
;chuckrock.c:281: processList(dinof2dat);
	ld	hl, #_dinof2dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:438: dinof2();
	jr	00117$
00114$:
;chuckrock.c:269: processList(dinof1dat);
	ld	hl, #_dinof1dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:440: dinof1();
00117$:
;chuckrock.c:443: if (songNote[2] != nOldVoice[2]) {
	ld	bc, (#(_songNote + 0x0004) + 0)
	ld	hl, (#(_nOldVoice + 0x0004) + 0)
	cp	a, a
	sbc	hl, bc
	jr	Z, 00122$
;chuckrock.c:445: nOldVol[2]=songVol[2];
	ld	a, (#(_songVol + 0x0002) + 0)
	ld	hl, #(_nOldVol + 0x0002)
	ld	(hl), a
;chuckrock.c:446: nOldVoice[2]=songNote[2];
	ld	bc, (#(_songNote + 0x0004) + 0)
	ld	((_nOldVoice + 0x0004)), bc
;chuckrock.c:447: nState[2]=!nState[2];
	ld	bc, #_nState + 2
	ld	a, (bc)
	sub	a,#0x01
	ld	a, #0x00
	rla
	ld	(bc), a
;chuckrock.c:448: if (nState[2]) {
	or	a, a
	jr	Z, 00119$
;chuckrock.c:341: processList(opheliaf2dat);
	ld	hl, #_opheliaf2dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:449: opheliaf2();
	jr	00122$
00119$:
;chuckrock.c:328: processList(opheliaf1dat);
	ld	hl, #_opheliaf1dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:451: opheliaf1();
00122$:
;chuckrock.c:454: if (songVol[3]+4 < nOldVol[3]) {
	ld	a, (#(_songVol + 0x0003) + 0)
	ld	c, a
	ld	b, #0x00
	inc	bc
	inc	bc
	inc	bc
	inc	bc
	ld	a, (#(_nOldVol + 0x0003) + 0)
	ld	e, a
	ld	d, #0x00
	ld	a, c
	sub	a, e
	ld	a, b
	sbc	a, d
	jp	PO, 00251$
	xor	a, #0x80
00251$:
	jp	P, 00127$
;chuckrock.c:456: nOldVoice[3]=songNote[3];
	ld	bc, (#(_songNote + 0x0006) + 0)
	ld	((_nOldVoice + 0x0006)), bc
;chuckrock.c:457: nState[3]=!nState[3];
	ld	bc, #_nState + 3
	ld	a, (bc)
	sub	a,#0x01
	ld	a, #0x00
	rla
	ld	(bc), a
;chuckrock.c:458: if (nState[3]) {
	or	a, a
	jr	Z, 00124$
;chuckrock.c:315: processList(garyf2dat);
	ld	hl, #_garyf2dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:459: garyf2();
	jr	00127$
00124$:
;chuckrock.c:299: processList(garyf1dat);
	ld	hl, #_garyf1dat
	push	hl
	call	_processList
	pop	af
;chuckrock.c:461: garyf1();
00127$:
;chuckrock.c:464: nOldVol[3]=songVol[3];
	ld	a, (#(_songVol + 0x0003) + 0)
	ld	(#(_nOldVol + 0x0003)),a
	jp	00128$
00130$:
;chuckrock.c:468: SOUND=0x9F;
	ld	a, #0x9f
	out	(_SOUND), a
;chuckrock.c:469: SOUND=0xBF;
	ld	a, #0xbf
	out	(_SOUND), a
;chuckrock.c:470: SOUND=0xDF;
	ld	a, #0xdf
	out	(_SOUND), a
;chuckrock.c:471: SOUND=0xFF;
	ld	a, #0xff
	out	(_SOUND), a
;chuckrock.c:475: chain = (unsigned int *)(*((volatile unsigned int*)(&flags[14])));
	ld	bc, (#(_flags + 0x000e) + 0)
	ld	l, c
;chuckrock.c:476: if (chain) {
	ld	a,b
	ld	h,a
	or	a, c
	jr	Z, 00136$
;chuckrock.c:478: unsigned int chained = *chain;
	ld	e, (hl)
	inc	hl
	ld	d, (hl)
	ld	c, e
;chuckrock.c:479: if (chained) {
	ld	a,d
	ld	b,a
	or	a, e
	jr	Z, 00136$
;chuckrock.c:488: memcpy((void*)0x7000, tramp, sizeof(tramp));   // this will trounce variables but we don't need them anymore
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
;chuckrock.c:489: *((unsigned int*)0x7001) = chained;     // patch the pointer, chained should be on the stack
	ld	(0x7001), bc
;chuckrock.c:490: ((void(*)())0x7000)();                  // call the function, never return
	call	0x7000
00136$:
;chuckrock.c:494: } while (*pLoop);
	ld	a, (#(_flags + 0x000d) + 0)
	or	a, a
	jp	NZ, 00135$
;chuckrock.c:505: __endasm;
	rst	0x00
;chuckrock.c:508: return 2;
	ld	hl, #0x0002
;chuckrock.c:509: }
	inc	sp
	pop	ix
	ret
_dinof1dat:
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x02	; 2
	.db #0x20	; 32
	.db #0x01	; 1
	.db #0x1b	; 27
	.db #0x05	; 5
	.db #0x42	; 66	'B'
	.db #0x01	; 1
	.db #0x34	; 52	'4'
	.db #0x03	; 3
	.db #0x63	; 99	'c'
	.db #0x01	; 1
	.db #0x4b	; 75	'K'
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
_dinof2dat:
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x70	; 112	'p'
	.db #0x02	; 2
	.db #0x20	; 32
	.db #0x01	; 1
	.db #0x7c	; 124
	.db #0x05	; 5
	.db #0x42	; 66	'B'
	.db #0x01	; 1
	.db #0x87	; 135
	.db #0x03	; 3
	.db #0x63	; 99	'c'
	.db #0x01	; 1
	.db #0x8d	; 141
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
_garyf1dat:
	.db #0x69	; 105	'i'
	.db #0x00	; 0
	.db #0x01	; 1
	.db #0x01	; 1
	.db #0x6d	; 109	'm'
	.db #0x00	; 0
	.db #0x04	; 4
	.db #0x01	; 1
	.db #0x89	; 137
	.db #0x00	; 0
	.db #0x07	; 7
	.db #0x01	; 1
	.db #0x8d	; 141
	.db #0x00	; 0
	.db #0x0b	; 11
	.db #0x02	; 2
	.db #0xa9	; 169
	.db #0x00	; 0
	.db #0x16	; 22
	.db #0x02	; 2
	.db #0xad	; 173
	.db #0x00	; 0
	.db #0x1a	; 26
	.db #0x02	; 2
	.db #0xc9	; 201
	.db #0x00	; 0
	.db #0x2c	; 44
	.db #0x03	; 3
	.db #0xcd	; 205
	.db #0x00	; 0
	.db #0x30	; 48	'0'
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
_garyf2dat:
	.db #0x69	; 105	'i'
	.db #0x00	; 0
	.db #0x57	; 87	'W'
	.db #0x01	; 1
	.db #0x6d	; 109	'm'
	.db #0x00	; 0
	.db #0x58	; 88	'X'
	.db #0x01	; 1
	.db #0x89	; 137
	.db #0x00	; 0
	.db #0x59	; 89	'Y'
	.db #0x01	; 1
	.db #0x8d	; 141
	.db #0x00	; 0
	.db #0x5a	; 90	'Z'
	.db #0x02	; 2
	.db #0xa9	; 169
	.db #0x00	; 0
	.db #0x5c	; 92
	.db #0x02	; 2
	.db #0xad	; 173
	.db #0x00	; 0
	.db #0x5e	; 94
	.db #0x02	; 2
	.db #0xc9	; 201
	.db #0x00	; 0
	.db #0x63	; 99	'c'
	.db #0x03	; 3
	.db #0xcd	; 205
	.db #0x00	; 0
	.db #0x66	; 102	'f'
	.db #0x02	; 2
	.db #0x00	; 0
	.db #0x00	; 0
_opheliaf1dat:
	.db #0xfe	; 254
	.db #0x00	; 0
	.db #0x55	; 85	'U'
	.db #0x02	; 2
	.db #0x1b	; 27
	.db #0x01	; 1
	.db #0x16	; 22
	.db #0x05	; 5
	.db #0x3b	; 59
	.db #0x01	; 1
	.db #0x2f	; 47
	.db #0x03	; 3
	.db #0x5c	; 92
	.db #0x01	; 1
	.db #0x47	; 71	'G'
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
_opheliaf2dat:
	.db #0xfe	; 254
	.db #0x00	; 0
	.db #0x6f	; 111	'o'
	.db #0x02	; 2
	.db #0x1b	; 27
	.db #0x01	; 1
	.db #0x77	; 119	'w'
	.db #0x05	; 5
	.db #0x3b	; 59
	.db #0x01	; 1
	.db #0x84	; 132
	.db #0x03	; 3
	.db #0x5c	; 92
	.db #0x01	; 1
	.db #0x8c	; 140
	.db #0x01	; 1
	.db #0x00	; 0
	.db #0x00	; 0
_chuckf1dat:
	.db #0xb5	; 181
	.db #0x00	; 0
	.db #0x1f	; 31
	.db #0x03	; 3
	.db #0xd5	; 213
	.db #0x00	; 0
	.db #0x36	; 54	'6'
	.db #0x03	; 3
	.db #0xf4	; 244
	.db #0x00	; 0
	.db #0x4d	; 77	'M'
	.db #0x04	; 4
	.db #0x13	; 19
	.db #0x01	; 1
	.db #0x10	; 16
	.db #0x05	; 5
	.db #0x34	; 52	'4'
	.db #0x01	; 1
	.db #0x2a	; 42
	.db #0x01	; 1
	.db #0x36	; 54	'6'
	.db #0x01	; 1
	.db #0x2c	; 44
	.db #0x02	; 2
	.db #0x56	; 86	'V'
	.db #0x01	; 1
	.db #0x43	; 67	'C'
	.db #0x02	; 2
	.db #0x75	; 117	'u'
	.db #0x01	; 1
	.db #0x57	; 87	'W'
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x00	; 0
_chuckf2dat:
	.db #0xb5	; 181
	.db #0x00	; 0
	.db #0x60	; 96
	.db #0x03	; 3
	.db #0xd5	; 213
	.db #0x00	; 0
	.db #0x68	; 104	'h'
	.db #0x03	; 3
	.db #0xf4	; 244
	.db #0x00	; 0
	.db #0x6b	; 107	'k'
	.db #0x04	; 4
	.db #0x13	; 19
	.db #0x01	; 1
	.db #0x72	; 114	'r'
	.db #0x05	; 5
	.db #0x34	; 52	'4'
	.db #0x01	; 1
	.db #0x81	; 129
	.db #0x01	; 1
	.db #0x36	; 54	'6'
	.db #0x01	; 1
	.db #0x82	; 130
	.db #0x02	; 2
	.db #0x56	; 86	'V'
	.db #0x01	; 1
	.db #0x8a	; 138
	.db #0x02	; 2
	.db #0x75	; 117	'u'
	.db #0x01	; 1
	.db #0x8f	; 143
	.db #0x03	; 3
	.db #0x00	; 0
	.db #0x00	; 0
	.area _CODE
	.area _INITIALIZER
	.area _CABS (ABS)

	pseg
	even
getDatZero
	clr  r1
	b    *r11
	.size	getDatZero, .-getDatZero
	even

	def	StartSong
StartSong
	movb @>1(r1), r3
	srl  r3, 8
	movb *r1, r4
	andi r4, >FF00
	a    r4, r3
	srl  r2, 8
	mov  r2, r4
	a    r2, r4
	sla  r2, >4
	a    r2, r4
	a    r4, r3
	a    r1, r3
	li   r2, strDat+6
L4
	movb *r3, r4
	andi r4, >FF00
	movb @>1(r3), r5
	srl  r5, 8
	a    r5, r4
	a    r1, r4
	mov  r4, @>FFFC(r2)
	clr  @>FFFA(r2)
	clr  r4
	movb r4, *r2
	li   r4, getDatZero
	mov  r4, @>FFFE(r2)
	clr  r4
	movb r4, @>1(r2)
	inct r3
	ai   r2, >8
	ci   r2, strDat+78
	jne  L4
	li   r2, >1
	mov  r2, @songNote
	movb r4, @songVol
	mov  r2, @songNote+2
	movb r4, @songVol+1
	mov  r2, @songNote+4
	movb r4, @songVol+2
	mov  r2, @songNote+6
	movb r4, @songVol+3
	mov  r1, @workBuf
	li   r4, >100
	movb r4, @songActive
	b    *r11
	.size	StartSong, .-StartSong
	even

	def	StopSong
StopSong
	clr  r1
	movb r1, @songActive
	b    *r11
	.size	StopSong, .-StopSong
	even

	def	SongLoop
SongLoop
	ai   r10, >FFEE
	mov  r10, r0
	mov  r11, *r0+
	mov  r9, *r0+
	mov  r13, *r0+
	mov  r14, *r0+
	mov  r15, *r0
	movb @songActive, r1
	srl  r1, 8
	andi r1, >1
	ci   r1, 0
	jne  JMP_0
	b    @L33
JMP_0
	mov  @strDat+66, r1
	jne  JMP_1
	b    @L12
JMP_1
	movb @strDat+71, r1
	jne  JMP_2
	b    @L13
JMP_2
	ai   r1, >FF00
	movb r1, @strDat+71
	li   r5, >100
L14
	li   r9, strDat+34
	mov  r9, r13
	ai   r13, >5
	li   r4, songVol
	li   r3, muteMap+1
	mov  r9, r15
	dect r15
	li   r14, >9000
	li   r6, >F000
	jmp  L32
L34
	ai   r1, >FF00
	movb r1, *r13
	li   r5, >100
L26
	ai   r9, >8
	ai   r13, >8
	inc  r4
	inc  r3
	ai   r15, >8
	cb   r14, r6
	jeq  L31
L36
	ai   r14, >2000
L32
	mov  *r9, r1
	jeq  L26
	movb *r13, r1
	jne  L34
	mov  r15, r1
	mov  @workBuf, r2
	mov  r3, @>A(r10)
	mov  r4, @>C(r10)
	mov  r5, @>E(r10)
	mov  r6, @>10(r10)
	li   r7, getCompressedByte
	bl   *r7
	mov  *r9, r2
	mov  @>A(r10), r3
	mov  @>C(r10), r4
	mov  @>E(r10), r5
	mov  @>10(r10), r6
	ci   r2, 0
	jeq  L35
	movb r1, r2
	andi r2, >F00
	movb r2, *r13
	srl  r1, >4
	li   r5, >100
L29
	socb r14, r1
	movb @songActive, r2
	movb *r3, r7
	inv  r7
	szcb r7, r2
	jne  L30
	movb r1, @>8400
L30
	movb r1, *r4
	ai   r9, >8
	ai   r13, >8
	inc  r4
	inc  r3
	ai   r15, >8
	cb   r14, r6
	jne  L36
L31
	jeq  0
	cb  r5, @$-1
	jne  L33
	movb r5, @songActive
L33
	mov  *r10+, r11
	mov  *r10+, r9
	mov  *r10+, r13
	mov  *r10+, r14
	mov  *r10, r15
	ai   r10, >A
	b    *r11
L35
	li   r1, >F00
	jmp  L29
L13
	li   r13, getCompressedByte
	li   r1, strDat+64
	mov  @workBuf, r2
	bl   *r13
	movb r1, r9
	mov  @strDat+66, r1
	jne  JMP_3
	b    @L12
JMP_3
	movb r9, r3
	andi r3, >F00
	movb r3, @strDat+71
	jeq  0
	cb  r9, @$-1
	jgt  JMP_4
	jeq  JMP_4
	b    @L37
JMP_4
L15
	srl  r9, 8
	mov  r9, r1
	andi r1, >40
	jeq  L18
	mov  @strDat+10, r1
	jeq  L18
	li   r1, strDat+8
	mov  @workBuf, r2
	bl   *r13
	mov  @strDat+10, r2
	jne  JMP_5
	b    @L19
JMP_5
	mov  @workBuf, r3
	srl  r1, 8
	a    r1, r1
	movb @>2(r3), r2
	andi r2, >FF00
	movb @>3(r3), r4
	srl  r4, 8
	a    r4, r2
	a    r2, r1
	a    r3, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >A000
L20
	mov  r2, @songNote+2
	movb @songActive, r1
	andi r1, >4000
	jne  L18
	mov  r2, r1
	movb r1, @>8400
	swpb r2
	movb r2, @>8400
L18
	mov  r9, r1
	andi r1, >20
	jeq  L21
	mov  @strDat+18, r1
	jeq  L21
	li   r1, strDat+16
	mov  @workBuf, r2
	bl   *r13
	mov  @strDat+18, r2
	jne  JMP_6
	b    @L22
JMP_6
	mov  @workBuf, r3
	srl  r1, 8
	a    r1, r1
	movb @>2(r3), r2
	andi r2, >FF00
	movb @>3(r3), r4
	srl  r4, 8
	a    r4, r2
	a    r2, r1
	a    r3, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >C000
L23
	mov  r2, @songNote+4
	movb @songActive, r1
	andi r1, >2000
	jne  L21
	mov  r2, r1
	movb r1, @>8400
	swpb r2
	movb r2, @>8400
L21
	andi r9, >10
	ci   r9, 0
	jeq  L24
	mov  @strDat+26, r1
	jeq  L24
	li   r1, strDat+24
	mov  @workBuf, r2
	bl   *r13
	mov  @strDat+26, r2
	jeq  L24
	ori  r1, >E000
	movb @songActive, r2
	andi r2, >1000
	jne  L25
	movb r1, @>8400
L25
	srl  r1, 8
	mov  r1, @songNote+6
	li   r5, >100
	b    @L14
L24
	li   r5, >100
	b    @L14
L12
	clr  r5
	b    @L14
L37
	mov  @strDat+2, r1
	jne  JMP_7
	b    @L15
JMP_7
	li   r1, strDat
	mov  @workBuf, r2
	bl   *r13
	mov  @strDat+2, r2
	jeq  L16
	mov  @workBuf, r3
	srl  r1, 8
	a    r1, r1
	movb @>2(r3), r2
	andi r2, >FF00
	movb @>3(r3), r4
	srl  r4, 8
	a    r4, r2
	a    r2, r1
	a    r3, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >8000
L17
	mov  r2, @songNote
	movb @songActive, r1
	andi r1, >8000
	jeq  JMP_8
	b    @L15
JMP_8
	mov  r2, r1
	movb r1, @>8400
	swpb r2
	movb r2, @>8400
	b    @L15
L19
	li   r2, >A100
	b    @L20
L22
	li   r2, >C100
	b    @L23
L16
	li   r2, >8100
	jmp  L17
	.size	SongLoop, .-SongLoop

	def	muteMap
	.type	muteMap, @object
	.size	muteMap, 5
muteMap
	byte	0
	byte	-128
	byte	64
	byte	32
	byte	16
	cseg

	even
strDat
	bss 72

	even
	def songVol
songVol
	bss 4

	even
	def songNote
songNote
	bss 8

	even
	def songActive
songActive
	bss 1

	even
	def workBuf
workBuf
	bss 2

	ref	getCompressedByte

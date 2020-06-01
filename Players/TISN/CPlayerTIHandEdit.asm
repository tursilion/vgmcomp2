	pseg
	even
getDatZero
	clr  r1
	b    *r11
	.size	getDatZero, .-getDatZero
	even

	def	StartSong
StartSong
	movb @>1(r1), r4
	srl  r4, 8
	movb *r1, r3
	andi r3, >FF00
	a    r3, r4
	mov  r4, @streamoffset
	movb @>2(r1), r3
	andi r3, >FF00
	movb @>3(r1), r5
	srl  r5, 8
	a    r5, r3
	mov  r3, @toneoffset
	mov  r2, r3
	a    r2, r3
	sla  r2, >4
	a    r2, r3
	a    r4, r3
	mov  r3, @streamoffset
	a    r1, r3
	li   r2, strDat+2
L4
	movb *r3, r4
	andi r4, >FF00
	movb @>1(r3), r5
	srl  r5, 8
	a    r5, r4
	a    r1, r4
	mov  r4, *r2
	clr  @>FFFE(r2)
	clr  @>4(r2)
	li   r4, getDatZero
	mov  r4, @>2(r2)
	clr  @>6(r2)
	inct r3
	ai   r2, >A
	ci   r2, strDat+92
	jne  L4
	li   r2, >1
	mov  r2, @songNote
	clr  r4
	movb r4, @songVol
	mov  r2, @songNote+2
	movb r4, @songVol+1
	mov  r2, @songNote+4
	movb r4, @songVol+2
	mov  r2, @songNote+6
	movb r4, @songVol+3
	mov  r1, @workBuf
	mov  r2, @songActive
	b    *r11
	.size	StartSong, .-StartSong
	even

	def	StopSong
StopSong
	clr  @songActive
	b    *r11
	.size	StopSong, .-StopSong
	even

	def	SongLoop
SongLoop
	ai   r10, >FFF0
	mov  r10, r0
	mov  r11, *r0+
	mov  r9, *r0+
	mov  r13, *r0+
	mov  r14, *r0+
	mov  r15, *r0
	mov  @songActive, r1
	jne  JMP_0
	b    @L28
JMP_0
	clr  @songActive
	mov  @strDat+82, r1
	jeq  L12
	mov  @strDat+88, r1
	jne  JMP_1
	b    @L13
JMP_1
	dec  r1
	mov  r1, @strDat+88
	li   r1, >1
	mov  r1, @songActive
L12
	li   r9, strDat+42
	mov  r9, r13
	ai   r13, >6
	li   r3, songVol
	mov  r9, r15
	dect r15
	li   r14, >9000
	li   r5, getCompressedByte
	li   r4, >F000
	mov  *r9, r1
	jeq  L23
	mov  *r13, r1
	jeq  L24
L29
	dec  r1
	mov  r1, *r13
	li   r1, >1
	mov  r1, @songActive
L23
	ai   r9, >A
	ai   r13, >A
	inc  r3
	ai   r15, >A
	cb   r14, r4
	jeq  L28
L31
	ai   r14, >2000
	mov  *r9, r1
	jeq  L23
	mov  *r13, r1
	jne  L29
L24
	mov  r15, r1
	mov  r3, @>A(r10)
	mov  r4, @>E(r10)
	mov  r5, @>C(r10)
	bl   *r5
	mov  *r9, r2
	mov  @>A(r10), r3
	mov  @>E(r10), r4
	mov  @>C(r10), r5
	ci   r2, 0
	jne  JMP_2
	b    @L30
JMP_2
	li   r2, >1
	mov  r2, @songActive
	movb r1, r2
	srl  r2, 8
	andi r2, >F
	mov  r2, *r13
	srl  r1, >4
	socb r14, r1
	movb r1, @>8400
	movb r1, *r3
L33
	ai   r9, >A
	ai   r13, >A
	inc  r3
	ai   r15, >A
	cb   r14, r4
	jne  L31
L28
	mov  *r10+, r11
	mov  *r10+, r9
	mov  *r10+, r13
	mov  *r10+, r14
	mov  *r10, r15
	ai   r10, >8
	b    *r11
L13
	li   r13, getCompressedByte
	li   r1, strDat+80
	bl   *r13
	mov  @strDat+82, r2
	jne  JMP_3
	b    @L12
JMP_3
	li   r2, >1
	mov  r2, @songActive
	movb r1, r9
	srl  r9, 8
	mov  r9, r3
	andi r3, >F
	mov  r3, @strDat+88
	jeq  0
	cb  r1, @$-1
	jgt  JMP_4
	jeq  JMP_4
	b    @L32
JMP_4
L14
	mov  r9, r1
	andi r1, >40
	jeq  L17
	mov  @strDat+12, r1
	jeq  L17
	li   r1, strDat+10
	bl   *r13
	mov  @strDat+12, r2
	jne  JMP_5
	b    @L18
JMP_5
	srl  r1, 8
	a    r1, r1
	a    @toneoffset, r1
	a    @workBuf, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >A000
	mov  r2, r3
	mov  r2, r1
	swpb r1
	mov  r2, @songNote+2
	movb r3, @>8400
	movb r1, @>8400
L17
	mov  r9, r1
	andi r1, >20
	jeq  L20
	mov  @strDat+22, r1
	jeq  L20
	li   r1, strDat+20
	bl   *r13
	mov  @strDat+22, r2
	jne  JMP_6
	b    @L21
JMP_6
	srl  r1, 8
	a    r1, r1
	a    @toneoffset, r1
	a    @workBuf, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >C000
	mov  r2, r3
	mov  r2, r1
	swpb r1
	mov  r2, @songNote+4
	movb r3, @>8400
	movb r1, @>8400
L20
	andi r9, >10
	ci   r9, 0
	jne  JMP_7
	b    @L12
JMP_7
	mov  @strDat+32, r1
	jne  JMP_8
	b    @L12
JMP_8
	li   r1, strDat+30
	bl   *r13
	mov  @strDat+32, r2
	jne  JMP_9
	b    @L12
JMP_9
	ori  r1, >E000
	movb r1, @>8400
	srl  r1, 8
	mov  r1, @songNote+6
	b    @L12
L30
	li   r1, >F00
	socb r14, r1
	movb r1, @>8400
	movb r1, *r3
	b    @L33
L32
	mov  @strDat+2, r1
	jne  JMP_10
	b    @L14
JMP_10
	li   r1, strDat
	bl   *r13
	mov  @strDat+2, r2
	jeq  L15
	srl  r1, 8
	a    r1, r1
	a    @toneoffset, r1
	a    @workBuf, r1
	movb *r1+, r2
	srl  r2, 8
	andi r2, >F
	sla  r2, >8
	movb *r1, r1
	srl  r1, 8
	a    r1, r2
	ori  r2, >8000
	mov  r2, r3
	mov  r2, r1
	swpb r1
	mov  r2, @songNote
	movb r3, @>8400
	movb r1, @>8400
	b    @L14
L21
	clr  r1
	li   r3, >C100
	li   r2, >C100
	mov  r2, @songNote+4
	movb r3, @>8400
	movb r1, @>8400
	b    @L20
L18
	clr  r1
	li   r3, >A100
	li   r2, >A100
	mov  r2, @songNote+2
	movb r3, @>8400
	movb r1, @>8400
	b    @L17
L15
	clr  r1
	li   r3, >8100
	li   r2, >8100
	mov  r2, @songNote
	movb r3, @>8400
	movb r1, @>8400
	b    @L14
	.size	SongLoop, .-SongLoop
	cseg

	even
	def workBuf
workBuf
	bss 2

	even
strDat
	bss 90

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
	bss 2

	even
	def streamoffset
streamoffset
	bss 2

	even
	def toneoffset
toneoffset
	bss 2

	ref	getCompressedByte

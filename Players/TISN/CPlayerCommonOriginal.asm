	pseg
	even
getDatInline
	mov  *r1, r2
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
	.size	getDatInline, .-getDatInline
	even
getDatRLE
	mov  *r1, r1
	movb *r1, r1
	b    *r11
	.size	getDatRLE, .-getDatRLE
	even
getDatRLE32
	mov  *r1, r2
	c    r2, @>2(r1)
	jeq  L9
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
	jmp  L10
L9
	ai   r2, >FFFC
	mov  r2, *r1
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
L10
	.size	getDatRLE32, .-getDatRLE32
	even
getDatRLE16
	mov  *r1, r2
	c    r2, @>2(r1)
	jeq  L14
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
	jmp  L15
L14
	dect r2
	mov  r2, *r1
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
L15
	.size	getDatRLE16, .-getDatRLE16
	even
getDatRLE24
	mov  *r1, r2
	c    r2, @>2(r1)
	jeq  L19
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
	jmp  L20
L19
	ai   r2, >FFFD
	mov  r2, *r1
	mov  r2, r3
	inc  r3
	mov  r3, *r1
	movb *r2, r1
	b    *r11
L20
	.size	getDatRLE24, .-getDatRLE24
	even

	def	getCompressedByte
getCompressedByte
L37
	movb @>6(r1), r3
L36
	jeq  0
	cb  r3, @$-1
	jne  L38
L22
	mov  @>2(r1), r6
	mov  r6, r5
	inc  r5
	mov  r5, @>2(r1)
	movb *r6, r3
	srl  r3, 8
	mov  r3, r4
	andi r4, >E0
	ci   r4, >60
	jne  JMP_0
	b    @L26
JMP_0
	ci   r4, >60
	jgt  L30
	ci   r4, >20
	jeq  L24
	ci   r4, >40
	jne  JMP_1
	b    @L25
JMP_1
	ci   r4, 0
	jne  L37
L24
	li   r4, getDatInline
	mov  r4, @>4(r1)
	mov  r5, *r1
	andi r3, >3F
	inc  r3
	swpb r3
	movb r3, @>6(r1)
	movb r3, r4
	srl  r4, 8
	a    r4, r5
	mov  r5, @>2(r1)
	jeq  0
	cb  r3, @$-1
	jeq  L22
L38
	ai   r3, >FF00
	movb r3, @>6(r1)
	mov  @>4(r1), r3
	b    *r3
L30
	ci   r4, >A0
	jeq  L28
	ci   r4, >A0
	jgt  L31
	ai   r4, >FF80
	jne  L37
	li   r4, getDatRLE16
	mov  r4, @>4(r1)
	mov  r5, *r1
	ai   r6, >3
	mov  r6, @>2(r1)
	andi r3, >1F
	inct r3
	swpb r3
	ab   r3, r3
	movb r3, @>6(r1)
	b    @L36
L28
	li   r4, getDatRLE24
	mov  r4, @>4(r1)
	mov  r5, *r1
	ai   r6, >4
	mov  r6, @>2(r1)
	andi r3, >1F
	mov  r3, r4
	a    r3, r4
	a    r3, r4
	ai   r4, >6
	mov  r4, r3
	swpb r3
	movb r3, @>6(r1)
	b    @L36
L26
	li   r4, getDatRLE32
	mov  r4, @>4(r1)
	mov  r5, *r1
	ai   r6, >5
	mov  r6, @>2(r1)
	andi r3, >1F
	inct r3
	swpb r3
	andi r3, >FF00
	sla  r3, >2
	movb r3, @>6(r1)
	b    @L36
L25
	li   r4, getDatRLE
	mov  r4, @>4(r1)
	mov  r5, *r1
	inc  r5
	mov  r5, @>2(r1)
	andi r3, >1F
	ai   r3, >3
	swpb r3
	movb r3, @>6(r1)
	b    @L36
L31
	ci   r4, >C0
	jeq  L29
	ai   r4, >FF20
	jeq  JMP_2
	b    @L37
JMP_2
L29
	li   r4, getDatInline
	mov  r4, @>4(r1)
	movb @>1(r5), r4
	srl  r4, 8
	movb *r5, r6
	andi r6, >FF00
	a    r6, r4
	jeq  L39
	a    r2, r4
	mov  r4, *r1
	inct r5
	mov  r5, @>2(r1)
	andi r3, >3F
	ai   r3, >4
	swpb r3
	movb r3, @>6(r1)
	b    @L36
L39
	mov  r4, @>2(r1)
	clr  r1
	b    *r11
	.size	getCompressedByte, .-getCompressedByte

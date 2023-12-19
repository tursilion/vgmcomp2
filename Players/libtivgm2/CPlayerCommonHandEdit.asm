* code that is //shared// between multiple instances of the player
* Hand edit of CPlayerCommon.c assembly by Tursi
* The functions in this file can not be directly called from C.
* To call getCompressedByte directly, you need to move the
* input argument from R1 to R15 with your own wrapper function.
* Public Domain

    dseg
    even

* enable this and you can track how many of each datatype is hit
* you will have to search through the file for "cnt", there is
* one hit for each. Also, your program will need to manually
* zero them at the start.
*    def cntInline,cntRle,cntRle16,cntRle24,cntRle32,cntBack
*cntInline bss 2
*cntRle bss 2
*cntRle16 bss 2
*cntRle24 bss 2
*cntRle32 bss 2
*cntBack bss 2

	pseg
	ref workBuf

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.
R3LSB EQU >8307

*****************************************************************************
* in the original design for this code, the player used a workspace of
* >8320 and left the C workspace of >8300 alone. However, it looks
* like we can get away with abusing the system and sharing the workspace.
* If your assembly code needs to preserve registers, you can probably
* share your separate workspace with StartSong and StopSong and then
* just switch to >8300 for SongLoop only.

* do note that with this density, some of the gcc preserved registers
* are destroyed - particularly r8 is a frame pointer (--omit-frame-pointer?),
* though I havent seen it used, r9 is supposed to be preserved (not sure why),
* and r13-r15 are supposed to be preserved in case blwp was used (again, I dont
* think GCC generates blwp sequences). Of course, if you are using from asm,
* then you know what you need and what to do with.

* TLDR: if you share one workspace, then you can assume everything gets used
* when you call SongLoop. If you split the workspaces, here are the separate
* usages.

* register usage for interface functions (StartSong and StopSong)
* R0 =                      R8 = 
* R1 = arg1,scratch,return  R9 = 
* R2 = arg2,scratch         R10= stack pointer (if used from C, not touched)
* R3 = scratch              R11= return address
* R4 = scratch              R12= 
* R5 =                      R13= 
* R6 =                      R14= 
* R7 =                      R15= 

* register usage for SongLoop and getCompressedByte
* R0 = SongLoop scratch     R8 = address of sound chip
* R1 = scratch,return       R9 = SongLoop scratch 
* R2 = scratch              R10= stack pointer (if used from C, not touched)
* R3 = scratch              R11= return address
* R4 = scratch              R12= SongLoop scratch
* R5 = scratch              R13= address of getCompressedByte
* R6 = contains >0100       R14= SongLoop scratch
* R7 = SongLoop scratch     R15= getCompressedByte arg1, SongLoop scratch

* TMS9900 GCC Register classification (up till 2023/12/12)
* R0 - Volatile, Bit shift count
* R1 - Volatile, Argument 1, return value 1
* R2 - Volatile, Argument 2, return value 2
* R3 - Volatile, Argument 3
* R4 - Volatile, Argument 4
* R5 - Volatile, Argument 5
* R6 - Volatile, Argument 6
* R7 - Volatile, Argument pointer
* R8 - Volatile, Frame pointer
* R9 - Preserved across BL calls
* R10 (SP) - Preserved across BL calls, Stack pointer
* R11 (LR) - Preserved across BL calls, Return address after BL
* R12 (CB) - Volatile, CRU base
* R13 (LW) - Preserved across BL calls, Old workspace register after BLWP
* R14 (LP) - Preserved across BL calls, Old program counter after BLWP
* R15 (LS) - Preserved across BL calls, Old status register after BLWP

* TMS9900 GCC Register classification (after 2023/12/12)
* R0 - Volatile, Bit shift count
* R1 - Volatile, Argument 1, return value 1
* R2 - Volatile, Argument 2, return value 2
* R3 - Volatile, Argument 3
* R4 - Volatile, Argument 4
* R5 - Volatile, Argument 5
* R6 - Volatile, Argument 6
* R7 - Volatile, Argument pointer
* R8 - Volatile, general purpose
* R9 - Volatile, general purpose
* R10 (SP) - Stack pointer
* R11 (LR) - Preserved across BL calls, Return address after BL
* R12 (CB) - Volatile, CRU base
* R13 - Volatile, general purpose
* R14 - frame pointer
* R15 - general purpose?

*****************************************************************************

* some useful data
    def getDatInline,getDatRLE16,getDatRLE24,getDatRLE32,getDatRLE
JMPTAB DATA L24,L24,L25,L26,L30_B,L28,L29,L29
ADRINLINE DATA getDatInline
ADRRLE16  DATA getDatRLE16
ADRRLE24  DATA getDatRLE24
ADRRLE32  DATA getDatRLE32
ADRRLE    DATA getDatRLE
	
* uint8 getDatInline(STREAM *str)
* just pull a string of bytes
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
	even
getDatInline
	mov *r15,r2		* get curptr
gdi2	
	inc *r15		* inc curptr
	movb *r2,r1		* get byte
	b    *r11       * r2 must be non-zero
	.size	getDatInline, .-getDatInline


* uint8 getDatRLE(STREAM *str)
* pull the single byte - no increment
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
	even
getDatRLE
	mov  *r15,r2	* get curptr (no increment)
	movb *r2,r1	    * get byte
	b    *r11       * r2 must be non-zero
	.size	getDatRLE, .-getDatRLE

* uint8 getDatRLE32(STREAM *str)
* pull the last four bytes over and over
* mainPtr is assumed already incremented
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
* mainptr is offset 2
	even
getDatRLE32
	mov *r15,r2			* fetch curptr
	c r2,@>2(r15)		* compare curptr against mainptr
	jne gdi2			* increment r15 and fetch from r2

	ai r2,>fffc			* subtract 4
    movb *r2+,r1        * get the byte and increment
	mov r2,*r15			* write it back
	b *r11              * r2 must be non-zero
	.size	getDatRLE32, .-getDatRLE32
	
* uint8 getDatRLE16(STREAM *str)
* pull the last two bytes over and over
* mainPtr is assumed already incremented
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
* mainptr is offset 2
	even
getDatRLE16
	mov *r15,r2			* fetch curptr
	c r2,@>2(r15)		* compare curptr against mainptr
	jne gdi2			* increment r15 and fetch from r2

	dect r2 			* subtract 2
    movb *r2+,r1        * get the byte and increment
	mov r2,*r15			* write it back
	b *r11              * r2 must be non-zero
	.size	getDatRLE16, .-getDatRLE16


* uint8 getDatRLE24(STREAM *str)
* pull the last three bytes over and over
* mainPtr is assumed already incremented
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
* mainptr is offset 2
	even
getDatRLE24
	mov *r15,r2			* fetch curptr
	c r2,@>2(r15)		* compare curptr against mainptr
	jne gdi2			* increment r15 and fetch from r2

	ai r2,>FFFD			* subtract 3
    movb *r2+,r1        * fetch the byte and increment
	mov r2,*r15			* write it back
	b *r11              * r2 must be non-zero
	.size	getDatRLE24, .-getDatRLE24
	even

* unpack a stream byte - offset and maxbytes are used to write a scaled
* address for the heatmap to display later
* cnt is row count, and maxbytes is used for scaling, max size of data
* uint8 getCompressedByte(STREAM *str)
* r15 = str (and curptr is offset 0)
* r6 /must/ contain >0100 on entry
* r2 will be zero if timestream was ended
* mainptr is offset 2, curType is 4, curBytes is 6
	def	getCompressedByte,getDatZero

getCompressedByte
	sb   r6,@>6(r15)        * decrement curBytes
    jnc  L22                * if it was zero, we need a new one

	mov  @>4(r15),r3        * else get curType pointer
	b    *r3			    * call it (it will return)

L22
	mov  @>2(r15),r5        * get mainptr
	movb *r5+,r3		    * get command byte and increment (not writing back till later)
	swpb r3                 * we always need to do this, so do it here to save bytes (remember MSB is garbage)
	mov  r3,r4			    * make a copy (MSB is still garbage)
	andi r4,>e0			    * get command bits only (fixes MSB)
    srl r4,4                * get them down to a word index (value*2)
    mov @JMPTAB(r4),r2      * get address to jump to
	b *r2       		    * jump to the correct setup function (r3 has original cmd, r5 has mainptr)
	
L24
	mov @ADRINLINE,@4(r15)	* set curType (Inline)
*   inc @cntInline
	movb *r5+,r1            * get the byte - we know theres at least one!
	mov  r5,*r15            * set curPtr to mainPtr after fetch
	andi r3,>3F 			* get count from command (fixes MSB too!)
*	inc  r3					* add minimum size of 1 (but dont do this, because we will fetch the first byte)
	movb @R3LSB,@>6(r15)   	* store to curbytes
	a    r3,r5				* add length of string to address
	mov  r5,@>2(r15)		* store back to mainPtr (note that we got the 1 we dropped from curbytes for free from the postinc)
	b    *r11               * r2 must be non-zero (contains setup function)

L30_B
	mov @ADRRLE16,@4(r15)	* set curType (RLE16)
*   inc @cntRle16
	movb *r5+,r1            * get the byte - we know theres at least one!
	mov  r5,*r15            * set curPtr to mainPtr after fetch
	inc r5  				* add cost of 2 to mainPtr, minus 1 for the fetch above
	mov  r5,@>2(r15)    	* store mainPtr
	andi r3,>1F			    * get command count (fixes MSB)
	inct r3					* add default count of 2
	sla  r3,>1				* multiply by 2
	dec  r3                 * minus the one we are taking
	movb @R3LSB,@>6(r15)    * save curBytes
	b    *r11               * r2 must be non-zero (contains setup function)
	
L28
	mov @ADRRLE24,@4(r15)	* set curType (RLE24)
*   inc @cntRle24
	movb *r5+,r1            * get the byte - we know theres at least one!
	mov  r5,*r15            * set curPtr to mainPtr after fetch
	ai   r5,>2				* add cost of 3 to mainPtr, minus 1 for the fetch above
	mov  r5,@>2(r15)    	* store mainptr
	andi r3,>1F			    * mask out command bits (fixes MSB)
	mov  r3,r4				* make a copy
	a    r4,r3				* x2
	a    r4,r3				* x3
	ai   r3,5				* add 6 (minimum 2, x3), minus the one we are taking
	movb @R3LSB,@>6(r15)    * write to curBytes
	b    *r11               * r2 must be non-zero (contains setup function)

L26
	mov @ADRRLE32,@4(r15)	* set curType (RLE32)
*   inc @cntRle32
	movb *r5+,r1            * get the byte - we know theres at least one!
	mov  r5,*r15            * set curPtr to mainPtr after fetch
	ai   r5,>3				* add cost of 4 to mainPtr, minus 1 for the fetch above
	mov  r5,@>2(r15)    	* store mainPtr
	andi r3,>1F			    * get command count (fixes MSB)
	inct r3					* add default count of 2
	sla  r3,>2				* multiply by 4
	dec  r3                 * minus the one we are taking
	movb @R3LSB,@>6(r15)    * save curBytes
	b    *r11               * r2 must be non-zero (contains setup function)

L25
	mov @ADRRLE,@4(r15)		* set curType (RLE)
*   inc @cntRle
	mov  r5,*r15            * set curPtr to mainPtr (this is where well pull from)
	movb *r5+,r1            * get the byte - we know theres at least one! Increment past it for mainPtr.
	mov  r5,@>2(r15) 		* save mainPtr
	andi r3,>1F		    	* get command count
	inct r3  				* add default of 3, minus the one we are taking
	movb @R3LSB,@>6(r15)    * save curBytes
	b    *r11               * r2 must be non-zero (contains setup function)

L29
	mov @ADRINLINE,@4(r15)	* BACKREF uses inline - only type that can end a stream
*   inc @cntBack
	movb @1(r5),r4			* get lsb - may be misaligned so cant use MOV
	swpb r4
	movb *r5,r4				* get msb
	mov  r4,r4				* zero means end of stream
	jeq  L39				* if zero, go clean up
	
	a    r5,r4		        * add current mainPtr to the offset
	movb *r4+,r1            * get the byte
	mov  r4,*r15    		* save to curPtr
	inct r5					* add cost of 2 to mainPtr
	mov  r5,@>2(r15)    	* save mainPtr
	andi r3,>3F			    * mask out just the count bits (fixes MSB)
	ai   r3,>3				* add minimum of 4 (minus one cause we are taking one)
	movb @R3LSB,@>6(r15)    * save curBytes
	b    *r11               * r2 must be non-zero (contains setup function)

L39
	clr  @>2(r15)		    * zero out mainPtr
    clr r2                  * flag a dead return

* while part of getCompressedByte, this is also all we need for getDatZero
getDatZero
	clr  r1					* zero out return
	b    *r11				* back to caller - r2 may be anything but is probably zero
	.size	getCompressedByte, .-getCompressedByte

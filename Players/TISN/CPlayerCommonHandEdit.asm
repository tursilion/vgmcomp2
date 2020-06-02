* code that is shared between multiple instances of the player
* Hand edit of CPlayerCommon.c assembly by Tursi
* Public Domain

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
* though I haven't seen it used, r9 is supposed to be preserved (not sure why),
* and r13-r15 are supposed to be preserved in case blwp was used (again, I don't
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
* R0 =                      R8 = SongLoop scratch
* R1 = arg1,scratch,return  R9 = SongLoop scratch 
* R2 = arg2,scratch         R10= stack pointer (if used from C, not touched)
* R3 = scratch              R11= return address
* R4 = scratch              R12= SongLoop scratch
* R5 = scratch              R13= SongLoop scratch
* R6 = scratch              R14= SongLoop scratch
* R7 = SongLoop scratch     R15= SongLoop scratch

*****************************************************************************

* some useful data
    def DAT0001
DAT0001 DATA >0001
JMPTAB DATA L24,L24,L25,L26,L30_B,L28,L29,L29
ADRINLINE DATA getDatInline
ADRRLE16  DATA getDatRLE16
ADRRLE24  DATA getDatRLE24
ADRRLE32  DATA getDatRLE32
ADRRLE    DATA getDatRLE
	
* uint8 getDatInline(STREAM *str, uint8 *buf)
* just pull a string of bytes
* r1 = str (and curptr is offset 0), buf not needed
	even
    def getDatInline
getDatInline
	mov *r1,r2		* get curptr
gdi2	
	inc *r1			* inc curptr
	movb *r2,r1		* get byte
	b    *r11
	.size	getDatInline, .-getDatInline


* uint8 getDatRLE(STREAM *str, uint8 *buf)
* pull the single byte - no increment
* r1 = str (and curptr is offset 0), buf not needed
	even
    def getDatRLE
getDatRLE
	mov  *r1,r1	    * get curptr (no increment)
	movb *r1,r1	    * get byte
	b    *r11
	.size	getDatRLE, .-getDatRLE

* uint8 getDatRLE32(STREAM *str, uint8 *buf)
* pull the last four bytes over and over
* mainPtr is assumed already incremented
* r1 = str (and curptr is offset 0), buf not needed
* mainptr is offset 2
	even
    def getDatRLE32
getDatRLE32
	mov *r1,r2			* fetch curptr
	c r2,@>2(r1)		* compare curptr against mainptr
	jne gdi2			* increment r1 and fetch from r2

	ai r2,>fffc			* subtract 4
    movb *r2+,r3        * get the byte and increment
	mov r2,*r1			* write it back
    movb r3,r1          * set return
	b *r11
	.size	getDatRLE32, .-getDatRLE32
	
* uint8 getDatRLE16(STREAM *str, uint8 *buf)
* pull the last two bytes over and over
* mainPtr is assumed already incremented
* r1 = str (and curptr is offset 0), buf not needed
* mainptr is offset 2
	even
    def getDatRLE16
getDatRLE16
	mov *r1,r2			* fetch curptr
	c r2,@>2(r1)		* compare curptr against mainptr
	jne gdi2			* increment r1 and fetch from r2

	dect r2				* subtract 2
	movb *r2+,r3        * get the byte and increment
	mov r2,*r1			* write it back
	movb r3,r1          * set return
	b *r11
	.size	getDatRLE16, .-getDatRLE16


* uint8 getDatRLE24(STREAM *str, uint8 *buf)
* pull the last three bytes over and over
* mainPtr is assumed already incremented
* r1 = str (and curptr is offset 0), buf not needed
* mainptr is offset 2
	even
	def	getDatRLE24
getDatRLE24
	mov *r1,r2			* fetch curptr
	c r2,@>2(r1)		* compare curptr against mainptr
	jne gdi2			* increment r1 and fetch from r2

	ai r2,>FFFD			* subtract 3
    movb *r2+,r3        * fetch the byte and increment
	mov r2,*r1			* write it back
    movb r3,r1          * set return
	b *r11
	.size	getDatRLE24, .-getDatRLE24
	even

* unpack a stream byte - offset and maxbytes are used to write a scaled
* address for the heatmap to display later
* cnt is row count, and maxbytes is used for scaling, max size of data
* uint8 getCompressedByte(STREAM *str, uint8 *buf)
* r1 = str (and curptr is offset 0), buf is unused and not provided
* mainptr is offset 2, curType is 4, curBytes is 6
	def	getCompressedByte,getDatZero
getCompressedByte
	cb @6(r1),@DAT0001	    * compare curBytes to 0
	jeq  L22			    * jump if yes (most common case is fallthrough)
    sb @DAT0001+1,@>6(r1)   * curBytes > 0 - decrement it
	mov  @>4(r1),r3	        * get curType pointer
	b    *r3			    * call it (it will return)

L22
	mov  @>2(r1),r5	        * get mainptr
	movb *r5+,r3		    * get command byte and increment (not writing back till later)
	swpb r3                 * we always need to do this, so do it here to save bytes (remember MSB is garbage)
	mov  r3,r4			    * make a copy (MSB is still garbage)
	andi r4,>e0			    * get command bits only (fixes MSB)
    srl r4,4                * get them down to a word index (value*2)
    mov @JMPTAB(r4),r4      * get address to jump to
	b *r4       		    * jump to the correct setup function (r3 has original cmd, r5 has mainptr)
	
L24
	mov @ADRINLINE,@4(r1)	* set curType
	movb *r5+,r6            * get the byte - we know theres at least one!
	mov  r5,*r1             * set curPtr to mainPtr after fetch
	andi r3,>3F 			* get count from command (fixes MSB too!)
*	inc  r3					* add minimum size of 1 (but dont do this, because we will fetch the first byte)
	movb @R3LSB,@>6(r1)    	* store to curbytes
	a    r3,r5				* add length of string to address
	mov  r5,@>2(r1) 		* store back to mainPtr (note that we got the 1 we dropped from curbytes for free from the postinc)
	movb r6,r1              * return value
	b    *r11

L30_B
	mov @ADRRLE16,@4(r1)	* set curType
	movb *r5+,r6            * get the byte - we know theres at least one!
	mov  r5,*r1             * set curPtr to mainPtr after fetch
	inc  r5					* add cost of 2 bytes, minus the one we took above
	mov  r5,@>2(r1)		    * write mainPtr back
	andi r3,>1F			    * get count from command (fixes MSB too!)
	inct r3					* add minimum size of 2
	a    r3,r3				* double the count (cause 2 each cycle)
	dec  r3                 * were going to take one now
	movb @R3LSB,@>6(r1)     * write out curBytes
	movb r6,r1              * return value
	b    *r11
	
L28
	mov @ADRRLE24,@4(r1)	* set curType
	movb *r5+,r6            * get the byte - we know theres at least one!
	mov  r5,*r1             * set curPtr to mainPtr after fetch
	ai   r5,>2				* add cost of 3 to mainPtr, minus 1 for the fetch above
	mov  r5,@>2(r1)	    	* store mainptr
	andi r3,>1F			    * mask out command bits (fixes MSB)
	mov  r3,r4				* make a copy
	a    r4,r3				* x2
	a    r4,r3				* x3
	ai   r3,5				* add 6 (minimum 2, x3), minus the one we are taking
	movb @R3LSB,@>6(r1)     * write to curBytes
	movb r6,r1              * return value
	b    *r11

L26
	mov @ADRRLE32,@4(r1)	* set curType
	movb *r5+,r6            * get the byte - we know theres at least one!
	mov  r5,*r1             * set curPtr to mainPtr after fetch
	ai   r5,>3				* add cost of 4 to mainPtr, minus 1 for the fetch above
	mov  r5,@>2(r1)	    	* store mainPtr
	andi r3,>1F			    * get command count (fixes MSB)
	inct r3					* add default count of 2
	sla  r3,>2				* multiply by 4
	dec  r3                 * minus the one we are taking
	movb @R3LSB,@>6(r1)     * save curBytes
	movb r6,r1              * return value
	b    *r11

L25
	mov @ADRRLE,@4(r1)		* set curType
	movb *r5,r6             * get the byte - we know theres at least one! DO NOT INCREMENT!
	mov  r5,*r1             * set curPtr to mainPtr
	inc  r5					* add cost of 1 to mainPtr (there was no increment on the fetch!)
	mov  r5,@>2(r1) 		* save mainPtr
	andi r3,>1F		    	* get command count
	inct r3  				* add default of 3, minus the one we are taking
	movb @R3LSB,@>6(r1)	    * save curBytes
	movb r6,r1              * return value
	b    *r11

L29
	mov @ADRINLINE,@4(r1)	* BACKREF uses inline - only type that can end a stream
	movb @1(r5),r4			* get lsb - may be misaligned so cant use MOV
	swpb r4
	movb *r5,r4				* get msb
	mov  r4,r4				* zero means end of stream
	jeq  L39				* if zero, go clean up
	
	mov @workBuf,r2			* get address of song base
	a    r2,r4				* add it to the offset
	movb *r4+,r6            * get the byte
	mov  r4,*r1	    		* save to curPtr
	inct r5					* add cost of 2 to mainPtr
	mov  r5,@>2(r1)	    	* save mainPtr
	andi r3,>3F			    * mask out just the count bits (fixes MSB)
	ai   r3,>3				* add minimum of 4 (minus one cause we are taking one)
	movb @R3LSB,@>6(r1)	    * save curBytes
	movb r6,r1              * return value
	b    *r11

L39
	clr  @>2(r1)		    * zero out mainPtr

* while part of getCompressedByte, this is also all we need for getDatZero
getDatZero
	clr  r1					* zero out return
	b    *r11				* back to caller
	.size	getCompressedByte, .-getCompressedByte

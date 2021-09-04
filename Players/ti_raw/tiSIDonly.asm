* enable these DEFs if you need them
*    def getDatInline,getDatRLE16,getDatRLE24,getDatRLE32,getDatRLE
*	def	getCompressedByte,getDatZero
*	def	SIDLoop
*	def	StartSID
*	def	StopSID
*    def sidDat
*	def sidVol
*	def sidNote
*	def workSID
*    def sidSave
*    def SidCtrl1,SidCtrl2,SidCtrl3

    even
sidDat
	bss 72
sidVol
	bss 4
sidNote
	bss 8
workSID
	bss 2
sidSave
    bss 2
* defined in reverse order on purpose so the volume loop can count down
SidCtrl3 bss 3

    even

R3LSB EQU >8307
R2LSB EQU >8305
songActive EQU sidNote+7
SidCtrl2 equ SidCtrl3+1
SidCtrl1 equ SidCtrl3+2

    even
* code that is //shared// between multiple instances of the player
* Hand edit of CPlayerCommon.c assembly by Tursi
* The functions in this file can not be directly called from C.
* To call getCompressedByte directly, you need to move the
* input argument from R1 to R15 with your own wrapper function.
* Public Domain

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


* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.

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

* TMS9900 GCC Register classification
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

*****************************************************************************

* some useful data
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


* uint8 getDatRLE(STREAM *str)
* pull the single byte - no increment
* r15 = str (and curptr is offset 0), buf not needed
* return in r1, r2 must be non-zero
	even
getDatRLE
	mov  *r15,r2	* get curptr (no increment)
	movb *r2,r1	    * get byte
	b    *r11       * r2 must be non-zero

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
	even

* unpack a stream byte - offset and maxbytes are used to write a scaled
* address for the heatmap to display later
* cnt is row count, and maxbytes is used for scaling, max size of data
* uint8 getCompressedByte(STREAM *str, uint8 *buf)
* r15 = str (and curptr is offset 0), buf is unused and not provided
* r6 /must/ contain >0100 on entry
* r2 will be zero if timestream was ended
* mainptr is offset 2, curType is 4, curBytes is 6

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
* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi, with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300


* SongActive is stored in the LSB of the noise channel


* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in sidSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* NOTE: this will enable the SID Blaster, do NOT call with any DSR enabled!
* it also changes the keyboard select column
	even

SIDLoop
    movb @songActive,r1     * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    clr  r12                * prepare to write CRU
    sbo  >24                * write keyboard select - this maps in the SID Blaster
    mov  r11,@sidSave       * save the return address
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r6,>0100           * 1 in a byte for byte math

	mov  @sidDat+66,r1      * timestream mainPtr
	jne  HASTIMESTR         * keep working if we still have a timestream

NOTIMESTR
    clr  r7                 * no timestream - zero outSongActive for volume loop
    jmp  VOLLOOP            * go work on volumes

HASTIMESTR
    sb r6,@sidDat+71        * decrement the timestream frames left
    jnc  DOTIMESTR          * if it was zero, jump over volumes to get a new byte
DONETONEACT
	seto r7                 * set outSongActive to true for later testing

* volume processing loop
VOLLOOP
	li   r15,sidDat+32      * stream 4 curPtr (vol[0])  r15
	li   r0,sidVol          * sidVol table pointer      r0
	li   r14,3              * process channel count     r14
    movb @songActive,r12    * actual mute flags         r12
    li   r8,>580C           * sustain/release voice 1   R8
	jmp  VLOOPEND           * jump to bottom of loop

VLOOPDEC
	seto r7                 * set outSongActive to true for later

VLOOPSHIFT
    sla r12,1               * if we come here, we still needed to shift
	inc  r0                 * next sidVol pointer

VLOOPNEXT
	ai   r15,>8             * next curPtr
    ai   r8,14              * next voice
	dec  r14                * count down (faster than a CI)
    jeq  VLOOPDONE          * that was the last one

VLOOPEND
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  VLOOPSHIFT         * if not, execute next loop after shifting r12
    sb r6,@7(r15)           * decrement frames left
    joc VLOOPDEC            * was not yet zero, next loop (will shift r12)

	bl   *r13               * and call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  VNEWVOL            * didnt end, go load it

	li   r1,>F000           * volume stream ended, load fixed volume >0f in high nibble
	jmp  VLOADVOL           * and go give it to the sound chip

VNEWVOL
	movb r1,r2              * make a copy of the byte
	andi r2,>F00            * extract frame count
	movb r2,@7(r15)         * and save it
	andi r1,>F000           * mask out the volume nibble (right place for SID)
	seto r7                 * set outSongActive to true for later

VLOADVOL
    sla  r12,1              * check the mute map - carry means muted (test and update in one!)
	joc  VMUTED             * if the bit was set, skip the write (we still want the rest!)
	movb r1,*r8             * write to the sound chip
    cb r1,*r0               * test direction of change, if any
    jle VMUTED              * less than or equal, no action needed

    movb @SidCtrl3-1(r14),r2   * get the control byte for this voice (gate bit is cleared)
    movb r2,@-4(r8)         * clear the gatebit (causes a release, but thats okay)
    ori r2,>0100            * set the gatebit
    movb r2,@-4(r8)         * set it (causes a 2ms attack, then a max 6ms decay to desired volume)

VMUTED
	movb r1,*r0+            * write the byte to sidVol as well, and increment
    jmp VLOOPNEXT           * next loop

VLOOPDONE
    clr  @>5832             * force a write to the read-only POTX reg to ensure we move the address latch
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  RETHOME            * skip if not zero
    movb r7,@songActive	 	* turn off the active bit and the mutes (this BYTE writes a >00)

RETHOME
    mov  @sidSave,r11       * back to caller
RETHOME2
	b    *r11

* handle new timestream event
DOTIMESTR
	li   r15,sidDat+64      * timestream curPtr
	bl   *r13               * getCompressedByte
    mov  r2,r2              * check if stream was ended
    jeq  NOTIMESTR  		* skip ahead if it was zero

	movb r1,r9              * make a copy
	andi r1,>F00            * get framesleft
	movb r1,@sidDat+71      * save in timestream framesLeft

    li r8,>5800             * base address of the sound chip (frequency voice 1)
	li r15,sidDat			* start with stream 0, curPtr
	li r0,sidNote			* output pointer for sidNote
	mov @workSID,r7         * get song address
	movb @songActive,r12	* get the songActive mutes

CKTONE1
    sla  r9,1               * test the timestream bit
	jnc  CKTONE2SHIFT       * not set, so skip with r12 shift
	mov  @2(r15),r1         * stream mainPtr
	jeq  CKTONE2SHIFT       * if zero, skip with r12 shift
	bl   *r13               * call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  TONETAB1

	li   r2,>0100           * stream ran out, load lowest pitch (we cant mute by freq alone)
	jmp  WRTONE1            * and go set it up

TONETAB1
*	srl  r1,8               * make note into word
*	sla  r1,1               * multiply by 2 to make index (cant merge the shifts can lsb must be zero)
    srl  r1,7               * this is disgusting, but we get away with it cause of the 15-bit address lines...
    a    @2(r7),r1          * add offset of tone table (word aligned)
	a    r7,r1              * add address of song (word aligned)
    mov  *r1,r2             * get the tone (note: little endian byte order)

WRTONE1
	mov  r2,*r0+            * save the result in sidNote and increment
	sla  r12,1              * check songActive - carry is mute
	joc  CKTONE2            * jump over if muted
	movb r2,*r8             * move command byte to sound chip
	movb @R2LSB,@2(r8)      * move other byte to sound chip (is this really faster? ;) )
	
CKTONE2
	ai r15,>8				* next curPtr
    ai r8,14                * next voice
    ci r8,>582A             * are we done?
	jne CKTONE1             * no, do the next one
	jmp DONETONEACT         * go work on the volumes with outSongActive set

CKTONE2SHIFT
	sla r12,1               * shift just to stay in sync
	inct r0					* next sidNote
	jmp CKTONE2

* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi, with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* This file contains start/stop and the data, the play function
* itself is in its own file.
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.


* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300

* SongActive is stored in the LSB of the noise channel

* Call this function to prepare to play
* enables the SID - do not call with any DSRs active
* it also changes the keyboard select column
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play in MSB (byte, starts at 0)
    even
StartSID
    li r3,18            * each table is 18 bytes (warning: StopSID uses this - dont move it!)
    srl r2,8            * make byte in R2 into a word
    mpy r2,r3           * multiply, result in r3/r4 (so r4)
	mov *r1,r3          * get pointer to stream indexes into r3
	a    r4,r3          * add song offset to stream offset to get base stream for this song
	a    r1,r3          * add buf to make it a memory pointer (also word aligned)
	li   r2,sidDat+6    * point to the first sidDats "curBytes" with r2
STARTLP
    mov *r3+,r4         * get stream offset from table and increment pointer
    jeq  nullptr        * if it was >0000, dont add the base address
	a    r1,r4          * make it a memory pointer
nullptr
	mov  r4,@>FFFC(r2)  * save as mainPtr
	clr  @>FFFA(r2)     * zero curPtr
	clr  *r2            * zero curBytes and framesLeft
	li   r4,getDatZero
	mov  r4,@>FFFE(r2)  * set curType to getSidZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,sidDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLP        * loop around if not done

	li   r2,>1          * set all three notes to >0001
	mov  r2,@sidNote
	mov  r2,@sidNote+2
	mov  r2,@sidNote+4

	mov  r1,@workSID    * store the song pointer in the global
	li   r2,>0101       * init value for noise with songActive bit set
	mov  r2,@sidNote+6 * set the noise channel note and songActive bit

    clr  r12            * activate the SID blaster by writing a console CRU
    sbo  >24            * keyboard, so I know its safe to tweak

* SID blaster is mapped in when any CRU less than >1000 is accessed
* The registers respond starting at >5800, and require an 80ns hold
* time. Each write is latched, so when finished write to one of the
* read-only registers to prevent continuous writes from causing
* problems with the sound generation. Here we will set defaults to
* all 25 writable registers.
* Well use full word writes to get the proper increment, even though
* only the even bytes (?) are taken.
    li r1,>5800     * register 0
    li r2,>0101     * byte 01 for frequency
    li r3,>0808     * byte 08 for pwm
    li r4,>0f0f     * byte 0f for master volume

* voice 1
    mov r2,*r1+     * freq lo
    clr *r1+        * freq hi
    clr *r1+        * PW lo
    mov r3,*r1+     * PW hi
    movb @SidCtrl1,r5   * get SID control
    andi r5,>FE00       * make sure gate is zeroed
    movb r5,@SidCtrl1   * write it back
    ori r5,>0100        * enable gate
    movb r5,*r1     * control
    inct r1
    clr *r1+        * attack/decay fastest
    clr *r1+        * zero sustain, fastest release

* voice 2
    mov r2,*r1+     * freq lo
    clr *r1+        * freq hi
    clr *r1+        * PW lo
    mov r3,*r1+     * PW hi
    movb @SidCtrl2,r5   * get SID control
    andi r5,>FE00       * make sure gate is zeroed
    movb r5,@SidCtrl2   * write it back
    ori r5,>0100        * enable gate
    movb r5,*r1     * control
    inct r1
    clr *r1+        * attack/decay fastest
    clr *r1+        * zero sustain, fastest release

* voice 3
    mov r2,*r1+     * freq lo
    clr *r1+        * freq hi
    clr *r1+        * PW lo
    mov r3,*r1+     * PW hi
    movb @SidCtrl3,r5   * get SID control
    andi r5,>FE00       * make sure gate is zeroed
    movb r5,@SidCtrl3   * write it back
    ori r5,>0100        * enable gate
    movb r5,*r1     * control
    inct r1
    clr *r1+        * attack/decay fastest
    clr *r1+        * zero sustain, fastest release

* config
    clr *r1+        * min filter lo
    clr *r1+        * min filter hi
    clr *r1+        * filter config
    mov r4,*r1+     * no notch filter, max master volume

    clr *r1+        * this actually writes to read-only POTX, but gets the address latch moved

setMutes
    clr @sidVol        * zero sidVol 0 and 1
    clr @sidVol+2      * zero sidVol 2 and 3
	b    *r11

* Call this to stop the current song
	even
StopSID
    movb @StartSID+2,@songActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
	jmp setMutes

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)

	even

	even

	even

	even

    even

    even

* we only need a special getCompressedSid if we have to use
* a different song bank. If, someday, we can share the same
* songbank, just sharing workBuf (instead of workSID) is all
* need to share the common code with the SN player.

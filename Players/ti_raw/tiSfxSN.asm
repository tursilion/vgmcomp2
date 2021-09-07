* enable these DEFs if you need them
*    def getDatInline,getDatRLE16,getDatRLE24,getDatRLE32,getDatRLE
*	def	getCompressedByte,getDatZero
*	def	SongLoop
*	def	StartSong
*	def	StopSong
*    def strDat
*	def songVol
*	def songNote
*	def workBuf
*    def retSave
*	def	SfxLoop
*	def	StartSfx
*	def	StopSfx
*    def sfxDat
*    def sfxActive
*	def sfxWorkBuf
*    def sfxSave

    even
strDat
	bss 72
songVol
	bss 4
songNote
	bss 8
workBuf
	bss 2
retSave
    bss 2
sfxDat
	bss 72
sfxActive
    bss 2
sfxWorkBuf
	bss 2
sfxSave
    bss 2

R3LSB EQU >8307
R2LSB EQU >8305
songActive EQU songNote+7

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
* uint8 getCompressedByte(STREAM *str)
* r15 = str (and curptr is offset 0)
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

* access the data

* SongActive is stored in the LSB of the noise channel

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in retSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
	even

SongLoop
    movb @songActive,r1     * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    mov  r11,@retSave       * save the return address
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r8,>8400           * address of the sound chip (warning: don''t move too much, my sample code relies on this offset)
    li   r6,>0100           * 1 in a byte for byte math

	mov  @strDat+66,r1      * timestream mainPtr
	jne  HASTIMESTR         * keep working if we still have a timestream

NOTIMESTR
    clr  r7                 * no timestream - zero outSongActive for volume loop
    jmp  DONETONEACT        * go work on volumes

HASTIMESTR
	seto r7                 * set outSongActive to true for later testing
    sb r6,@strDat+71        * decrement the timestream frames left
    jnc  DOTIMESTR          * if it was zero, jump over volumes to get a new byte

* volume processing loop
DONETONEACT
	li   r15,strDat+32      * stream 4 curPtr (vol[0])  r15
	li   r0,songVol         * songVol table pointer     r0
	li   r14,>9000          * command nibble            r14
    movb @songActive,r12    * actual mute flags         r12
	jmp  VLOOPEND           * jump to bottom of loop

VLOOPDEC
	seto r7                 * set outSongActive to true for later

VLOOPSHIFT
    sla r12,1               * if we come here, we still needed to shift
	inc  r0                 * next songVol pointer

VLOOPNEXT
	ai   r15,>8             * next curPtr
	ai   r14,>2000          * next command nibble
    joc  VLOOPDONE          * that was the last one

VLOOPEND
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  VLOOPSHIFT         * if not, execute next loop after shifting r12
    sb r6,@7(r15)           * decrement frames left
    joc VLOOPDEC            * was not yet zero, next loop (will shift r12)

	bl   *r13               * and call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  VNEWVOL            * didnt end, go load it

	li   r1,>F00            * volume stream ended, load fixed volume >0f
	jmp  VLOADVOL           * and go give it to the sound chip

VNEWVOL
	movb r1,r2              * make a copy of the byte
	andi r2,>F00            * extract frame count
	movb r2,@7(r15)         * and save it
	srl  r1,>4              * shift down the volume nibble
	seto r7                 * set outSongActive to true for later

VLOADVOL
	socb r14,r1             * merge in the command bits

    sla  r12,1              * check the mute map - carry means muted (test and update in one!)
	joc  VMUTED             * if the bit was set, skip the write (we still want the rest!)
	movb r1,*r8             * write to the sound chip

VMUTED
	movb r1,*r0+            * write the byte to songVol as well, and increment
    jmp VLOOPNEXT           * next loop

VLOOPDONE
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  RETHOME            * skip if not zero
    movb r7,@songActive	 	* turn off the active bit and the mutes (this BYTE writes a >00)

RETHOME
    mov  @retSave,r11       * back to caller
RETHOME2
	b    *r11

* handle new timestream event
DOTIMESTR
	li   r15,strDat+64      * timestream curPtr
	bl   *r13               * getCompressedByte
    mov  r2,r2              * check if stream was ended
    jeq  NOTIMESTR  		* skip ahead if it was zero

	movb r1,r9              * make a copy
	andi r1,>F00            * get framesleft
	movb r1,@strDat+71      * save in timestream framesLeft

	li r15,strDat			* start with stream 0, curPtr
	li r0,songNote			* output pointer for songNote
	li r14,>8000			* first command nibble
	mov @workBuf,r7         * get song address
	movb @songActive,r12	* get the songActive mutes

CKTONE1
    sla  r9,1               * test the timestream bit
	jnc  CKTONE2SHIFT       * not set, so skip with r12 shift
	mov  @2(r15),r1         * stream mainPtr
	jeq  CKTONE2SHIFT       * if zero, skip with r12 shift
	bl   *r13               * call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  TONETAB1

	li   r2,>A100           * stream ran out, load mute word
	jmp  WRTONE1            * and go set it up

TONETAB1
*	srl  r1,8               * make note into word
*	sla  r1,1               * multiply by 2 to make index (cant merge the shifts can lsb must be zero)
    srl  r1,7               * this is disgusting, but we get away with it cause of the 15-bit address lines...
    a    @2(r7),r1          * add offset of tone table (word aligned)
	a    r7,r1              * add address of song (word aligned)
    mov  *r1,r2             * get the tone (note: C version does a bunch of masking! We assume tone table has MS nibble zeroed)
    soc  r14,r2				* OR in the command bits

WRTONE1
	mov  r2,*r0+            * save the result in songNote and increment
	sla  r12,1              * check songActive - carry is mute
	joc  CKTONE2            * jump over if muted
	movb r2,*r8             * move command byte to sound chip
	movb @R2LSB,*r8         * move other byte to sound chip
	
CKTONE2
	ai r15,>8				* next curPtr
	ai r14,>2000			* next command nibble
	ci r14,>e000            * did we reach noise?
	jne CKTONE1             * no, do the next one

CKNOISE
    sla  r9,1               * test (technically) >10 (noise)
	jnc  DONETONEACT        * not set, so jump
	mov  @2(r15),r1         * stream 3 mainPtr
	jeq  DONETONEACT        * jump if zero
	bl   *r13               * getCompressedByte
	mov  r2,r2              * check if stream was ended
	jeq  DONETONEACT        * jump if so

    soc  r14,r1				* no tone table here, just OR in the command bits
	sla  r12,1              * check songActive - carry is mute
	joc  NOISEMUTE
	movb r1,*r8             * else just write it to the sound chip

NOISEMUTE
	movb r1,*r0             * save byte only in the songNote, no need to increment

DONETONE
	jmp  DONETONEACT        * go work on the volumes with outSongActive set

CKTONE2SHIFT
	sla r12,1               * shift just to stay in sync
	inct r0					* next SongNote
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
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play in MSB (byte, starts at 0)
    even
StartSong
    li r3,18            * each table is 18 bytes (warning: StopSong uses this - don''t move it!)
    srl r2,8            * make byte in R2 into a word
    mpy r2,r3           * multiply, result in r3/r4 (so r4)
	mov *r1,r3          * get pointer to stream indexes into r3
	a    r4,r3          * add song offset to stream offset to get base stream for this song
	a    r1,r3          * add buf to make it a memory pointer (also word aligned)
	li   r2,strDat+6    * point to the first strDats "curBytes" with r2
STARTLP
    mov *r3+,r4         * get stream offset from table and increment pointer
    jeq  nullptr        * if it was >0000, dont add the base address
	a    r1,r4          * make it a memory pointer
nullptr
	mov  r4,@>FFFC(r2)  * save as mainPtr
	clr  @>FFFA(r2)     * zero curPtr
	clr  *r2            * zero curBytes and framesLeft
	li   r4,getDatZero
	mov  r4,@>FFFE(r2)  * set curType to getDatZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,strDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLP        * loop around if not done

	li   r2,>1          * set all three notes to >0001
	mov  r2,@songNote
	mov  r2,@songNote+2
	mov  r2,@songNote+4

	mov  r1,@workBuf    * store the song pointer in the global
	li   r2,>0101       * init value for noise with songActive bit set
	mov  r2,@songNote+6 * set the noise channel note and songActive bit

setMutes
    li   r2,>9FBF
    mov  r2,@songVol    * mute songVol 0 and 1
    li   r2,>DFFF
    mov  r2,@songVol+2  * mute songVol 2 and 3

	b    *r11

* Call this to stop the current song
	even
StopSong
    movb @StartSong+2,@songActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
    jmp  setMutes                    * make sure the song mutes its voices

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)

	even

	even

	even

	even

    even
* Based on CPlayerTIHandEdit.asm, also required the TISN player
* to be available. This SFX player has the following differences,
* beyond just names:
*
* - no storage of note/volume data (to save memory, usually not needed)
* - StartSfx takes an additional priority value, higher numbers are higher priority
* - when it finishes playing, or is stopped, it attempts to restore
*   the current audio from the main player (if playing)
* - rather than honoring the mutes, it sets them in its own flags.
*   the user application will normally just transfer them over to
*   the main player, unless it has need of further processing.
* - You may NOT clear the "trigger bits" from the songNotes and songVol
*   arrays - the stop code here relies on them being present to be
*   able to more quickly restore the original song
*
* Remain aware of the connection between the third voice and the noise
* channel - if your song or SFX uses custom noises and does not claim
* BOTH channels, you may get some unexpected results. ;)
*
* Also note that a sound effect that pre-empts a lower priority sound
* effect does not restore it - this means that if your new sound effect
* uses different channels, the original channels will remain blocked
* to the main song and MAY be left with a continuing tone until the
* new SFX ends. The best way around this is to make sure your sfx
* all use the same channels. Restoring the voices at the end of a
* sound effect can be rather expensive, so this saves a bit of time
* when the most likely case is that its unneded.
*
* Public Domain


* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.


* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300

* songActive is stored in the LSB of the noise channel
* assembler doesnt like this syntax, so done inline
*songActive EQU songNote+7

* sfxActive is its own word. Its LSB is the current priority

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in sfxSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* As far as the mute bits go, we set them only based on volume output
	even

SfxLoop
    movb @sfxActive,r1      * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME3           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    mov  r11,@sfxSave       * save the return address
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r8,>8400           * address of the sound chip
    li   r6,>0100           * 1 in a byte for byte math

	mov  @sfxDat+66,r1      * timestream mainPtr
	jne  HASTIMESTs         * keep working if we still have a timestream

NOTIMESTs
    clr  r7                 * no timestream - zero outSongActive for volume loop
    jmp  VOLLOOP            * go work on volumes

HASTIMESTs
    sb r6,@sfxDat+71        * decrement the timestream frames left
    jnc  DOTIMESTs          * if it was zero, jump over volumes to get a new byte
DONETONEACu
	seto r7                 * set outSongActive to true for later testing

* volume processing loop
VOLLOOP
	li   r15,sfxDat+32      * stream 4 curPtr (vol[0])  r15
	li   r14,>9000          * command nibble            r14
    li   r0,>8000           * mute bits for this voice  r0
    clr  r12                * actual mute flags         r12
	jmp  VLOOPENe           * jump to bottom of loop

VLOOPDEd
	seto r7                 * set outSongActive to true for later

VLOOPSHIFu
    srl  r0,1               * we always come here to shift in this version
	ai   r15,>8             * next curPtr
	ai   r14,>2000          * next command nibble
    joc  VLOOPDONf          * that was the last one

VLOOPENe
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  VLOOPSHIFu         * if not, execute next loop after shifting r0
    sb r6,@7(r15)           * decrement frames left
    joc VLOOPDEd            * was not yet zero, next loop (will shift r0)

	bl   *r13               * and call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  VNEWVOm            * didnt end, go load it

	li   r1,>F00            * volume stream ended, load fixed volume >0f
	jmp  VLOADVOm           * and go give it to the sound chip

VNEWVOm
	movb r1,r2              * make a copy of the byte
	andi r2,>F00            * extract frame count
	movb r2,@7(r15)         * and save it
	srl  r1,>4              * shift down the volume nibble
	seto r7                 * set outSongActive to true for later

VLOADVOm
    soc  r0,r12             * set the mute bit in r12
	socb r14,r1             * merge in the command bits
	movb r1,*r8             * write to the sound chip
    jmp VLOOPSHIFu          * next loop

VLOOPDONf
    socb r12,@sfxActive     * merge in the mute bits - note we never clear them!
    mov  @sfxSave,r11       * recover return address
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  RETHOMf            * skip if not zero

    li   r0,>f000           * going to relieve the mutes on the main song
    szcb r0,@songNote+7     * no more mutes
    b    @StopSfx           * go restore the songs notes

RETHOMf
    movb @songNote+7,r0     * get main songs activity
    coc r6,r0               * check for activity
    jne RETHOME3            * not active, so never mind
    movb @sfxActive,@songNote+7     * copy our active bits over to its mute bits

RETHOME3
	b    *r11

* handle new timestream event
DOTIMESTs
	li   r15,sfxDat+64      * timestream curPtr
	bl   *r13               * getCompressedByte
    mov  r2,r2              * check if stream was ended
    jeq  NOTIMESTs  		* skip ahead if it was zero

	movb r1,r9              * make a copy
	andi r1,>F00            * get framesleft
	movb r1,@sfxDat+71      * save in timestream framesLeft

	li r15,sfxDat			* start with stream 0, curPtr
	li r14,>8000			* first command nibble
	mov @sfxWorkBuf,r7      * get song address

CKTONE3
    sla  r9,1               * test the timestream bit
	jnc  CKTONE4            * not set, skip
	mov  @2(r15),r1         * stream mainPtr
	jeq  CKTONE4            * if zero, skip 
	bl   *r13               * call getCompressedByte
	mov  r2,r2              * check if stream was ended
	jne  TONETAB2

	li   r2,>A100           * stream ran out, load mute word
	jmp  WRTONE2            * and go set it up

TONETAB2
*	srl  r1,8               * make note into word
*	sla  r1,1               * multiply by 2 to make index (cant merge the shifts can lsb must be zero)
    srl  r1,7               * this is disgusting, but we get away with it cause of the 15-bit address lines...
    a    @2(r7),r1          * add offset of tone table (word aligned)
	a    r7,r1              * add address of song (word aligned)
    mov  *r1,r2             * get the tone (note: C version does a bunch of masking! We assume tone table has MS nibble zeroed)
    soc  r14,r2				* OR in the command bits

WRTONE2
	movb r2,*r8             * move command byte to sound chip
	movb @R2LSB,*r8         * move other byte to sound chip
	
CKTONE4
	ai r15,>8				* next curPtr
	ai r14,>2000			* next command nibble
	ci r14,>e000            * did we reach noise?
	jne CKTONE3             * no, do the next one

CKNOISf
    sla  r9,1               * test (technically) >10 (noise)
	jnc  DONETONEACu        * not set, so jump
	mov  @2(r15),r1         * stream 3 mainPtr
	jeq  DONETONEACu        * jump if zero
	bl   *r13               * getCompressedByte
	mov  r2,r2              * check if stream was ended
	jeq  DONETONEACu        * jump if so

    soc  r14,r1				* no tone table here, just OR in the command bits
	movb r1,*r8             * else just write it to the sound chip
	jmp  DONETONEACu        * go work on the volumes with outSongActive set


* Based on CPlayerTIHandEdit.asm, also required the TISN player
* to be available. This SFX player has the following differences,
* beyond just names:
*
* - no storage of note/volume data (to save memory, usually not needed)
* - StartSfx takes an additional priority value, higher numbers are higher priority
* - when it finishes playing, or is stopped, it attempts to restore
*   the current audio from the main player (if playing)
* - rather than honoring the mutes, it sets them in its own flags.
*   the user application will normally just transfer them over to
*   the main player, unless it has need of further processing.
* - You may NOT clear the "trigger bits" from the songNotes and songVol
*   arrays - the stop code here relies on them being present to be
*   able to more quickly restore the original song
*
* Remain aware of the connection between the third voice and the noise
* channel - if your song or SFX uses custom noises and does not claim
* BOTH channels, you may get some unexpected results. ;)
*
* Also note that a sound effect that pre-empts a lower priority sound
* effect does not restore it - this means that if your new sound effect
* uses different channels, the original channels will remain blocked
* to the main song and MAY be left with a continuing tone until the
* new SFX ends. The best way around this is to make sure your sfx
* all use the same channels. Restoring the voices at the end of a
* sound effect can be rather expensive, so this saves a bit of time
* when the most likely case is that its unneded.
*
* This file contains start, stop and the data, the play function
* itself is in its own file.
*
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.


* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300

* songActive is stored in the LSB of the noise channel
* assembler doesnt like this syntax, so done inline
*songActive EQU songNote+7

* sfxActive is its own word. Its LSB is the current priority

* Call this function to prepare to play
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play in MSB (byte, starts at 0)
* r3 = priority in MSB - higher is more priority
    even
StartSfx
    li r4,>0100         * activity bit (warning: StopSong uses this - dont move it!)
    mov @sfxActive,r5   * get the activity bits
    coc r4,r5           * are we active?
    jne okstart         * no, go ahead
    swpb r5             * get priority byte
    cb r3,r5            * check priority
    jh okstart
    b *r11              * too low priority, ignore

okstart
    movb r3,@sfxActive+1    * store the new priority

    li r3,18            * each table is 18 bytes
    srl r2,8            * make byte in R2 into a word
    mpy r2,r3           * multiply, result in r3/r4 (so r4)
	mov *r1,r3          * get pointer to stream indexes into r3
	a    r4,r3          * add song offset to stream offset to get base stream for this song
	a    r1,r3          * add buf to make it a memory pointer (also word aligned)
	li   r2,sfxDat+6    * point to the first strDats "curBytes" with r2
STARTLq
    mov *r3+,r4         * get stream offset from table and increment pointer
    jeq  nullpts        * if it was >0000, dont add the base address
	a    r1,r4          * make it a memory pointer
nullpts
	mov  r4,@>FFFC(r2)  * save as mainPtr
	clr  @>FFFA(r2)     * zero curPtr
	clr  *r2            * zero curBytes and framesLeft
	li   r4,getDatZero
	mov  r4,@>FFFE(r2)  * set curType to getDatZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,sfxDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLq        * loop around if not done

	mov  r1,@sfxWorkBuf * store the song pointer in the global
	li   r2,>0100       * init value for noise with songActive bit set, no mutes set
	movb r2,@sfxActive  * songActive bit (LSB is priority, already set)
    b    *r11

* Call this to stop the current song
* this is automatically called when an SFX ends
* we always restore from what is in the songNote and songVol registers,
* even if its not playing, so make sure they are initialized properly.
	even
StopSfx
    li r2,>8400                 * sound address
    movb @sfxActive,r1          * its faster to check and restore only what is needed, due to slow SOUND
    sla r1,1                    * check first bit
    jnc stop2

    li r0,songNote              * first note (assumes command bits are still set)
    movb *r0+,*r2               * write first byte
    movb *r0,*r2                * write second byte
    movb @songVol,*r2           * write volume byte

stop2
    sla r1,1                    * check bit
    jnc stop3

    li r0,songNote+2            * note (assumes command bits are still set)
    movb *r0+,*r2               * write first byte
    movb *r0,*r2                * write second byte
    movb @songVol+1,*r2         * write volume byte

stop3
    sla r1,1                    * check bit
    jnc stop4

    li r0,songNote+4            * note (assumes command bits are still set)
    movb *r0+,*r2               * write first byte
    movb *r0,*r2                * write second byte
    movb @songVol+2,*r2         * write volume byte

stop4
    sla r1,1                    * check bit
    jnc stopDone

    movb @songNote+6,*r2        * write noise byte
    movb @songVol+3,*r2         * write volume byte

stopDone
    movb @StartSfx+3,@sfxActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
	b    *r11

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)

	even

    even

	even

    even


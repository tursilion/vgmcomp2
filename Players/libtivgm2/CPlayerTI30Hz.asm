* code that is specific to a single instance of the player
* This is the split 30hz player - you should still call it at
* 60hz, but only 2 channels are processed each call instead
* of all 4. Two bytes of extra RAM is used.
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.
R2LSB EQU >8305

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307
R6LSB EQU >830D

* access the data
    ref strDat,songVol,songNote,workBuf,retSave
	ref	getCompressedByte

* SongActive is stored in the LSB of the noise channel
songActive EQU songNote+7

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section songDatVars
oldTS   bss 2    * saved timestream byte from first half of processing

    even
    pseg

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in retSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* Since this is the 30 hz player, only two channels are processed
* per call (1/2, and 3/4)
	def	SongLoop30
	even

* bits for songActive - MSB assumed
BITS01 DATA >0100
BITS02 DATA >0200

SongLoop30
    movb @songActive,r1     * need to check if its active
    coc @BITS01,r1          * isolate the bit
	jne  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    mov  r11,@retSave       * save the return address
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r8,>8400           * address of the sound chip (warning: don''t move too much, my sample code relies on this offset)
    li   r6,>0100           * 1 in a byte for byte math

	mov  @strDat+66,r2      * timestream mainPtr
	jne  HASTIMESTR         * keep working if we still have a timestream

NOTIMESTR
    clr  r7                 * no timestream - zero outSongActive for volume loop
    coc @BITS02,r1          * are we on the second half?
    jne DONETONEFIRST       * no
    jmp DONETONESEC

HASTIMESTR
    seto r7                 * assume we are still playing
    coc @BITS02,r1          * are we on the second half?
    jne FIRSTHALF           * no
    mov @oldTS,r9           * yes, get back the timestream flags
	li r15,strDat+16		* start with stream 2, curPtr
	li r0,songNote+4		* output pointer for songNote[2]
	li r14,>C000			* third command nibble
	mov @workBuf,r7         * get song address
	movb @songActive,r12	* get the songActive mutes
    sla r12,2               * skip first two
    jmp CKTONE1             * and go process it

FIRSTHALF
    sb r6,@strDat+71        * decrement the timestream frames left
    jnc  DOTIMESTR          * if it was zero, jump over volumes to get a new byte

* volume processing loop
DONETONEFIRST
    socb @bits02,@songActive    * set the bitflag for half
	li   r15,strDat+32      * stream 4 curPtr (vol[0])  r15
	li   r0,songVol         * songVol table pointer     r0
	li   r14,>9000          * command nibble            r14
    movb @songActive,r12    * actual mute flags         r12
	jmp  VLOOPEND           * jump to bottom of loop

DONETONESEC
    szcb @bits02,@songActive    * clear the bitflag for half
	li   r15,strDat+48      * stream 4 curPtr (vol[2])  r15
	li   r0,songVol+2       * songVol table pointer     r0
	li   r14,>d000          * command nibble            r14
    movb @songActive,r12    * actual mute flags         r12
    sla  r12,2              * skip the first two
	jmp  VLOOPEND           * jump to bottom of loop

VLOOPDEC
	seto r7                 * set outSongActive to true for later

VLOOPSHIFT
    sla r12,1               * if we come here, we still needed to shift
	inc  r0                 * next songVol pointer

VLOOPNEXT
	ai   r15,>8             * next curPtr
	ai   r14,>2000          * next command nibble
    joc  VLOOPDONE          * that was the last one (wrap around after >F000)
    ci   r14,>d000          * check if we finished the first two
    jeq  VLOOPDONE          * end if so

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
    movb @R6LSB,@songActive	 * turn off the active bit and the mutes (this BYTE writes a >00)

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
	jeq CKNOISE             * yes, go do noise
    ci r14,>c000            * did we reach the end of the first pass?
    jne CKTONE1             * no, loop around

    mov r9,@oldTS           * save the timestream status for next call
    jmp DONETONEFIRST       * go work on volumes, first half

CKNOISE
    sla  r9,1               * test (technically) >10 (noise)
	jnc  DONETONESEC        * not set, so jump
	mov  @2(r15),r1         * stream 3 mainPtr
	jeq  DONETONESEC        * jump if zero
	bl   *r13               * getCompressedByte
	mov  r2,r2              * check if stream was ended
	jeq  DONETONESEC        * jump if so

    soc  r14,r1				* no tone table here, just OR in the command bits
	sla  r12,1              * check songActive - carry is mute
	joc  NOISEMUTE
	movb r1,*r8             * else just write it to the sound chip

NOISEMUTE
	movb r1,*r0             * save byte only in the songNote, no need to increment

DONETONE
	jmp  DONETONESEC        * go work on the volumes with outSongActive set

CKTONE2SHIFT
	sla r12,1               * shift just to stay in sync
	inct r0					* next SongNote
	jmp CKTONE2

	.size	SongLoop,.-SongLoop


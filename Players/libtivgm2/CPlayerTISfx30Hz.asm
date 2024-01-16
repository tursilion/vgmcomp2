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

    ref songNote,songVol,StopSfx
    ref sfxDat,sfxActive,sfxWorkBuf,sfxSave
	ref	getCompressedByte,sfxstackSave

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.
R2LSB EQU >8305

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307

* songActive is stored in the LSB of the noise channel
songActive EQU songNote+7

* sfxActive is its own word. Its LSB is the current priority

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section sfxDatVars
oldSfxTS   bss 2    * saved timestream byte from first half of processing

    even
    pseg

* this needs to be called 60 times per second by your system
* it will only process 2 channels per call
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in sfxSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* As far as the mute bits go, we set them only based on volume output
* Since this is the 30 hz player, only two channels are processed
* per call (1/2, and 3/4)
    def	SfxLoop30,Sfx2Lp30
	even

* bits for sfxActive - MSB assumed
BITS01 DATA >0100
BITS02 DATA >0200

SfxLoop30
    li   r8,>8400           * address of the sound chip
Sfx2Lp30    
    movb @sfxActive,r1      * need to check if its active
    coc @BITS01,r1          * isolate the bit
	jne  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    mov  r11,@sfxSave       * save the return address
    mov  r10,@sfxstackSave  * new stack
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r6,>0100           * 1 in a byte for byte math

	mov  @sfxDat+66,r2      * timestream mainPtr
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
    mov @oldSfxTS,r9        * yes, get back the timestream flags
	li r15,sfxDat+16		* start with stream 2, curPtr
	li r14,>C000			* third command nibble
	mov @sfxWorkBuf,r7      * get song address
    jmp CKTONE1             * and go process it

FIRSTHALF
    sb r6,@sfxDat+71        * decrement the timestream frames left
    jnc  DOTIMESTR          * if it was zero, jump over volumes to get a new byte

* volume processing loop
DONETONEFIRST
    socb @bits02,@songActive    * set the bitflag for half
	li   r15,sfxDat+32      * stream 4 curPtr (vol[0])  r15
	li   r14,>9000          * command nibble            r14
    li   r0,>8000           * mute bits for this voice  r0
    clr  r12                * actual mute flags         r12
	jmp  VLOOPEND           * jump to bottom of loop

DONETONESEC
    szcb @bits02,@songActive    * clear the bitflag for half
	li   r15,sfxDat+48      * stream 4 curPtr (vol[2])  r15
	li   r14,>d000          * command nibble            r14
    li   r0,>2000           * mute bits for this voice  r0
    clr  r12                * actual mute flags         r12
	jmp  VLOOPEND           * jump to bottom of loop

VLOOPDEC
	seto r7                 * set outSongActive to true for later

VLOOPSHIFT
    srl  r0,1               * we always come here to shift in this version
	ai   r15,>8             * next curPtr
	ai   r14,>2000          * next command nibble
    joc  VLOOPDONE          * that was the last one (wrap around after >F000)
    ci   r14,>d000          * check if finished the first two
    jeq  VLOOPDONE          * end if so

VLOOPEND
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  VLOOPSHIFT         * if not, execute next loop after shifting r0
    sb r6,@7(r15)           * decrement frames left
    joc VLOOPDEC            * was not yet zero, next loop (will shift r0)

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
    soc  r0,r12             * set the mute bit in r12
	socb r14,r1             * merge in the command bits
	movb r1,*r8             * write to the sound chip
    jmp VLOOPSHIFT          * next loop

VLOOPDONE
    socb r12,@sfxActive     * merge in the mute bits - note we never clear them!
    mov  @sfxSave,r11       * recover return address
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  RETHOME            * skip if not zero

    li   r0,>f000           * going to relieve the mutes on the main song
    szcb r0,@songActive     * no more mutes
    b    @StopSfx           * go restore the songs notes

RETHOME
    movb @songActive,r0     * get main songs activity
    coc r6,r0               * check for activity
    jne RETHOME2            * not active, so never mind
    movb @sfxActive,@songActive     * copy our active bits over to its mute bits

RETHOME2
    mov  @sfxstackSave,R10  * new stack
	b    *r11

* handle new timestream event
DOTIMESTR
	li   r15,sfxDat+64      * timestream curPtr
	bl   *r13               * getCompressedByte
    mov  r2,r2              * check if stream was ended
    jeq  NOTIMESTR  		* skip ahead if it was zero

	movb r1,r9              * make a copy
	andi r1,>F00            * get framesleft
	movb r1,@sfxDat+71      * save in timestream framesLeft

	li r15,sfxDat			* start with stream 0, curPtr
	li r14,>8000			* first command nibble
	mov @sfxWorkBuf,r7      * get song address

CKTONE1
    sla  r9,1               * test the timestream bit
	jnc  CKTONE2            * not set, skip
	mov  @2(r15),r1         * stream mainPtr
	jeq  CKTONE2            * if zero, skip 
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
	movb r2,*r8             * move command byte to sound chip
	movb @R2LSB,*r8         * move other byte to sound chip
	
CKTONE2
	ai r15,>8				* next curPtr
	ai r14,>2000			* next command nibble
	ci r14,>e000            * did we reach noise?
	jeq CKNOISE             * yes, go do noise
    ci r14,>c000            * did we reach the end of the first pass?
    jne CKTONE1             * no, loop around

    mov r9,@oldSfxTS        * save the timestream status
    jmp DONETONEFIRST

CKNOISE
    sla  r9,1               * test (technically) >10 (noise)
	jnc  DONETONESEC        * not set, so jump
	mov  @2(r15),r1         * stream 3 mainPtr
	jeq  DONETONESEC        * jump if zero
	bl   *r13               * getCompressedByte
	mov  r2,r2              * check if stream was ended
	jeq  DONETONESEC        * jump if so

    soc  r14,r1				* no tone table here, just OR in the command bits
	movb r1,*r8             * else just write it to the sound chip
	jmp  DONETONESEC        * go work on the volumes with outSongActive set

	.size	SfxLoop,.-SfxLoop


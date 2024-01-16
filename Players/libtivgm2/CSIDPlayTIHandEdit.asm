* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi, with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.
R2LSB EQU >8305

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307

    pseg

* SongActive is stored in the LSB of the noise channel
songActive EQU sidNote+7

    ref getCompressedByte
    ref sidDat,sidVol,sidNote,workSID
    ref sidSave,SidCtrl1,SidCtrl2,SidCtrl3
    ref sidstackSave

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in sidSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* NOTE: this will enable the SID Blaster, do NOT call with any DSR enabled!
* it also changes the keyboard select column
	def	SIDLoop
	even

SIDLoop
    movb @songActive,r1     * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    clr  r12                * prepare to write CRU
    sbo  >24                * write keyboard select - this maps in the SID Blaster
    mov  r11,@sidSave       * save the return address
    mov  r10,@sidstackSave  * new stack
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
    mov  @sidstackSave,r10  * new stack
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

	.size	SIDLoop,.-SIDLoop

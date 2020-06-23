* TODO:
* - study Silius title - its almost 2k smaller on the old one. Why? Where is the gain?
*   If we can merge that gain with this tool, then well have it nailed.
* - Mutes: this player (music) will /check/ the bits for mute. SFX will /set/ the bits for mute.
*   Theres no need for the SFX player to check mutes and no need for this one to set them.

* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi, with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* Public Domain

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes a workspace of >8300 and that it can pretty much completely
* wipe it out. If you need to preserve your own registers, use a different workspace.
R2LSB EQU >8305

    pseg

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307
R6LSB EQU >830D

* SongActive is stored in the LSB of the noise channel
songActive EQU sidNote+7

* Call this function to prepare to play
* enables the SID - do not call with any DSRs active
* it also changes the keyboard select column
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play in MSB (byte, starts at 0)
	def	StartSID
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
	li   r4,getSidZero
	mov  r4,@>FFFE(r2)  * set curType to getSidZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,sidDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLP        * loop around if not done

	li   r2,>1          * set all three notes to >0001
	mov  r2,@sidNote
	mov  r2,@sidNote+2
	mov  r2,@sidNote+4
    clr @sidVol        * zero sidVol 0 and 1
    clr @sidVol+2      * zero sidVol 2 and 3

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

	b    *r11
	.size	StartSID,.-StartSID

* Call this to stop the current song
	def	StopSID
	even
StopSID
    movb @StartSID+2,@songActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
	b    *r11
	.size	StopSID,.-StopSID

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in sidSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
* NOTE: this will enable the SID Blaster, do NOT call with any DSR enabled!
* it also changes the keyboard select column
	def	SongSID
	even

SongSID
    movb @songActive,r1     * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    clr  r12                * prepare to write CRU
    sbo  >24                * write keyboard select - this maps in the SID Blaster
    mov  r11,@sidSave       * save the return address
	li   r13,getCompressedSid  * store address of getCompressedSid
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

	bl   *r13               * and call getCompressedSid
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
    movb @R6LSB,@songActive	 * turn off the active bit and the mutes (this BYTE writes a >00)

RETHOME
    mov  @sidSave,r11       * back to caller
RETHOME2
	b    *r11

* handle new timestream event
DOTIMESTR
	li   r15,sidDat+64      * timestream curPtr
	bl   *r13               * getCompressedSid
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
	bl   *r13               * call getCompressedSid
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

	.size	SongSID,.-SongSID

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section songDat

	even
    def sidDat
sidDat
	bss 72

	even
	def sidVol
* warning: sidVol is implemented in reverse order - 3,2,1,x
sidVol
	bss 4

	even
	def sidNote
sidNote
	bss 8

	even
	def workSID
workSID
	bss 2

    even
    def sidSave
sidSave
    bss 2

    even
    def SidCtrl1,SidCtrl2,SidCtrl3
* defined in reverse order on purpose so the volume loop can count down
SidCtrl3  bss 1
SidCtrl2  bss 1
SidCtrl1  bss 1
    even

* we only need a special getCompressedSid if we have to use
* a different song back. If, someday, we can share the same
* songbank, just sharing workBuf (instead of workSID) is all
* need to share the common code with the SN player.
	ref	getCompressedSid

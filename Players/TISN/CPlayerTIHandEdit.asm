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
songActive EQU songNote+7

* Call this function to prepare to play
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play (starts at 0)
	def	StartSong
    even
StartSong
	mov *r1,r3          * get pointer to stream indexes into r3

	srl  r2,8           * make word of song index byte
	mov  r2,r4          * make copy
	a    r2,r4          * *2 at r4
	sla  r2,>4          * *16 at r2
	a    r2,r4          * add to get *18 in r4
	a    r4,r3          * add to stream offset to get base stream for this song
	a    r1,r3          * add buf to make it a memory pointer (also word aligned)
	li   r2,strDat+6    * point to the first strDats "curBytes" with r2
STARTLP
    mov *r3+,r4         * get stream offset from table and increment pointer
	a    r1,r4          * make it a memory pointer
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
    clr @songVol        * zero songVol 0 and 1
    clr @songVol+2      * zero songVol 2 and 3

	mov  r1,@workBuf    * store the song pointer in the global
	li   r2,>0101       * init value for noise with songActive bit set
	mov  r2,@songNote+6 * set the noise channel note and bit
	b    *r11
	.size	StartSong,.-StartSong

* Call this to stop the current song
	def	StopSong
	even
StopSong
    movb @R6LSB,@songActive	 * zero all bits in songActive
	b    *r11
	.size	StopSong,.-StopSong

* this needs to be called 60 times per second by your system
* if this function can be interrupted, dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in retSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do preserve R10, the C stack.)
	def	SongLoop
	even

SongLoop
    movb @songActive,r1     * need to check if its active
    andi r1,>0100           * isolate the bit
	jeq  RETHOME2           * if clear, back to caller (normal case drops through faster)

* load some default values for the whole call
    mov  r11,@retSave       * save the return address
	li   r13,getCompressedByte  * store address of getCompressedByte
    li   r8,>8400           * address of the sound chip
    li   r6,>0100           * 1 in a byte for byte math

	mov  @strDat+66,r1      * timestream mainPtr
	jne  HASTIMESTR         * keep working if we still have a timestream

NOTIMESTR
    clr  r7                 * no timestream - zero outSongActive for volume loop
    jmp  VOLLOOP            * go work on volumes

HASTIMESTR
    sb r6,@strDat+71        * decrement the timestream frames left
    jnc  DOTIMESTR          * if it was zero, jump over volumes to get a new byte
DONETONEACT
	seto r7                 * set outSongActive to true for later testing

* volume processing loop
VOLLOOP
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

	.size	SongLoop,.-SongLoop

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section songDat

	even
    def strDat
strDat
	bss 72

	even
	def songVol
songVol
	bss 4

	even
	def songNote
songNote
	bss 8

	even
	def workBuf
workBuf
	bss 2

    even
    def retSave
retSave
    bss 2

	ref	getCompressedByte

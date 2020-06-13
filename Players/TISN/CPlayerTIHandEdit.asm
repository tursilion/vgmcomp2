* TODO:
* - see if we can put the timestream/voices into a loop to save some code space, should be easier now
* - can we use one mute test to include the tone and volume writes both?
* - study Silius title - its almost 2k smaller on the old one. Why? Where is the gain?
*   If we can merge that gain with this tool, then well have it nailed.
* - Mutes: this player (music) will /check/ the bits for mute. SFX will /set/ the bits for mute.
*   Theres no need for the SFX player to check mutes and no need for this one to set them.

* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi, with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* Public Domain

    pseg

    ref DAT0001
DATFFFE data 0xfffe

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307
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
    movb @DAT0001,@songActive	* zero all bits in songActive
	b    *r11
	.size	StopSong,.-StopSong

* called by SongLoop to look up note frequencies
* R1 MSB has note index, R2 gets return
ToneTable
	mov  @workBuf,r12       * get address of song
	srl  r1,8               * make note into word
	a    r1,r1              * multiply by 2 to make index
    mov  @2(r12),r2         * get offset of tone table (word aligned)
	a    r2,r1              * add offset of tone (word aligned)
	a    r12,r1             * add address of song (word aligned)
    mov  *r1,r2             * get the tone (note: C version does a bunch of masking! We assume tone table has MS nibble zeroed)
    b    *r11

* this needs to be called 60 times per second by your system
* if this function can be interrupted,dont manipulate songActive
* in any function that can do so, or your change will be lost.
* to be called with BL so return on R11, which we save in retSave.
* By replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry, we can do away with
* the stack usage completely. (We do honor R10, the C stack.)
	def	SongLoop
	even

SongLoop
    mov  r11,@retSave       * save the return address
	li   r13,getCompressedByte  * new timestream byte - store address of getCompressedByte
    li   r8,>8400           * address of the sound chip

	movb @songActive,r1     * get songActive byte into r1 MSB (Noise LSB: 1234 xxxA  - 1-4 mutes, A=active)
    szcb @DATFFFE+1,r1      * zero everything but the active bit
	jeq  RETHOME            * if clear, back to caller (normal case drops through faster)
	mov  @strDat+66,r1      * timestream mainPtr
	jne  HASTIMESTR         * keep working if we still have a timestream

NOTIMESTR
    clr  r7                 * no timestream - zero outSongActive for volume loop
    jmp  VOLLOOP            * go work on volumes

HASTIMESTR
	movb @strDat+71,r1      * get timestream frames left byte
	jeq  DOTIMESTR          * if its zero, jump over volumes to get a new byte
	ab @DATFFFE,@strDat+71  * otherwise, subtract one from frames left
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
    sb @DAT0001+1,@7(r15)   * decrement frames left
	seto r7                 * set outSongActive to true for later

VLOOPSHIFT
    sla r12,1               * if we come here, we still needed to shift

VLOOPNEXT
	inc  r0                 * next songVol pointer
	ai   r15,>8             * next curPtr
	ai   r14,>2000          * next command nibble
    joc  VLOOPDONE          * that was the last one

VLOOPEND
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  VLOOPSHIFT         * if not, execute next loop after shifting r12
	movb @7(r15),r1         * check framesLeft
	jne  VLOOPDEC           * if not zero, go decrement it, then next loop (will shift r12)

	mov  r15,r1             * get curPtr into r1
	bl   *r13               * and call getCompressedByte
	mov  @2(r15),r2         * check mainPtr again (might have been zeroed!)
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
	movb r1,*r0             * write the byte to songVol as well
    jmp VLOOPNEXT           * next loop

VLOOPDONE
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  RETHOME            * skip if not zero
    movb @DAT0001,@songActive	* turn off the active bit and the mutes

RETHOME
    mov  @retSave,r11       * back to caller
	b    *r11

* handle new timestream event
DOTIMESTR
	li   r1,strDat+64       * timestream curPtr
	bl   *r13               * getCompressedByte
	movb r1,r9              * make a copy
	mov  @strDat+66,r1      * check timestream mainptr to see if it was zeroed
    jeq  NOTIMESTR  		* skip ahead if it was zero

	movb r9,r12             * data byte in R12
	andi r12,>F00           * get framesleft
	movb r12,@strDat+71     * save in timestream framesLeft
    sla  r9,1               * test msb
	joc  DOTONE0			* otherwise was set, go process 0x80

CKTONE1
    sla  r9,1               * test (technically) >40
	jnc  CKTONE2            * not set, so skip
	mov  @strDat+10,r1      * stream 1 mainPtr
	jeq  CKTONE2            * if zero, skip
	li   r1,strDat+8        * load stream 1 curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+10,r2      * check stream 1 mainPtr is still set
	jne  TONETAB1

	li   r2,>A100           * stream 1 ran out, load mute word
	jmp  WRTONE1            * and go set it up

TONETAB1
    bl   @ToneTable         * look up tone in r1
	ori  r2,>A000           * OR in the command bits

WRTONE1
	mov  r2,@songNote+2     * save the result in songNote
	movb @songActive,r1     * get songActive
	andi r1,>4000           * check for mute
	jne  CKTONE2            * jump ahead if muted
	movb r2,*r8             * move command byte to sound chip
	swpb r2                 * other byte
	movb r2,*r8             * move other byte to sound chip

CKTONE2
    sla  r9,1               * test (technically) >20
	jnc  CKNOISE            * not set, so skip
	mov  @strDat+18,r1      * stream 2 mainptr
	jeq  CKNOISE            * if zero, skip
	li   r1,strDat+16       * stream 2 curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+18,r2      * check if mainPtr still set
	jne  TONETAB2

	li   r2,>C100           * stream 2 ran out, load mute word
	jmp  WRTONE2            * and go set it up

TONETAB2
    bl   @ToneTable         * look up frequency in r1
	ori  r2,>C000           * OR in the command bits

WRTONE2
	mov  r2,@songNote+4     * save the result in songNote
	movb @songActive,r1     * get songActive
	andi r1,>2000           * test for mute
	jne  CKNOISE            * jump if muted
	movb r2,*r8             * MSB
	swpb r2                 * swap
	movb r2,*r8             * LSB

CKNOISE
    sla  r9,1               * test (technically) >10 (noise)
	jnc  DONETONE           * not set, so jump
	mov  @strDat+26,r1      * stream 3 mainPtr
	jeq  DONETONE           * jump if zero
	li   r1,strDat+24       * stream 3 curPtr
	bl   *r13               * getCompressedByte
	mov  @strDat+26,r2      * check if mainPtr was zeroed
	jeq  DONETONE           * jump if so

	ori  r1,>E000           * no tone table here, just OR in the command bits
	movb @songActive,r2     * get songActive
	andi r2,>1000           * test bit
	jne  NOISEMUTE          * jump if muted
	movb r1,*r8             * else just write it to the sound chip

NOISEMUTE
	movb r1,@songNote+6     * save byte only in the songNote

DONETONE
	b    @DONETONEACT       * go work on the volumes with outSongActive set

DOTONE0
	mov  @strDat+2,r1       * timestream voice 0 - get mainPtr
	jeq CKTONE1             * skip back to channel 1 if zero

	li   r1,strDat          * get curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+2,r2       * check mainPtr again
	jne  TONETAB0           * not zeroed, jump down and load it

	li   r2,>8100           * stream 0 ran out, load mute word
	jmp  WRTONE0            * and go set it up

TONETAB0
    bl   @ToneTable         * lookup frequency in R1
	ori  r2,>8000           * OR in command bits

WRTONE0
	mov  r2,@songNote       * store value in songNote
	movb @songActive,r1     * get songActive
	andi r1,>8000           * check mute
	jne CKTONE1             * skip back to channel 1 if muted (no more to do here)

	movb r2,*r8             * MSB
	swpb r2                 * swap
	movb r2,*r8             * LSB
	b    @CKTONE1           * go work on channel 1

	.size	SongLoop,.-SongLoop

	dseg

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

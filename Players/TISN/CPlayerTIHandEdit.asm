* TODO:
* - see if we can put the timestream/voices into a loop to save some code space, should be easier now
* - convert Silius title into a SN VGM so we can run it through the old compressor and get real stats (or choose a new title)


* code that is specific to a single instance of the player
* Hand edit of CPlayerTI.c assembly by Tursi,with SN mode
* and mute enabled in songActive. Due to hard coded addresses,
* you need a separate build for sfx (CPlayerCommon is shared)
* Public Domain
    ref DAT0001
DATFFFE data 0xfffe

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307

* note usage of songActive differs some from the C version,its 16 bits
* but active is the least significant bit, and the mutes are in the
* most significant byte

	pseg

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
L4
    mov *r3+,r4         * get stream offset from table and increment pointer
	a    r1,r4          * make it a memory pointer
	mov  r4,@>FFFC(r2)  * save as mainPtr
	clr  @>FFFA(r2)     * zero curPtr
	clr  *r2            * zero curBytes and framesLeft
	li   r4,getDatZero
	mov  r4,@>FFFE(r2)  * set curType to getDatZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,strDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  L4             * loop around if not done

	li   r2,>1          * set all four notes to >0001
	mov  r2,@songNote
	mov  r2,@songNote+2
	mov  r2,@songNote+4
	mov  r2,@songNote+6
    clr @songVol        * zero songVol 0 and 1
    clr @songVol+2      * zero songVol 2 and 3

	mov  r1,@workBuf    * store the song pointer in the global
	mov  r2,@songActive  * set the songActive bit (r2 is still >0001 from above)
	b    *r11
	.size	StartSong,.-StartSong

* Call this to stop the current song
	def	StopSong
	even
StopSong
    clr  @songActive    * zero all bits in songActive
	b    *r11
	.size	StopSong,.-StopSong

* called by SongLoop to look up note frequencies
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
* in any function that can do so,or your change will be lost.
* to be called with BL so return on R11, which we save in retSave
* by replacing GCC regs 3-6 with 12,0,7,8, and knowing that we dont need to
* preserve or restore ANY registers on entry,we can do away with
* the stack usage completely
	def	SongLoop
	even
JMP_0
    mov @retSave,r11        * recover return address
    b *r11                  * back to caller
JMP_1
    b @L12  * TODO: if we need it!

SongLoop
    mov  r11,@retSave       * save the return address

	mov  @songActive,r1     * get songActive
    szc  @DATFFFE,r1        * zero everything but the active bit
	jeq  JMP_0              * if clear, back to caller (normal case drops through faster)
	mov  @strDat+66,r1      * timestream mainPtr
	jeq  JMP_1              * skip to volumes if its empty
	movb @strDat+71,r1      * get timestream frames left byte
	jeq  L13                * if its zero, get a new byte (use JMP_2 label if we cant reach)
	ab @DATFFFE,@strDat+71  * subtract one from frames left
	seto r7                 * set outSongActive to true for later testing

* volume processing loop
L14
	li   r15,strDat+32      * stream 4 curPtr (vol[0])  r15
	li   r0,songVol         * songVol table pointer     r0
	li   r12,muteMap        * first entry in muteMap    r12
	li   r14,>9000          * command nibble            r14
	li   r8,>F000           * value for last one        r8
	jmp  L32                * jump to bottom of loop
L34
    sb @DAT0001+1,@7(r15)   * decrement frames left
	seto r7                 * set outSongActive to true for later
L26
	inc  r0                 * next songVol pointer
	inc  r12                * next muteMap
	ai   r15,>8             * next curPtr
	cb   r14,r8             * was this the last command?
	jeq  L31                * if yes, jump out of loop

L36
	ai   r14,>2000          * next command nibble
L32
	mov  @2(r15),r1         * check if mainPtr is valid
	jeq  L26                * if not, execute next loop
	movb @7(r15),r1         * check framesLeft
	jne  L34                * if not zero, go decrement it, then next loop

	mov  r15,r1             * get curPtr into r1
	bl   @getCompressedByte  * and call getCompressedByte
	mov  @2(r15),r2         * check mainPtr again (might have been zeroed!)
	jne  L35                * didnt end, go load it

	li   r1,>F00            * volume stream ended, load fixed volume >0f
	jmp  L29                * and go give it to the sound chip

L35
	movb r1,r2              * make a copy of the byte
	andi r2,>F00            * extract frame count
	movb r2,@7(r15)         * and save it
	srl  r1,>4              * shift down the volume nibble
	seto r7                 * set outSongActive to true for later

L29
	socb r14,r1             * merge in the command bits

	mov  @songActive,r2     * get songActive
	szcb *r12,r2            * mask off the zeros (mutes are in the MSB)
	jne  L30                * if the bit was set, skip the write (we still want the rest!)
	movb r1,@>8400          * write to the sound chip

L30
	movb r1,*r0             * write the byte to songVol as well
    jmp L26                 * next loop

L31
    mov  r7,r7              * end of loop - check if outSongActive was set
    jne  L33                * skip if not zero
    clr  @songActive        * turn off the active bit and the mutes

L33
    mov  @retSave,r11       * back to caller
	b    *r11

* handle new timestream event
L13
	li   r13,getCompressedByte  * new timestream byte - store address of getCompressedByte
	li   r1,strDat+64       * timestream curPtr
	bl   *r13               * getCompressedByte
	movb r1,r9              * make a copy
	mov  @strDat+66,r1      * check timestream mainptr to see if it was zeroed
	jne  JMP_3
	b    @L12               * skip ahead if it was zero
JMP_3
	movb r9,r12             * data byte in R12
	andi r12,>F00           * get framesleft
	movb r12,@strDat+71     * save in timestream framesLeft
    sla  r9,1               * test msb
    jnc JMP_4               * not set, skip
	b    @L37               * otherwise was set, go process 0x80
JMP_4
L15
    sla  r9,1               * test (technically) >40
	jnc  L18                * not set, so skip
	mov  @strDat+10,r1      * stream 1 mainPtr
	jeq  L18                * if zero, skip
	li   r1,strDat+8        * load stream 1 curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+10,r2      * check stream 1 mainPtr is still set
	jne  JMP_5

	li   r2,>A100           * stream 1 ran out, load mute word
	jmp  L20                * and go set it up

JMP_5
    bl   @ToneTable         * look up tone in r1
	ori  r2,>A000           * OR in the command bits

L20
	mov  r2,@songNote+2     * save the result in songNote
	mov  @songActive,r1     * get songActive
	andi r1,>4000           * check for mute
	jne  L18                * jump ahead if muted
	movb r2,@>8400          * move command byte to sound chip
	swpb r2                 * other byte
	movb r2,@>8400          * move other byte to sound chip

L18
    sla  r9,1               * test (technically) >20
	jnc  L21                * not set, so skip
	mov  @strDat+18,r1      * stream 2 mainptr
	jeq  L21                * if zero, skip
	li   r1,strDat+16       * stream 2 curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+18,r2      * check if mainPtr still set
	jne  JMP_6

	li   r2,>C100           * stream 2 ran out, load mute word
	jmp  L23                * and go set it up

JMP_6
    bl   @ToneTable         * look up frequency in r1
	ori  r2,>C000           * OR in the command bits

L23
	mov  r2,@songNote+4     * save the result in songNote
	mov  @songActive,r1     * get songActive
	andi r1,>2000           * test for mute
	jne  L21                * jump if muted
	movb r2,@>8400          * MSB
	swpb r2                 * swap
	movb r2,@>8400          * LSB

L21
    sla  r9,1               * test (technically) >10 (noise)
	jnc  L24                * not set, so jump
	mov  @strDat+26,r1      * stream 3 mainPtr
	jeq  L24                * jump if zero
	li   r1,strDat+24       * stream 3 curPtr
	bl   *r13               * getCompressedByte
	mov  @strDat+26,r2      * check if mainPtr was zeroed
	jeq  L24                * jump if so

	ori  r1,>E000           * no tone table here, just OR in the command bits
	mov  @songActive,r2     * get songActive for mutes
	andi r2,>1000           * test bit
	jne  L25                * jump if muted
	movb r1,@>8400          * else just write it to the sound chip

L25
*	srl  r1,8               * make byte a word (waste of time.... its just for stats, user can deal with the MSB)
	mov  r1,@songNote+6     * save it in the songNote
L24
	seto r7                 * flag outSongActive as true
	b    @L14               * go work on the volumes

L12
	clr  r7                 * no timestream work this frame - zero outSongActive in case volume doesnt set it
	b    @L14               * and go work on them

L37
	mov  @strDat+2,r1       * timestream voice 0 - get mainPtr
	jne  JMP_7
	b    @L15               * skip back to channel 2 if zero

JMP_7
	li   r1,strDat          * get curPtr
	bl   *r13               * call getCompressedByte
	mov  @strDat+2,r2       * check mainPtr again
	jne  L16                * not zeroed, jump down and load it

	li   r2,>8100           * stream 0 ran out, load mute word
	jmp  L17                * and go set it up

L16
    bl   @ToneTable         * lookup frequency in R1
	ori  r2,>8000           * OR in command bits

L17
	mov  r2,@songNote       * store value in songNote
	mov  @songActive,r1     * get songActive for mutes
	andi r1,>8000           * check mute
	jeq  JMP_8
	b    @L15               * skip back to channel 2 if muted (no more to do here)

JMP_8
	movb r2,@>8400          * MSB
	swpb r2                 * swap
	movb r2,@>8400          * LSB
	b    @L15               * go work on channel 2

	.size	SongLoop,.-SongLoop

	def	muteMap
	.type	muteMap,@object
	.size	muteMap,4
* Since we need to use szcb, we store these values inverted
muteMap
	byte	0x7f
	byte	0xbf
	byte	0xdf
	byte	0xef
	cseg

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
	def songActive
songActive
	bss 2

	even
	def workBuf
workBuf
	bss 2

    even
    def retSave
retSave
    bss 2

	ref	getCompressedByte

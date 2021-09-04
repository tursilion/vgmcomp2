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
R2LSB EQU >8305

    ref songNote,songVol
    pseg

* we sometimes need to directly access the LSB of some registers - addresses here
* Note this assumes that this code uses a workspace of >8300
R3LSB EQU >8307

* songActive is stored in the LSB of the noise channel
* assembler doesnt like this syntax, so done inline
*songActive EQU songNote+7

* sfxActive is its own word. Its LSB is the current priority

* Call this function to prepare to play
* r1 = pSbf - pointer to song block data (must be word aligned)
* r2 = songNum - which song to play in MSB (byte, starts at 0)
* r3 = priority in MSB - higher is more priority
	def	StartSfx
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
	ci   r2,sfxDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLP        * loop around if not done

	mov  r1,@sfxWorkBuf * store the song pointer in the global
	li   r2,>0100       * init value for noise with songActive bit set, no mutes set
	movb r2,@sfxActive  * songActive bit (LSB is priority, already set)
    b    *r11
	.size	StartSfx,.-StartSfx

* Call this to stop the current song
* this is automatically called when an SFX ends
* we always restore from what is in the songNote and songVol registers,
* even if its not playing, so make sure they are initialized properly.
	def	StopSfx
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
	.size	StopSfx,.-StopSfx

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section sfxDatVars

	even
    def sfxDat
sfxDat
	bss 72

    even
    def sfxActive
sfxActive
    bss 2

	even
	def sfxWorkBuf
sfxWorkBuf
	bss 2

    even
    def sfxSave
sfxSave
    bss 2

	ref	getCompressedByte

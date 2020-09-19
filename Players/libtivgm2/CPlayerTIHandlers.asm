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
* r2 = songNum - which song to play in MSB (byte, starts at 0)
	def	StartSong
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
	.size	StartSong,.-StartSong

* Call this to stop the current song
	def	StopSong
	even
StopSong
    movb @StartSong+2,@songActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
    jmp  setMutes                    * make sure the song mutes its voices
	.size	StopSong,.-StopSong

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section songDatVars

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

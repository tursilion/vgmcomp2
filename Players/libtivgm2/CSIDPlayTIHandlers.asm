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
	li   r4,getDatZero
	mov  r4,@>FFFE(r2)  * set curType to getSidZero (just a safety move)

	ai   r2,>8          * next structure
	ci   r2,sidDat+78   * check if we did the last (9th) one (9*8=72,+6 offset = 78. I dont know why GCC used an offset,but no biggie)
	jne  STARTLP        * loop around if not done

	li   r2,>1          * set all three notes to >0001
	mov  r2,@sidNote
	mov  r2,@sidNote+2
	mov  r2,@sidNote+4

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

setMutes
    clr @sidVol        * zero sidVol 0 and 1
    clr @sidVol+2      * zero sidVol 2 and 3
	b    *r11
	.size	StartSID,.-StartSID

* Call this to stop the current song
	def	StopSID
	even
StopSID
    movb @StartSID+2,@songActive	 * zero all bits in songActive (pulls a 0 byte from a LI)
	jmp setMutes
	.size	StopSID,.-StopSID

* this data is in a special section so that you can relocate it at will
* in the makefile (ie: to put it in scratchpad). If you do nothing,
* it will be placed after the bss section (normally in low RAM)
    .section sidSongDat

	even
    def sidDat
sidDat
	bss 72

	even
	def sidVol
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
SidCtrl3 bss 3
SidCtrl2 equ SidCtrl3+1
SidCtrl1 equ SidCtrl3+2
    even

* we only need a special getCompressedSid if we have to use
* a different song bank. If, someday, we can share the same
* songbank, just sharing workBuf (instead of workSID) is all
* need to share the common code with the SN player.
	ref	getCompressedSid

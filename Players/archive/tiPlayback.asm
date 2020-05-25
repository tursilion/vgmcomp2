**************************************
* Player for VGMComp2 - TI-99/4A PSG version
* Part of libti99, Public Domain
* Meant to be assembled by the gcc assembler
* by Mike Brent aka Tursi
**************************************

**************************************
* Some options you can disable to reduce code size/improve performance
**************************************

* Defines a second code pass for playing sound effects. Won't work correctly without mutes.
#define USE_SFX
* Makes the engine run every second call
#define USE_30HZ
* Enables the use of the mute bits - necessary for the SFX engine
#define USE_MUTES
* Enables the use of the master volume (actually a subtraction - 0 is normal, 1 is 1 quieter, etc)
#define USE_MASTER_VOLUME

**************************************
*
* Each stream (8 bytes):
* 
* MainPtr				Pointer to data in the main stream
* CurPtr				Pointer for current run of data
* FctnPtr				Function to get data from CurPtr (NULL if need new one)
* RunCnt / LastData	Count of bytes to read in MSB, last data read in LSB (contains note lookup or volume)
* 
* Also, the 4 volume channels plus the time stream have a count byte:
* Cnt0 / Cnt1			Count for vol0 and vol1
* Cnt2 / Cnt3			Count for vol2 and vol3
* Time / Vol			Count for timestream, byte for volume control
* 
* Vol:
* 0123 MMMM				most significant nibble for muting audio (1=mute), least for master volume (just a subtraction)
* 
* And a storage for song address. (2 bytes)
* 
* That's 8*9+8 bytes for a player = 80 bytes. Was 124 bytes before.
* 
* R0 =				R8 = fctnptr
* R1 =				R9 = runcnts
* R2 =				R10= counters
* R3 =				R11= subroutine return
* R4 =				R12= song base
* R5 = other mute	R13= old WP
* R6 = mainptr		R14= old PC
* R7 = curptr		R15= old ST
*
**************************************

**************************************
* function exports
**************************************
	def ststart			* start a new tune - module in R1, index in R2 (usually 0)
	def ststop			* stop any music playing
	def stloop			* call every vertical blank if you call it yourself (BLWP! NOT BL!)
	def stint			* load this version into the vertical blank interrupt hook

#ifdef USE_SFX
	def sfxstart		* start a new SFX (if you want priority implement externally, same regs as stinit)
	def sfxstop			* stop any playing SFX (rarely needed)
	def sfxloop			* called automatically, normally you do not call this
#endif
	
* these are just intended to be read from the map file so they can be used for timing
	def timingin
	def timingout
	
**************************************
* some equates
**************************************

SOUND equ >8400
mywp  equ >8320
gplws equ >83e0

**************************************
* song data storage
**************************************
	dseg

* storage for return address
retsav			bss 2
retsav2         bss 2

* song base address
songad			bss 2
* main stream pointer
mainptr			bss 18
* current run pointer
curptr			bss 18
* function pointer for getting data
fctnptr			bss 18
* rle counter and last data values
runcnts			bss 18
* compression counters (not used for volume)
counters		bss 10
* volume master
volume			bss 1

* and the same for SFX
#ifdef USE_SFX

* song base address
sfxsongad		bss 2
* main stream pointer
sfxmainptr		bss 18
* current run pointer
sfxcurptr		bss 18
* function pointer for getting data
sfxfctnptr		bss 18
* rle counter and last data values
sfxruncnts		bss 18
* compression counters (not used for volume)
sfxcounters		bss 10
* volume master
sfxvolume		bss 1

#endif

* sadly, we need to waste a whole word to this simple flag
* maybe we can use more of it later...
#ifdef USE_30HZ

flag30hz		bss 2

#endif

**************************************
* Code starts here
**************************************
	pseg
	
**************************************
* some constant data
**************************************

datf0	data >f000
dat80	data >8000
dat40	data >4000
dat20	data >2000
dat10	data >1000
dat03   data >0300
dat02   data >0200
dat01   data >0100
tonemk	data >80a0,>c0e0
volmk	data >90b0,>d0f0
mutes	data >9fbf,>dfff
dat0004 data >0004
dat0003 data >0003

* function table for starting a new stream
strstart
    data stbackref,strle32,strle24,strle16
    data strle,stinline

**************************************
* mute the sound channels
**************************************
mute
	movb @mutes,@sound
	movb @mutes+1,@sound
	movb @mutes+2,@sound
	movb @mutes+3,@sound
	b *r11
	
**************************************
* ststart - start a new song
*	R0 - scratch (destroyed)
* 	R1 - address of song bank (destroyed)
*	R2 - index of song (normally 0, destroyed)
*   R3 - scratch (destroyed)
**************************************
ststart
	mov r1,@songad		* save the base
	mov r2,r0			* calculate stream offset
	sla r0,4			* multiply by 16
	sla r2,1			* multiply by 2
	a r2,r0				* add together (to get *18)
	a r1,r0				* stream base
	li r2,9				* 9 streams
	li r3,mainptr		* target
stsl1
	mov *r0+,*r3+		* load base address
	dec r2				* count down
	jne stsl1
	
	li r2,30			* 30 words to zero
stsl2
	clr *r3+			* zero it
	dec r2
	jne stsl2
	
	b @mute				* back to caller

**************************************
* ststop - stop the current song
**************************************
ststop
	clr @songad			* clear the playback
	b @mute
	
**************************************
* stint - interrupt hook wrapper for stloop
* meant to be called from the console interrupt hook
* will change the workspace to gplws
**************************************
stint
	blwp @stloop		* do the playing
	b *r11

**************************************
* stloop - main entry point for the playback every frame
* call with BLWP. Will use mywp for workspace
* this lets it return with your workspace restored
* the ifdefs get a little hairy in here because of sfx or no,
* and 30hz or no.
**************************************
stloop
	data mywp,stloop2
	
* hard coding is nice for the SFX mode to share code,
* but for simplicity we have to start somewhere	
stloop2
timingin

* in 30Hz mode, only music OR SFX runs
#ifdef USE_30HZ
	mov @flag30hz,r12	* check whose turn it is
	jeq dosfx			* do SFX if it's zero
	clr @flag30hz		* zero it for next time
#endif

* do music
	mov @songad,r12		* get current song address
	jeq donemusic		* no song to play

* load up the pointers and call the main process
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	li r4,volume		* address of my mute/volume register
#endif
#ifdef USE_SFX
	clr r5				* don't mute anyone else
#endif
	li r6,mainptr		* main pointers
	li r7,curptr		* current pointers
	li r8,fctnptr		* function pointers
	li r9,runcnts		* run counts
	li r10,counters		* counters
	bl @runsong

    mov r6,r6           * check if done
    jne stlpok
    bl @mute            * turn off the audio
    clr @songad         * song is over
stlpok

donemusic
#ifdef USE_30HZ
	jmp alldone			* don't do SFX same cycle
#endif

* do SFX (if enabled)
dosfx
#ifdef USE_SFX
	bl @sfxloop

    mov r6,r6           * check if done
    jne sfxlpok
    bl @restore         * restore the main audio
    clr @sfxsongad      * sfx is over
sfxlpok

#endif
#ifdef USE_30HZ
	seto @flag30hz		* set it for next time
#endif
	
alldone	
timingout
	rtwp

**************************************
**************************************

#ifdef USE_SFX

**************************************
* sfxstart - start a new sfx
*	R0 - scratch (destroyed)
* 	R1 - address of song bank (destroyed)
*	R2 - index of song (normally 0, destroyed)
*   R3 - scratch (destroyed)
**************************************
sfxstart
	mov r1,@sfxsongad	* save the base
	mov r2,r0			* calculate stream offset
	sla r0,4			* multiply by 16
	sla r2,1			* multiply by 2
	a r2,r0				* add together (to get *18)
	a r1,r0				* stream base
	li r2,9				* 9 streams
	li r3,sfxmainptr	* target
stsl1
	mov *r0+,*r3+		* load base address
	dec r2				* count down
	jne stsl1
	
	li r2,30			* 30 words to zero
stsl2
	clr *r3+			* zero it
	dec r2
	jne stsl2
	
	b *r11				* back to caller

**************************************
* sfxstop - stop the current sfx
*   R0 - scratch (destroyed)
*	R1 - scratch (destroyed)
*	R2 - scratch (destroyed)
*	R4 - changed to main song volume pointer
*
* don't expect this to work right if you disabled mutes
**************************************
sfxstop
	clr @sfxsongad		* clear the playback

* fall through into sfxrestore	

**************************************
* sfxrestore - clear the mutes and put audio back on the song
*   R0 - scratch (destroyed)
*	R1 - scratch (destroyed)
*	R2 - scratch (destroyed)
*	R4 - changed to main song volume pointer
*   R9 - changed to main song runcnts pointer
*
* don't expect this to work right if you disabled mutes
**************************************
sfxrestore
	mov r11,r2				* save return
	
	li r4,volume			* get the main player's mute byte
	li r9,runcnts			* get the main player's runcnts
	movb *r4,r0				* collect mute bits
	szcb @datf0,*r4		 	* clear the main song mutes
	mov @songad,r1			* check for an active song
	jne sfxr0
	b @mute					* no song, so just mute
sfxr0

	cocb @dat80,r0			* check mute, if it WAS muted we restore it
	jne sfxr1
	bl @restore0
	bl @restore0v
sfxr1

	cocb @dat40,r0			* check mute
	jne sfxr2
	bl @restore1
	bl @restore1v
sfxr2
	
	cocb @dat20,r0			* check mute
	jne sfxr3
	bl @restore2
	bl @restore2v
sfxr3

	cocb @dat10,r0			* check mute
	jne sfxr4
	bl @restore3
	bl @restore3v
sfxr4
	
	b *r2					* back to caller

**************************************
* sfxloop - main entry point for the sfx playback every frame
* you should not need to call this, called by stloop
**************************************
sfxloop
	mov @sfxsongad,r12	* get current song address
	jne sfxloop2a
	b *r11				* no song to play
sfxloop2a

* load up the pointers and call the main process
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	li r4,sfxvolume		* address of my mute/volume register
#endif
	li r5,volume		* address of main song mute register
	li r6,sfxmainptr	* main pointers
	li r7,sfxcurptr		* current pointers
	li r8,sfxfctnptr	* function pointers
	li r9,sfxruncnts	* run counts
	li r10,sfxcounters	* counters
	
	b @runsong
#endif

**************************************
* restore0 - restore tone channel 0
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore0
#ifdef USE_MUTES
	movb *r4,r1				* get mute flags
	cocb @DAT80,r1			* is it muted?
	jne restore0a			* jump if no
	b *r11					* else return
restore0a
#endif

	clr r1					* to get data
	movb @1(r9),r1			* get last byte
	srl r1,7				* shift to LSB and multiply by 2
	a @2(R1),r1				* add base of note table
	mov *r1,r1				* get the note
	ori r1,>8000			* command bits
	movb r1,@sound			* write command
	swpb r1
	movb r1,@sound			* write other half
	
	b *r11

**************************************
* restore0v - restore volume channel 0
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore0v
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	movb *r4,r1				* get mute flags
#endif

#ifdef USE_MUTES
	cocb @DAT80,r1			* is it muted?
	jne restore0va			* jump if no
	b *r11					* else return
restore0va
#endif

#ifdef USE_MASTER_VOLUME
	andi r1,>0f00			* get just the master volume
	ab @9(r9),r1			* add in last volume (more positive = more quieter)
	ci r1,>1000				* check if we exceeded mute
	jl restore0vb			* jump if okay
	li r1,>0f00				* reset to mute
restore0vb
#else
	movb @9(r9),r1			* get last volume
#endif

	ori r1,>9000			* command bits
	movb r1,@sound			* write command
	b *r11

**************************************
* restore1 - restore tone channel 1
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore1
#ifdef USE_MUTES
	movb *r4,r1				* get mute flags
	cocb @DAT40,r1			* is it muted?
	jne restore1a			* jump if no
	b *r11					* else return
restore1a
#endif

	clr r1					* to get data
	movb @3(r9),r1			* get last byte
	srl r1,7				* shift to LSB and multiply by 2
	a @2(R1),r1				* add base of note table
	mov *r1,r1				* get the note
	ori r1,>A000			* command bits
	movb r1,@sound			* write command
	swpb r1
	movb r1,@sound			* write other half
	
	b *r11

**************************************
* restore1v - restore volume channel 1
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore1v
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	movb *r4,r1				* get mute flags
#endif

#ifdef USE_MUTES
	cocb @DAT40,r1			* is it muted?
	jne restore1va			* jump if no
	b *r11					* else return
restore1va
#endif

#ifdef USE_MASTER_VOLUME
	andi r1,>0f00			* get just the master volume
	ab @11(r9),r1			* add in last volume (more positive = more quieter)
	ci r1,>1000				* check if we exceeded mute
	jl restore1vb			* jump if okay
	li r1,>0f00				* reset to mute
restore1vb
#else
	movb @11(r9),r1			* get last volume
#endif

	ori r1,>B000			* command bits
	movb r1,@sound			* write command
	b *r11

**************************************
* restore2 - restore tone channel 2
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore2
#ifdef USE_MUTES
	movb *r4,r1				* get mute flags
	cocb @DAT20,r1			* is it muted?
	jne restore2a			* jump if no
	b *r11					* else return
restore2a
#endif

	clr r1					* to get data
	movb @5(r9),r1			* get last byte
	srl r1,7				* shift to LSB and multiply by 2
	a @2(R1),r1				* add base of note table
	mov *r1,r1				* get the note
	ori r1,>C000			* command bits
	movb r1,@sound			* write command
	swpb r1
	movb r1,@sound			* write other half
	
	b *r11

**************************************
* restore2v - restore volume channel 2
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore2v
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	movb *r4,r1				* get mute flags
#endif

#ifdef USE_MUTES
	cocb @DAT20,r1			* is it muted?
	jne restore2va			* jump if no
	b *r11					* else return
restore2va
#endif

#ifdef USE_MASTER_VOLUME
	andi r1,>0f00			* get just the master volume
	ab @13(r9),r1			* add in last volume (more positive = more quieter)
	ci r1,>1000				* check if we exceeded mute
	jl restore2vb			* jump if okay
	li r1,>0f00				* reset to mute
restore2vb
#else
	movb @13(r9),r1			* get last volume
#endif

	ori r1,>D000			* command bits
	movb r1,@sound			* write command
	b *r11

**************************************
* restore3 - restore noise channel 3
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore3
#ifdef USE_MUTES
	movb *r4,r1				* get mute flags
	cocb @DAT10,r1			* is it muted?
	jne restore3a			* jump if no
	b *r11					* else return
restore3a
#endif

	clr r1					* to get data
	movb @7(r9),r1			* get last byte
	ori r1,>E000			* command bits
	movb r1,@sound			* write command
	
	b *r11

**************************************
* restore3v - restore noise channel 3
*   r1 - corrupted
*	R4 - points to mute/volume byte
*   R9 - points to runcnts
**************************************
restore3v
#if defined(USE_MUTES) || defined(USE_MASTER_VOLUME)
	movb *r4,r1				* get mute flags
#endif

#ifdef USE_MUTES
	cocb @DAT10,r1			* is it muted?
	jne restore3va			* jump if no
	b *r11					* else return
restore3va
#endif

#ifdef USE_MASTER_VOLUME
	andi r1,>0f00			* get just the master volume
	ab @15(r9),r1			* add in last volume (more positive = more quieter)
	ci r1,>1000				* check if we exceeded mute
	jl restore3vb			* jump if okay
	li r1,>0f00				* reset to mute
restore3vb
#else
	movb @15(r9),r1			* get last volume
#endif

	ori r1,>F000			* command bits
	movb r1,@sound			* write command
	b *r11

**************************************
* stbackref - start a backref string 
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
* fall through into the run function
* count is 6 bits long (0x3f)
**************************************
stbackref
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>3f00           * get the count bits
    ab @dat03,r0            * add 4, then pre-decrement for the byte pulled below
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,gobackref
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r0              * get pointer
    inct *r2                * going to pull two bytes
    movb *r0+,r2            * get first byte of backref (NOT byte aligned)
    swpb r2
    movb *r0,r2             * get second byte
    swpb r2                 * fix byte order
    mov r2,r2               * check for zero
    jne stback1             * it's valid, keep going

    clr r6                  * clear main pointer, caller needs to check
    b *r11

stback1
    mov r7,r0               * get curptr
    a r1,r0                 * add offset
    mov r2,*r0              * save curptr

* fall through

**************************************
* gobackref - get a byte from a backref string
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
gobackref
    mov r7,r0               * get curptr
    a r1,r0                 * get offset
    mov *r0,r2              * get the pointer
    inc *r0                 * increment it
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* strle32 - start an RLE32 string
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
* fall through into the run function
* count is 5 bits long (0x1f)
**************************************
strle32
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>1f00           * get the count bits
    ab @dat02,r0            * add 2
    sla r0,2                * multiply by 4 for bytes
    dec r0                  * predecrement for the byte below
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,gorle32
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r0              * get pointer
    a @dat0004,*r2          * going to pull four bytes

    mov r7,r2               * get curptr
    a r1,r2                 * add stream offset
    mov r0,*r2              * save (original) mainptr to curptr

* fall through

**************************************
* gorle32 - get a byte from an RLE
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
gorle32
    mov r7,r0               * get curptr
    a r1,r0                 * get offset

    mov r6,r2               * get mainptr
    a r1,r2                 * get offset

    c *r0,*r2               * are they equal?
    jne nogorle32

    s @dat0004,*r0          * backup four bytes
nogorle32

    mov *r0,r2              * get the actual pointer
    inc *r0                 * increment it
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* strle24 - start an RLE24 string
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
* fall through into the run function
* count is 5 bits long (0x1f)
**************************************
strle24
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>1f00           * get the count bits
    ab @dat02,r0            * add 2
    mov r0,r2
    a r2,r0                 * multiply by 3 for bytes
    a r2,r0
    dec r0                  * predecrement for the byte below
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,gorle24
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r0              * get pointer
    a @dat0003,*r2          * going to pull three bytes

    mov r7,r2               * get curptr
    a r1,r2                 * add stream offset
    mov r0,*r2              * save (original) mainptr to curptr

* fall through

**************************************
* gorle24 - get a byte from an RLE
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
gorle24
    mov r7,r0               * get curptr
    a r1,r0                 * get offset

    mov r6,r2               * get mainptr
    a r1,r2                 * get offset

    c *r0,*r2               * are they equal?
    jne nogorle24

    s @dat0003,*r0          * backup three bytes
nogorle24

    mov *r0,r2              * get the actual pointer
    inc *r0                 * increment it
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* strle16 - start an RLE16 string
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
* fall through into the run function
* count is 5 bits long (0x1f)
**************************************
strle16
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>1f00           * get the count bits
    ab @dat02,r0            * add 2
    sla r0,1                * multiply by 2 for bytes
    dec r0                  * predecrement for the byte below
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,gorle16
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r0              * get pointer
    inct *r2                * going to pull two bytes

    mov r7,r2               * get curptr
    a r1,r2                 * add stream offset
    mov r0,*r2              * save (original) mainptr to curptr

* fall through

**************************************
* gorle16 - get a byte from an RLE
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
gorle16
    mov r7,r0               * get curptr
    a r1,r0                 * get offset

    mov r6,r2               * get mainptr
    a r1,r2                 * get offset

    c *r0,*r2               * are they equal?
    jne nogorle16

    dect *r0                * backup two bytes
nogorle16

    mov *r0,r2              * get the actual pointer
    inc *r0                 * increment it
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* strle - start an RLE string
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
* fall through into the run function
* count is 5 bits long (0x1f)
**************************************
strle
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>1f00           * get the count bits
    ab @dat03,r0            * add 3
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,gorle
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r0              * get pointer
    inc *r2                 * going to pull one byte

    mov r7,r2               * get curptr
    a r1,r2                 * add stream offset
    mov r0,*r2              * save (original) mainptr to curptr

* fall through

**************************************
* gorle - get a byte from an RLE
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
gorle
    mov *r0,r2              * get the actual pointer
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* stinline - start an inline string
*   R0 - contains the control byte in the MSB
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
*   R3 - scratch (corrupted)
* fall through into the run function
* count is 6 bits long (0x3f)
**************************************
stinline
    mov r10,r2              * get counters
    a r1,r2                 * add the offset
    andi r0,>3f00           * get the count bits
    ab @dat01,r0            * add 1
    movb r0,*r2             * save the new counter

    mov r8,r2               * get fctnptr
    a r1,r2                 * add the offset
    li r0,goinline
    mov r0,*r2              * save it

    mov r6,r2               * get mainptr
    a r1,r2                 * add stream offset
    mov *r2,r3              * get pointer
    srl r0,8                * make byte a word
    a r0,*r2                * move to end of string

    mov r7,r2               * get curptr
    a r1,r2                 * add stream offset
    mov r3,*r2              * save (original) mainptr to curptr

* fall through

**************************************
* goinline - get a byte from an inline
*   R0 - MSB return contains byte
*   R1 - contains the word offset
*   R2 - scratch (corrupted)
**************************************
goinline
    mov r7,r0               * get curptr
    a r1,r0                 * get offset
    mov *r0,r2              * get the pointer
    inc *r0                 * increment it
    movb *r2,r0             * get the byte
    b *r11                  * return

**************************************
* getcompressed - return a compressed byte in R0 MSB
*   R1 - offset for the stream data
*   R2 - scratch - corrupted
*   R3 - scratch (corrupted)
**************************************
getcompressed
* Check for bytes left in current sequence
    mov r1,r2               * get the word offset
    a r10,r2                * get counters array
    movb *r2,r0             * get the count
    jeq comp2               * if it's zero

    sb @dat01,*r2           * decrement
    mov @fctnptr(r1),r0     * get current lookup function
    b *r0                   * jump to it (it will call return)

comp2
* start a new stream
    mov r6,r2               * mainptr
    a r1,r2                 * add offset
    mov *r2,r0              * get pointer
    inc *r2                 * going to read a byte
    movb *r0,r0             * get the next control byte
    mov r0,r2               * make a scratch copy for type
    srl r2,13               * keep only 3 bits
    sla r2,1                * make a word pointer
    mov @strstart(r2),r2    * get the function address
    b *r2                   * jump to it (it will call return)

**************************************
**************************************
** Main Player Code
**************************************
**************************************
runsong
	mov r11,@retsave		* save return address
	
* Process timestream first
* We can call the restore functions to load the notes and volumes
* That way they will automatically handle mutes and master volume
* and all we need to do is get the bytes loaded into the right place

* when we pull a byte from a stream, we should set the other side's mute
* this could cause weirdness if a sfx used voice 2 but no noise and the main tune
* used custom frequency noises -- but I think that's up to the user to prevent
* For performance, this code assumes that the song is valid if we get this
* far. If the timestream ends, we will invalidate the song, but we need
* to check everything else.

    movb @16(r9),r0         * get run count
    jeq runsong1            * if it's zero

    sb @dat01,@16(r9)       * count down
    jmp chkvol

* we need a new timestream byte
runsong1
    li r1,16                * offset we are after
    bl @getcompressed       * get a byte into R0 - timestream ending clears r6 to end the song
    mov r6,r6
    jne runsong2
    mov @retsave,r11
    b *r11

runsong2
    mov r0,r1               * make a copy
    andi r1,>0f00           * extract just the counts
    movb r1,@16(r9)         * save the result
    sla r0                  * check tone 1
    jnc rsnot1

* process tone 1, save r0

rsnot1
    sla r0                  * check tone 2
    jnc rsnot2

* process tone 2, save r0

rsnot2
    sla r0                  * check tone 3
    jnc rsnot3

* process tone 3, save r0

rsnot3
    sla r0                  * check noise
    jnc rsnonoi

* process noise

jsnonoi


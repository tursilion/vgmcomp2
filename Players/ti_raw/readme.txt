This files are automatically created with makerawasm.bat. refs, defs, and equs are moved to the top of the file, all else is as listed. This sometimes leaves orphan comments - if anything is confusing to read check the true source files in libtivgm2.

You can include these in your source with COPY or you can REF the required symbols in your code. Both work. These files do not include an END directive for that reason. You just choose the one file that applies to you.

If you are using GCC, don't use these files. Link with libtivgm2.a instead.

In ALL cases, SongLoop (and other xxxLoop functions) must be called with workspace >8300, and ALL registers should be assumed trashed.

30hz versions split the work, 2 channels each call instead of all 4. To be clear, you must still call the 30 versions at 60hz, they just play slower data. This better evens out the CPU load.

If you want to only update the music at 30hz, then encode the music for 30hz per normal, but use the 60hz functions (just call every other frame instead).

Naturally, 60 and 30 are analogous to 50 and 25 on PAL systems.

I didn't build combined SID/SN files, since the number of variants is already large. But this is perfectly legal - just make sure to include only one copy of the GetCompressedByte function and its helpers. You can use makerawasm to do it for you, just copy the existing templates and include only one copy of CPlayerCommonHandEdit.asm.

-------------------------------
---- SN Music only at 60hz ----
-------------------------------

tiSNonly.asm - Music playback only, 60hz version.

	StartSong: Prepares a new song to start playing
		li r1,<sbf address>
		li r2,<song index in MSB - 0 is first>
		bl @StartSong
		
	StopSong: Stops playback and mutes audio.
		bl @StopSong
		
	SongLoop: Call every frame to play the music - must be on WP >8300
		bl @SongLoop
		
	
-------------------------------
---- SN Music only at 30hz ----
-------------------------------

tiSNonly30.asm - Music playback only, 30hz version.

	StartSong: Prepares a new song to start playing
		li r1,<sbf address>
		li r2,<song index in MSB - 0 is first>
		bl @StartSong
		
	StopSong: Stops playback and mutes audio.
		bl @StopSong
		
	SongLoop30: Call every frame to play the music - must be on WP >8300
		bl @SongLoop30
		
		
----------------------------------		
---- SN SFX and Music at 60hz ----
----------------------------------		

tiSfxSN.asm - SFX and Music playback, 60hz version.

	StartSong: Prepares a new song to start playing
		li r1,<sbf address>
		li r2,<song index in MSB - 0 is first>
		bl @StartSong
		
	StopSong: Stops playback and mutes audio.
		bl @StopSong
		
	StartSfx: Starts a new SFX, replacing the old one if the priority is higher
		li r1,<sbf address>
		li r2,<sfx index in MSB - 0 is first>
		li r3,<priority in MSB - higher takes precedence>
		bl @StartSfx
		
	StopSfx: Stops the current SFX and restores the music channels
		bl @StopSfx
		
	SfxLoop: Call every frame to play the sfx - must be on WP >8300. Note: call BEFORE SongLoop.
		bl @SfxLoop

	SongLoop: Call every frame to play the music - must be on WP >8300. Note: call AFTER SfxLoop.
		bl @SongLoop
		
	
----------------------------------		
---- SN SFX and Music at 30hz ----
----------------------------------		

tiSfxSN30.asm - SFX and Music playback, 30hz version.

	StartSong: Prepares a new song to start playing
		li r1,<sbf address>
		li r2,<song index - 0 is first>
		bl @StartSong
		
	StopSong: Stops playback and mutes audio.
		bl @StopSong
		
	StartSfx: Starts a new SFX, replacing the old one if the priority is higher
		li r1,<sbf address>
		li r2,<sfx index in MSB - 0 is first>
		li r3,<priority in MSB - higher takes precedence>
		bl @StartSfx
		
	StopSfx: Stops the current SFX and restores the music channels
		bl @StopSfx
		
	SfxLoop30: Call every frame to play the sfx - must be on WP >8300. Note: call BEFORE SongLoop30.
		bl @SfxLoop

	SongLoop30: Call every frame to play the music - must be on WP >8300. Note: call AFTER SfxLoop30.
		bl @SongLoop30
		

--------------------------------		
---- SID Music only at 60hz ----
--------------------------------		

tiSIDonly.asm - Music playback only, 60hz version.

	StartSID: Prepares a new song to start playing
		li r1,<control byte for channel 1 in MSB>
		movb r1,@SidCtrl1
		li r1,<control byte for channel 2 in MSB>
		movb r1,@SidCtrl2
		li r1,<control byte for channel 3 in MSB>
		movb r1,@SidCtrl3
		
		li r1,<sbf address>
		li r2,<song index in MSB - 0 is first>
		bl @StartSID
		
	StopSID: Stops playback and mutes audio.
		bl @StopSID
		
	SIDLoop: Call every frame to play the music - must be on WP >8300
		bl @SIDLoop
		
	
--------------------------------		
---- SID Music only at 30hz ----
--------------------------------		

tiSIDonly30.asm - Music playback only, 30hz version.

	StartSID: Prepares a new song to start playing
		li r1,<control byte for channel 1 in MSB>
		movb r1,@SidCtrl1
		li r1,<control byte for channel 2 in MSB>
		movb r1,@SidCtrl2
		li r1,<control byte for channel 3 in MSB>
		movb r1,@SidCtrl3
		
		li r1,<sbf address>
		li r2,<song index in MSB - 0 is first>
		bl @StartSID
		
	StopSID: Stops playback and mutes audio.
		bl @StopSID
		
	SIDLoop30: Call every frame to play the music - must be on WP >8300
		bl @SIDLoop

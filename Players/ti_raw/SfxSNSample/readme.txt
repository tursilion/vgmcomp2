This example tested with Asm994a. The only file to build is main.a99 - it uses COPY to pull in the other two.

However, Asm994A seems to need full paths, so you will want to edit main.a99 for your local paths. xdt99 should be able to handle local paths.

The EA#5 files were made by loading the object file, then the SAVE utility, and using that to save program.

main.a99 - quick and dirty re-coding of the TISNSFX sample app in assembly
music.asm - the SBF file from above, re-converted for TI format (using my bin2inc)
tiSfxSN.asm - the combined player source code for SFX and SN playback (copied from parent folder)

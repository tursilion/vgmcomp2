This repo is NOT ready for prime-time yet, but I'm actively working on it. I'm NOT accepting patches - wait till I'm done.

You will find EXEs in the "dist" folder for Windows.

Docs are in progress, but basic flow:

- use vgm_id to determine what chips are in your VGM file. Support now for AY8910, Gameboy DMG, NES, Pokey, and PSG (which is native to us!)
- run vgm_XXX2psg on your file (where XXX is the desired chip to extract) to get PSG compatible tracks
- run prepare4PSG and pass in three tone channels and one noise to prepare for PSG compression. Note at the moment there are no tools to merge tracks or convert invalid data - those are coming.
- run vgmcomp2 on the prepared file to compress. Multiple songs can be passed to pack into a single shared file.

At any point in the process you can run testPlayPSG and pass it any number of tracks or even prepared PSG files, and it will play them more or less as the output would sound in that state. It is not limited to the restrictions of the PSG, you can have 8 tone channels or 4 noise channels or even both, if you have the source files.

Each tool outputs command line help if run with no arguments. Only psg2psg is going to sound exactly correct.

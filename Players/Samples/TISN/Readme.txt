- to aid compatibility with assembly code, there are no initialized variables (so you don't need to worry about initialized data)
- the default build links the data space at 0x2000 and code at 0xA000, you can change this with:
        --section-start .text=a000 --section-start .data=2080
- Before starting, you need to set the stack pointer (R15). The GCC startup uses 0x4000. This code
  requires 16 bytes of stack for C (0 for asm). This is scratch memory that need not be preserved between calls.

Unlike the older VGM player, you have to do a little more work yourself in this one. This is to keep
the player code lighter while offering more flexibility to your own code. For instance, if you want sound effects,
you need to provide the library twice with different prefixes, call both updates at the appropriate time,
and transfer muting of active channels from SFX yourself. This sounds terrible, but it means that the player
doesn't need to handle pointers to run SFX and Music with the same code. 

I'll try to provide example usage to show it working, anyway.

The player was originally based on the C code but has been extensively hand-tuned. 
The sample player calls it from the interrupt hook to demonstrate usage from C. 
The main issue is that you now need to give it the workspace at >8300. The C version
also needs the stack pointer R10 set up, but that's not recommended for use.

MEMORY USAGE (C):                                V1              HandTuned
    Song data storage: 88 bytes                 124 bytes        88 bytes
    Stack usage: 16 bytes                         0 bytes         0 bytes
    Workspace: 32 bytes - but optional           32 bytes         0 bytes (*)

    Total: 136 bytes                            156 bytes        88 bytes

    Code: 1409 bytes                            608 bytes       732 bytes (348 bytes shared)

CPU USAGE:
    Over the course of my test song (Silius title):
        MIN:  2,198 cycles                         938             1,294
        MAX: 17,310 cycles                       9,722             8,266
        AVG:  6,208 cycles                       3,989             3,822
  Scanlines:  11-90 (avg 32)                  5-51 (avg 21)     7-43 (avg 20)

  (190.8 cycles per scanline)

(* - no /separate/ workspace is required anymore, but it will completely eat your existing workspace.
However, depending on your application, this may be okay. For instance, the demo player is full GCC
and yet GCC did not need to save any registers to call it. It's up to you now if you need it.)

You can improve assembly performance SLIGHTLY by moving the song data into the scratchpad - add
this to the linker line: --section-start songDat=8320
This will use 88 bytes of scratchpad starting at >8320, but the average performance gain is only 
around 200 cycles, and is probably not worth the cost. Still, if the scratchpad is there doing
nothing anyway... ;)

C code can be made slightly faster by defining uWordSize as unsigned int, but this costs 30-40 bytes more in RAM usage.

.. but, it's working! So now it's just refinement... (and getting the sound effect demo version done...)


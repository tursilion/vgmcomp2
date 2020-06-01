- to aid compatibility with assembly code, there are no initialized variables (so you don't need to worry about initialized data)
- the default build links the data space at 0x2000 and code at 0xA000, you can change this with:
        --section-start .text=a000 --section-start .data=2080
- Before starting, you need to set the stack pointer (R15). The GCC startup uses 0x4000. This code
  requires (?XX?X?) bytes of stack. This is scratch memory that need not be preserved between calls.

Unlike the older VGM player, you have to do a little more work yourself in this one. This is to keep
the player code lighter while offering more flexibility to your own code. For instance, if you want sound effects,
you need to provide the library twice with different prefixes, call both updates at the appropriate time,
and deal with the muting of music during SFX yourself. This sounds terrible, but it means that the player
doesn't need to check inline for mutes, or to handle pointers to run SFX and Music with the same code.
It also lets you tune the system to your needs. For instance, if you only want sound effects on one
channel, you can now only worry about muting that one channel, where the old code always checked all four.

I'll try to provide example usage to show it working, anyway.

The player is now written in C which makes it a little more directly applicable to libti99 projects,
but it's still usable from assembly language. The sample player calls it from the interrupt hook to
demonstrate this. The main issue is that you now need to give it the workspace at >8300 and set a
stack pointer in R15 with enough space below to execute.

MEMORY USAGE:                                   V1              HandTuned1
    Song data storage: 110 bytes                124 bytes
    Stack usage: 16 bytes                         0 bytes
    Workspace: 32 bytes - but optional           32 bytes

    Total: 158 bytes                            156 bytes

    Code: 1348 bytes                            608 bytes       1218 bytes

CPU USAGE:
    Over the course of my test song:
        MIN:  2,070 cycles                       1,200           2,038
        MAX: 12,898 cycles                      10,000          10,276
        AVG:  5,619 cycles                       2,500           5,073

So, it's probably still worth doing a more optimized assembly build. We can likely attribute the cycle count 
directly to the increase in code size - roughly doubled.

.. but, it's working! So now it's just refinement... (and getting the sound effect demo version done...)





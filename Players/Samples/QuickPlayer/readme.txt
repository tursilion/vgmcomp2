Chaining allows a player to branch out to a cartridge program when the song ends.

Your program needs to load the target program, and change the flags variable for chaining.

// 00: six bytes of flag (~~FLAG)
// 06: two bytes for SN music (can be 0)
// 08: two bytes for SID or AY music (can be 0)
// 0A: three bytes of SID flags
// 0D: one byte of loop
// 0E: two bytes of pointer to a memory address for chaining

The address provided in the flags variable is the address of a cartridge page. The TI and Coleco versions each operate differently.

The TI version will bank the cartridge page in, and then start the first program in the program list.

  mov r0,@>6000     * page select (patched when copied to RAM)
  mov @>6006,r0     * pointer to program list
  inct r0           * point to start address of first program
  mov *r0,r0        * get start address
  b *r0             * execute it

The Coleco version will bank the cartridge page in using the Megacart scheme, and jump to 0xC000

  LD   a,($ffff)    ; page select (patched when copied to RAM)
  JP   $c000        ; execute it

  The code above is copied to RAM and executed from there, which permits the cartridge page to change without crashing the program.

  When you build your cartridge, you can hard code the chain entry as you build the ROM image. If you are using the TI, you can
  also load the file into RAM first and then patch it there (this is not an option for the Coleco).


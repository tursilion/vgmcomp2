This is based on ti_raw, but is hand edited. There is a possibility that this code may fall out of date if that code is updated - let me know if you notice that.

At this time, I've elected to only bring across the SN player, 60hz and 30hz versions. They both work the same way and the only difference is which one you choose to load. The following CALL LINKS are added.

CALL LOAD("DSK1.TISN")
-or-
CALL LOAD("DSK1.TISN30")

If you are using with the HMLOADER included with The Missing Link, you will instead use TISNHI, which knows about the extended REF/DEF table. It will ONLY work in this environment.

...then, load your music. You can load it anywhere you have room if you know how to do that, but the important part is that it must DEFine a label named "MUSIC". The SONGGO function will search the REF/DEF table for this name.

Then it's simple:

CALL LINK("SONGGO",0)
- will search the ref/def table for the label MUSIC and play the first song from that entry. Replace 0 with other values for later songs. They must exist - the code does not check if the song number is valid!
- the player will set the interrupt hook, so interrupts must be active. Any existing address is linked (ie: TML)
- XB holds interrupts frequently, so expect music to play irregularly in regular XB, unless it's fairly simple

CALL LINK("ACTIVE",VAR)
- will set 'VAR' to 0 if the music is not playing, or 1 if it is.

CALL LINK("STOPSN") 
- will stop the music and unhook the interrupt (restoring any previous interrupt if applicable)
- You should also use STOPSN shortly after loading the player to initialize it

Advanced
--------

--SFX--
At the moment, sound effects that play over the music aren't possible, but you can do them the old fashioned way (CALL SOUND) with limited success, depending on the complexity of your music.


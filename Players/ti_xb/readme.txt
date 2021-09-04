This is based on ti_raw, but is hand edited. There is a possibility that this code may fall out of date if that code is updated - let me know if you notice that.

At this time, I've elected to only bring across the SN player, 60hz and 30hz versions. They both work the same way and the only difference is which one you choose to load. The following CALL LINKS are added.

CALL LOAD("DSK1.TISN")
-or-
CALL LOAD("DSK1.TISN30")

...then, load your music. You can load it anywhere you have room if you know how to do that, but the important part is that it must DEFine a label named "MUSIC". This is because I hate passing numbers from XB, and addresses suck in decimal anyway.

Then it's simple:

CALL LINK("SONGGO")
- will search the ref/def table for the label MUSIC and play the first song from that entry
- the player will set the interrupt hook, so interrupts must be active. I think the compiler requires this as well, so any existing address is linked.
- XB holds interrupts frequently, so expect music to play irregularly in regular XB, unless it's fairly simple
- it will use a workspace at >8320 internally - this can't be moved without reassembling

CALL LINK("STOPSN")
- will stop the music and unhook the interrupt
- use STOPSN shortly after loading the player to initialize it

Advanced
--------
I deliberately didn't build more functions in there, but it is possible to have multiple songs and check if ended.

--SFX--
At the moment, sound effects that play over the music aren't possible, but you can do them the old fashioned way (CALL SOUND) with limited success, depending on the complexity of your music.

--Different song--
- patch the address of 'MUSIC' with a different one in the REF/DEF table. This is a bit slow to do if you need to search, but this code will do it, if you know the address you want. Getting that in itself may be a mission, but if you know iT, and its address is in 'ADR', you can do something like this:

10000 REM PATCH ADDRESS OF MUSIC TO 'ADR'
10010 CALL PEEK(8196,N1,N2)::REM GET TOP OF FREE RAM
10020 N=N1*256+N2
10030 REM SEARCH TOP DOWN
10040 FOR PTR=16376 TO N STEP -8
10050 CALL PEEK(PTR,N1,N2,N3,N4,N5,N6)::REM CHECK NAME
10060 IF N1<>77 OR N2<>85 OR N3<>83 OR N4<>73 OR N5<>67 OR N6<>32 THEN 10100
10070 CALL LOAD(PTR+6,INT(ADR/256),ADR-(INT(ADR/256)*256))
10080 PTR=0
10100 NEXT PTR
10110 RETURN

It would be okay to cache PTR as well, if you needed to do this often.

--Check if Ended--
Similar to the above, the address of the byte that indicates if the song is stopped can be looked up relative to SONGGO. Once you have the address, you can use CALL PEEK to check it without doing the search again. 0 means it's not playing.

11000 REM FIND ADDRESS OF SONGACTIVE, SET IT IN 'ACTIVE'
11005 REM THEN USE CALL PEEK(ACTIVE,A) AND CHECK A
11010 CALL PEEK(8196,N1,N2)::REM GET TOP OF FREE RAM
11020 N=N1*256+N2
11030 REM SEARCH TOP DOWN
11040 FOR PTR=16376 TO N STEP -8
11050 CALL PEEK(PTR,N1,N2,N3,N4,N5,N6)::REM CHECK NAME
11060 IF N1<>83 OR N2<>79 OR N3<>78 OR N4<>71 OR N5<>71 OR N6<>79 THEN 11100
11070 CALL PEEK(PTR+6,N1,N2)
11080 ACTIVE=N1*256+N2-7
11090 PTR=0
11100 NEXT PTR
11110 RETURN

--Play different index in same sound bank--
This time you can patch the actual assembly language to change the starting song index, in a multi-song bank.
This assumes a fixed offset forward from SONGGO. Again, you can cache the final value N to use repeatedly.

12000 REM PATCH SONGGO WITH INDEX IN 'I' (first song is 0)
12010 CALL PEEK(8196,N1,N2)::REM GET TOP OF FREE RAM
12020 N=N1*256+N2
12030 REM SEARCH TOP DOWN
12040 FOR PTR=16376 TO N STEP -8
12050 CALL PEEK(PTR,N1,N2,N3,N4,N5,N6)::REM CHECK NAME
12060 IF N1<>83 OR N2<>79 OR N3<>78 OR N4<>71 OR N5<>71 OR N6<>79 THEN 12100
12070 CALL PEEK(PTR+6,N1,N2)
12080 N=N1*256+N2+56
12090 CALL LOAD(N,I)
12095 PTR=0
12100 NEXT PTR
12110 RETURN


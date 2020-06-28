This links together the TI playback code into a single linkable library. Link with -ltivgm2

There are three players involved. The SN and SID players will standalone, but the SFX player will also bring in SN. (There is no SID SFX player at the moment). All players will share the common decompression code.
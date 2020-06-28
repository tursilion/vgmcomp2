This is a port of TISN to operate in parallel as an SFX player. It's essentially the same code except that the variable names are changed so as not to conflict, and rather than /using/ the mute bits, it /sets/ them. Therefore, this code will not link unless you ALSO build in TISN, as in the sample.

Naturally, it shares the CPlayerCommonHandEdit code, adding about 350 bytes for SFX.

If you ONLY want sound effects, then just use TISN. You only need to link in this code if you want both sound effects and music at the same time.

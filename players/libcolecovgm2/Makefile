## build libcolecovgm2
CC = "c:/program files/sdcc/bin/sdcc"
CFLAGS = -mz80 -c "-I../include" "-I../CPlayer" --std-sdcc99 --vc -DENABLEFX -DBUILD_COLECO --opt-code-speed --fsigned-char
AS = "c:/program files/sdcc/bin/sdasz80"
AR = "c:/program files/sdcc/bin/sdar"
AFLAGS = -plosgff
RM = del /F
CP = copy /Y
EXT=rel

# output file
NAME=libcolecovgm2.a

# List of compiled objects used in executable
OBJECT_LIST=\
    ColecoPlayerAY.$(EXT)   \
    ColecoPlayerSFX.$(EXT)  \
    ColecoPlayerSN.$(EXT)   \
    CPlayerCommon.$(EXT) \
    CPlayerTest.$(EXT)

# Recipe to compile the library
all: library

library: $(OBJECT_LIST)
	$(AR) -rc $(NAME) $(OBJECT_LIST)

# Recipe to clean all compiled objects
.phony clean:
	$(RM) *.rel *.map *.lst *.lnk *.sym *.asm *~ *.o *.obj *.ihx *.sprite.* *.rom *.rel *.a *.lib

# Recipe to import foreign files
CPlayerCommon.rel: ..\CPlayer\CPlayerCommon.c ..\CPlayer\CPlayer.c
	$(CP) ..\CPlayer\CPlayerCommon.c .
	$(CC) -c CPlayerCommon.c $(CFLAGS) -o CPlayerCommon.rel
CPlayerTest.rel: ..\CPlayer\CPlayerTest.c
	$(CP) ..\CPlayer\CPlayerTest.c .
	$(CC) -c CPlayerTest.c $(CFLAGS) -o CPlayerTest.rel

# Recipe to compile all C files
%.rel: %.c
	$(CC) -c $< $(CFLAGS) -o $@

# Recipe to compile all assembly files
%.rel: %.s
	$(AS) -o $@ $<

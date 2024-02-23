all:
	@echo make ti99 or make coleco

############ TI99 part ###############
# Paths to TMS9900 compilation tools
GASTI=/home/tursilion/gcc9900/bin/tms9900-as
LDTI=/home/tursilion/gcc9900/bin/tms9900-ld
CCTI=/home/tursilion/gcc9900/bin/tms9900-gcc
B2ITI=/mnt/d/tools/bin2inc.exe

# Path to elf2cart conversion utility
ELF2EA5=/home/tursilion/gcc9900/bin/elf2ea5
EA5PLIT=/home/tursilion/gcc9900/bin/ea5split

# Flags used during linking
#
# Locate the code (.text section) and the data (.data section)
LDFLAGS_EA5=\
  --section-start .text=2100 --section-start .data=3800 -M

C_FLAGSTI=\
  -O2 -std=c99 -s --save-temp -DBUILD_TI99 -DTI99

INCPATHTI=-I/mnt/d/work/libti99ALL -I../../../../libtivgm2
LIBPATHTI=-L/mnt/d/work/libti99ALL/buildti -L../../../../libtivgm2
LIBSTI=-lti99 -ltivgm2

# Recipe to compile the executable
ti99: piano.c crt0_ea5.asm
	$(GASTI) crt0_ea5.asm -o crt0_ea5.o
	$(CCTI) -c piano.c $(C_FLAGSTI) $(INCPATHTI) -o piano.o
	$(LDTI) crt0_ea5.o piano.o $(LIBSTI) $(LIBPATHTI) $(LDFLAGS_EA5) -o piano.ea5.elf > ea5.map
	$(ELF2EA5) piano.ea5.elf piano.ea5.bin
	$(EA5PLIT) piano.ea5.bin
	$(B2ITI) PIANO1 ../../qpPianoTI.c qpPianoTI

############ Coleco part ###############

# tested with SDCC 4.0.0
# requires libti99coleco
CC = "c:/program files/sdcc/bin/sdcc"
AS = "c:/program files/sdcc/bin/sdasz80"
AR = "c:/program files/sdcc/bin/sdar"
MB = "c:/program files/sdcc/bin/makebin"
DD = "D:/tools/gnuwin32/gnuwin32/bin/dd.exe"
B2I= "D:/tools/bin2inc.exe"
LIBTI99 = "d:/work/coleco/libti99coleco"
RM = del /F 
EXT=rel
IFLAGS = "-I$(LIBTI99)" -I../../../../libcolecovgm2
CFLAGS = -mz80 -c $(IFLAGS) -DBUILD_COLECO --opt-code-size --fsigned-char --max-allocs-per-node 18000 --std-sdcc99 --vc
LFLAGS = --no-std-crt0 -l$(LIBTI99)/libti99.a -l../../../../libcolecovgm2/libcolecovgm2.a
AFLAGS = -plosgff

# this rule links the files into the ihx
# makebin doesn't know you can start at $8000 instead of $0000, so we use dd to trim it
coleco: crt0.s piano.c
	$(AS) $(AFLAGS) crt0.rel crt0.s
	$(CC) $(CFLAGS) -o piano.rel piano.c
	$(CC) -mz80 --code-loc 0x8100 --data-loc 0x7000 $(LFLAGS) crt0.rel piano.rel
	$(MB) -s 65536 crt0.ihx | $(DD) bs=32768 skip=1 of=qpPianoColeco.rom
	$(B2I) qpPianoColeco.rom ..\..\qpPianoColeco.c qpPianoColeco

############ Shared part ###############

# Recipe to clean all compiled objects
clean:
	-rm *.o
	-rm *.elf
	-rm *.map
	-rm *.bin
	-rm *.rel *.map *.lst *.sym *.ihx *.rom


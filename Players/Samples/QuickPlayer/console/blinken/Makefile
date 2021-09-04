all:
	@echo make ti99 or make coleco

# Paths to TMS9900 compilation tools 
# ( Set in environment to override paths )
TMS9900_DIR?=/home/tursilion/gcc9900/bin
ELF2EA5_DIR?=/home/tursilion/gcc9900/bin
EA5_SPLIT_DIR?=/home/tursilion/gcc9900/bin
CLASSIC99_DSK1?=/mnt/d/classic99/DSK1/
LIBTI99TI?=/mnt/d/work/ti/libti99/
CP=/bin/cp
B2ITI=/mnt/d/tools/bin2inc.exe

# Full paths to the executables used
GASTI=$(TMS9900_DIR)/tms9900-as
LDTI=$(TMS9900_DIR)/tms9900-ld
CCTI=$(TMS9900_DIR)/tms9900-gcc
ARTI=$(TMS9900_DIR)/tms9900-ar
ELF2EA5=$(ELF2EA5_DIR)/elf2ea5
EA5_SPLIT=$(EA5_SPLIT_DIR)/ea5split

# Flags used during linking
#
# Locate the code (.text section) and the data (.data section)
LDFLAGS_EA5=\
  --section-start .text=2000 --section-start .data=3800 -M

INCPATHTI=-I$(LIBTI99TI) -I../../../../libtivgm2
LIBPATHTI=-L$(LIBTI99TI) -L. -L../../../../libtivgm2
LIBSTI=-lti99 -ltivgm2

C_FLAGSTI=\
  -O2 -std=c99 -s --save-temp -DBUILD_TI99

# Recipe to compile the executable
ti99: crt0_ea5.asm blinken.c
	$(GASTI) crt0_ea5.asm -o crt0_ea5.o
	$(CCTI) -c blinken.c $(C_FLAGSTI) $(INCPATHTI) -o blinken.o
	$(LDTI) crt0_ea5.o blinken.o $(LIBSTI) $(LIBPATHTI) $(LDFLAGS_EA5) -o blinken.ea5.elf > ea5.map
	$(ELF2EA5) blinken.ea5.elf blinken.ea5.bin
	$(EA5_SPLIT) blinken.ea5.bin
	$(B2ITI) BLINKEN1 ../../qpBlinkenTI.c qpBlinkenTI

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
coleco: crt0.s blinken.c
	$(AS) $(AFLAGS) crt0.rel crt0.s
	$(CC) $(CFLAGS) -o blinken.rel blinken.c
	$(CC) -mz80 --code-loc 0x8050 --data-loc 0x7000 $(LFLAGS) crt0.rel blinken.rel
	$(MB) -s 65536 crt0.ihx | $(DD) bs=32768 skip=1 of=qpBlinkenColeco.rom
	$(B2I) qpBlinkenColeco.rom ..\..\qpBlinkenColeco.c qpBlinkenColeco

############ Shared part ###############

# Recipe to clean all compiled objects
clean:
	-rm *.o
	-rm *.elf
	-rm *.map
	-rm *.bin
	-rm *.rel *.map *.lst *.sym *.ihx *.rom


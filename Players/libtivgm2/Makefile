# Paths to TMS9900 compilation tools 
# ( Set in environment to override paths )
TMS9900_DIR?=/home/tursilion/newtms9900-gcc/newgcc9900/bin
ELF2EA5_DIR?=/home/tursilion/gcc9900/bin
EA5_SPLIT_DIR?=/home/tursilion/gcc9900/bin
CLASSIC99_DSK1?=/mnt/d/classic99/DSK1/
LIBTI99?=/mnt/d/work/ti/libti99/

# Full paths to the executables used
GAS=$(TMS9900_DIR)/tms9900-as
LD=$(TMS9900_DIR)/tms9900-ld
CC=$(TMS9900_DIR)/tms9900-gcc
AR=$(TMS9900_DIR)/tms9900-ar
ELF2EA5=$(ELF2EA5_DIR)/elf2ea5
EA5_SPLIT=$(EA5_SPLIT_DIR)/ea5split
CP=/bin/cp

LDFLAGS_EA5=\
  --section-start .text=a000 --section-start .data=2080 -M

OBJECT_LIST_EA5=\
  crt0_ea5.o\

# output file
NAME=libtivgm2.a

C_FLAGS=-O2 -std=c99 -s --save-temp -I./ -fno-builtin -DBUILD_TI99
LIBPATH=-L$(LIBTI99) -L.
INCPATH=-I$(LIBTI99) -I../CPlayer

# List of compiled objects used in executable
OBJECT_LIST=\
  CPlayerCommonHandEdit.o	\
  CPlayerTIHandEdit.o	\
  CPlayerTI30Hz.o   \
  CPlayerTIHandlers.o   \
  CPlayerTISfx.o	\
  CPlayerTISfx30Hz.o    \
  CPlayerTISfxHandlers.o \
  CSIDPlayTIHandEdit.o	\
  CSIDPlayTI30Hz.o \
  CSIDPlayTIHandlers.o \
  CPlayerTest.o

# Recipe to compile the library and samples
all: library samples

library: $(OBJECT_LIST)
	$(AR) rcs $(NAME) $(OBJECT_LIST)
	
# build the samples	
.phony samples: tisn tisid tisnsfx tisnsid

tisn: library
	make -C ../Samples/TISN/ tisnplay

tisid: library
	make -C ../Samples/TISID/ tisidplay

tisnsfx: library
	make -C ../Samples/TISNSFX/ tisfxplay

tisnsid: library
	 make -C ../Samples/TISNSID/ tisnsidplay

# Recipe to clean all compiled objects
.phony clean:
	-rm *.o || true
	-rm *.a || true
	-rm *.s || true
	-rm *.i || true
	-rm *.elf || true
	-rm *.map || true
	-rm *.bin || true
	-make -C ../Samples/TISN/ clean
	-make -C ../Samples/TISID/ clean
	-make -C ../Samples/TISNSFX/ clean
	-make -C ../Samples/TISNSID/ clean

# Recipe for imported file
CPlayerTest.o: ../CPlayer/CPlayerTest.c
	$(CP) ../CPlayer/CPlayerTest.c .
	$(CC) -c CPlayerTest.c $(INCPATH) $(C_FLAGS) -o CPlayerTest.o

# Recipe to compile all assembly files
%.o: %.asm
	$(GAS) $< -o $@

# Recipe to compile all C files
%.o: %.c
	$(CC) -c $< $(INCPATH) $(C_FLAGS) -o $@

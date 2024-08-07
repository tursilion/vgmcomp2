# Paths to TMS9900 compilation tools 
# ( Set in environment to override paths )
TMS9900_DIR?=/home/tursilion/newtms9900-gcc/newgcc9900/bin
ELF2EA5_DIR?=/home/tursilion/gcc9900/bin
EA5_SPLIT_DIR?=/home/tursilion/gcc9900/bin
CLASSIC99_DSK1?=/mnt/d/classic99/DSK1/
LIBTI99?=/mnt/d/work/libti99ALL/

# Full paths to the executables used
GAS=$(TMS9900_DIR)/tms9900-as
LD=$(TMS9900_DIR)/tms9900-ld
CC=$(TMS9900_DIR)/tms9900-gcc
CP=/bin/cp
NAME=tisfxplay

# Path to elf2cart conversion utility
ELF2CART=~/gcc9900/bin/elf2cart
ELF2EA5=~/gcc9900/bin/elf2ea5
EA5PLIT=~/gcc9900/bin/ea5split

# Flags used during linking
#
# Locate the code (.text section) and the data (.data section)
LDFLAGS_EA5=\
  --section-start .text=a000 --section-start .data=2080 -M

INCPATH=-I$(LIBTI99) -I../../libtivgm2
LIBPATH=-L$(LIBTI99)/buildti -L../../libtivgm2
LIBS=-ltivgm2 -lti99

# here we define the SN PSH and TI99 targets for CPlayer
C_FLAGS=\
  -O2 -std=c99 -s --save-temp -fno-builtin -fno-function-cse -DUSE_SN_PSG -DBUILD_TI99 -DTI99

# List of compiled objects used in executable
OBJECT_LIST_EA5=\
  crt0_ea5.o

OBJECT_LIST=\
  demomusic.o \
  snsfxplay.o

# List of all files needed in executable
PREREQUISITES=\
  $(OBJECT_LIST_EA5) $(OBJECT_LIST)
  
all: tisfxplay
	-/mnt/d/work/classic99paste/release/classic99paste.exe -reset xx25DSK0.TISFXPLAY1\\n || true

# Recipe to compile the executable
tisfxplay: $(PREREQUISITES)
	$(LD) $(OBJECT_LIST_EA5) $(OBJECT_LIST) $(LIBS) $(LIBPATH) $(LDFLAGS_EA5) -o $(NAME).ea5.elf > ea5.map
	$(ELF2EA5) $(NAME).ea5.elf $(NAME).ea5.bin
	$(EA5PLIT) $(NAME).ea5.bin
	$(CP) TISFXPLA* /mnt/d/classic99/dsk1/

split:
	$(EA5PLIT) $(NAME).ea5.bin
	$(ELF2CART) $(NAME).c.elf $(NAME).c.bin
	$(CP) TISFXPLAY* /mnt/d/classic99/dsk1/

# Recipe to clean all compiled objects
.phony clean:
	-rm *.o || true
	-rm *.i || true
	-rm *.s || true
	-rm *.lst || true
	-rm *.elf || true
	-rm *.map || true
	-rm *.bin || true
	-rm TISFXPLAY1 || true
	-rm TISFXPLAY2 || true
	-rm TISFXPLAY3 || true

# Recipe to compile all assembly files
%.o: %.asm
	$(GAS) $< -o $@

# Recipe to compile all C files
%.o: %.c
	$(CC) -c $< $(C_FLAGS) $(INCPATH) -o $@

# Paths to TMS9900 compilation tools
GAS=~/gcc9900/bin/tms9900-as
LD=~/gcc9900/bin/tms9900-ld
CC=~/gcc9900/bin/tms9900-gcc
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

INCPATH=-I/mnt/d/work/ti/libti99 -I../../libtivgm2
LIBPATH=-L/mnt/d/work/ti/libti99 -L../../libtivgm2
LIBS=-ltivgm2 -lti99

# here we define the SN PSH and TI99 targets for CPlayer
C_FLAGS=\
  -O2 -std=c99 -s --save-temp -DUSE_SN_PSG -DBUILD_TI99

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

#note: no cart in this one

# Paths to TMS9900 compilation tools 
# ( Set in environment to override paths )
TMS9900_DIR?=/home/tursilion/gcc9900/bin
ELF2EA5_DIR?=/home/tursilion/gcc9900/bin
EA5_SPLIT_DIR?=/home/tursilion/gcc9900/bin
CLASSIC99_DSK1?=/mnt/d/classic99/DSK1/
LIBTI99?=/mnt/d/work/ti/libti99/
CP=/bin/cp
B2I=/mnt/d/tools/bin2inc.exe

# Full paths to the executables used
GAS=$(TMS9900_DIR)/tms9900-as
LD=$(TMS9900_DIR)/tms9900-ld
CC=$(TMS9900_DIR)/tms9900-gcc
AR=$(TMS9900_DIR)/tms9900-ar
ELF2EA5=$(ELF2EA5_DIR)/elf2ea5
EA5_SPLIT=$(EA5_SPLIT_DIR)/ea5split

# output file
NAME=ANI

# Flags used during linking
#
# Locate the code (.text section) and the data (.data section)
LDFLAGS_EA5=\
  --section-start .text=2100 --section-start .data=3800 -M

INCPATH=-I$(LIBTI99) -I../../../../libtivgm2
LIBPATH=-L$(LIBTI99) -L. -L../../../../libtivgm2
LIBS=-lti99 -ltivgm2

C_FLAGS=\
  -O2 -std=c99 -s --save-temp

# List of compiled objects used in executable
OBJECT_LIST_EA5=\
  crt0_ea5.o

OBJECT_LIST=\
  main.o

# List of all files needed in executable
PREREQUISITES=\
  $(OBJECT_LIST_EA5) $(OBJECT_LIST) ani1.c
  
all: ani1
	$(B2I) ANI1 ../../qpballsTI.c qpballs


# Recipe to compile the executable
ani1: $(PREREQUISITES)
	$(LD) $(OBJECT_LIST_EA5) $(OBJECT_LIST) $(LIBS) $(LIBPATH) $(LDFLAGS_EA5) -o $(NAME).ea5.elf > ea5.map
	$(ELF2EA5) $(NAME).ea5.elf $(NAME).ea5.bin
	$(EA5_SPLIT) $(NAME).ea5.bin
	$(CP) ANI* $(CLASSIC99_DSK1)

# Recipe to clean all compiled objects
.phony clean:
	rm *.o || true
	rm *.elf || true
	rm *.map || true
	rm *.bin || true

# Recipe to compile all assembly files

%.o: %.asm
	$(GAS) $< -o $@

# Recipe to compile all C files
%.o: %.c
	$(CC) -c $< $(C_FLAGS) $(INCPATH) -o $@

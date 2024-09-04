# path to gcc arm
GCCPATH=d:/work/devkitPro/devkitARM/bin

# gba specific
GBAFIX=$(GCCPATH)/../../tools/bin/gbafix.exe
GBASPECS=$(GCCPATH)/../arm-none-eabi/lib/gba.specs

# Full paths to the executables used
GAS=$(GCCPATH)/arm-none-eabi-as.exe
LD=$(GCCPATH)/arm-none-eabi-ld.exe
CC=$(GCCPATH)/arm-none-eabi-gcc.exe
AR=$(GCCPATH)/arm-none-eabi-ar.exe
OBJCOPY=$(GCCPATH)/arm-none-eabi-objcopy.exe

# specific assembly files
OBJECT_LIST_CRT=\
  intwrap.o

# source files
OBJECT_LIST=\
  CPlayerCommon.o	\
  GBAPlayerSFX.o   \
  GBAPlayerSN.o	\
  CPlayerTest.o

# output file
NAME=libgbavgm2.a

C_FLAGS=\
  -g -Wall -O2 -Xlinker -Map=output.map\
		-mcpu=arm7tdmi -mtune=arm7tdmi -fsigned-char \
		-save-temps -funroll-loops \
		-fsigned-char -DBUILD_GBA \
		-DPRINTF_DISABLE_SUPPORT_FLOAT -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL \
		-DPRINTF_DISABLE_SUPPORT_LONG_LONG -DPRINTF_DISABLE_SUPPORT_PTRDIFF_T \
		-DUSE_SN_PSG -DGBA\
		-mthumb -mthumb-interwork

LDFLAGS	= -g -mthumb -mthumb-interwork -Wl,-Map,$(notdir $*.map)

INCLUDES = -I../ -I../../CPlayer

# Recipe to compile the library
all: library test.gba

library: $(OBJECT_LIST)
	$(AR) rcs $(NAME) $(OBJECT_LIST)

test.gba: test.elf
	$(OBJCOPY) -O binary $< $@
	$(GBAFIX) $@

test.elf: library gbasnplaytest.o outfile1.o sil_nes.o $(OBJECT_LIST_CRT) $(OBJECT_LIST)
	$(CC) $(OBJECT_LIST_CRT) $(OBJECT_LIST) $(LDFLAGS) gbasnplaytest.o outfile1.o sil_nes.o -specs=$(GBASPECS) -L./ -lgbavgm2 -o test.elf > map_testlib.map

# Recipe to compile all assembly files
%.o: ../%.asm
	$(GAS) -g -mthumb -mthumb-interwork $< -o $@

# Recipe to compile all C files
%.o: ../%.c
	$(CC) -c $< $(C_FLAGS) $(INCLUDES) -o $@
%.o: ../../CPlayer/%.c
	$(CC) -c $< $(C_FLAGS) $(INCLUDES) -o $@

# clean
.PHONY: clean
clean:
	rm *.s
	rm *.i
	rm *.o
	rm *.elf

    
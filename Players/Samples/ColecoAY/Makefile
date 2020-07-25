# tested with SDCC 4.0.0
# requires libti99coleco

CC = "c:/program files/sdcc/bin/sdcc"
AS = "c:/program files/sdcc/bin/sdasz80"
AR = "c:/program files/sdcc/bin/sdar"
MB = "c:/program files/sdcc/bin/makebin"
DD = "D:/tools/gnuwin32/gnuwin32/bin/dd.exe"
LIBTI99 = "d:/work/coleco/libti99coleco"
RM = del /F 
EXT=rel
IFLAGS = "-I$(LIBTI99)" -I../../libcolecovgm2
CFLAGS = -mz80 -c $(IFLAGS) -DBUILD_COLECO --opt-code-size --max-allocs-per-node 18000 --std-sdcc99 --vc
LFLAGS = --no-std-crt0 -l$(LIBTI99)/libti99.a -l../../libcolecovgm2/libcolecovgm2.a
AFLAGS = -plosgff

.PHONY: all clean

# crt0 must be first! (note this is also why the output is named crt0.ihx)
# list all your object files in this variable
objs = crt0.rel colecoayplay.rel ab_ay.rel

# makebin doesn't know you can start at $8000 instead of $0000, so we use dd to trim it
all: buildcoleco
	$(MB) -s 65536 crt0.ihx | $(DD) bs=32768 skip=1 of=playerAY.rom

# this rule links the files into the ihx
buildcoleco: $(objs)
	$(CC) -mz80 --code-loc 0x8100 --data-loc 0x7000 $(LFLAGS) $(objs)

# clean up the build
clean:
	-rm *.rel *.map *.lst *.sym *.asm *.ihx *.rom

# Recipe to compile all assembly files
%.rel: %.s
	$(AS) $(AFLAGS) $@ $<

# Recipe to compile all C files
%.rel: %.c
	$(CC) $(CFLAGS) -o $@ $<

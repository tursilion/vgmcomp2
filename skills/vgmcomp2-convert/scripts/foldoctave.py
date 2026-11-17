# foldoctave.py - move out-of-range notes by whole octaves until they fit, keeping the pitch class
# usage: python foldoctave.py <max counter> [-min <min counter>] <channel file>
#   max counter: SN 0x3ff, AY 0xfff (notes with a larger counter are too LOW - folded up)
#   -min:        smallest counter allowed (notes below are too HIGH - folded down);
#                for SID output use about 29 (0x1d)
# Counters are in the 60hz file units (SN shift counts) for every chip.
# cliptones clamps to the limit instead, which changes the note; this keeps the pitch class.
# Only rows with volume are changed. Original file is renamed with .fold.old extension.
import sys, os
args = sys.argv[1:]
minc = 1
if '-min' in args:
    i = args.index('-min'); minc = int(args[i + 1], 0); del args[i:i + 2]
maxc, fn = int(args[0], 0), args[1]
rows = [l.strip().split(',') for l in open(fn)]
os.replace(fn, fn + '.fold.old')
up = down = 0
with open(fn, 'w', newline='\r\n') as fp:
    for f, v in rows:
        c = int(f, 0)
        flags, cnt = c & 0xffff0000, c & 0xffff
        if int(v, 0) > 0 and cnt:
            if cnt > maxc:
                while cnt > maxc: cnt = (cnt + 1) // 2
                up += 1
            elif cnt < minc:
                while cnt < minc: cnt *= 2
                down += 1
        fp.write('0x%08X,%s\n' % (flags | cnt, v))
print('%d notes folded up, %d folded down into range' % (up, down))

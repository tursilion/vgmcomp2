# foldoctave.py - raise notes below the chip's range by whole octaves until they fit
# usage: python foldoctave.py <max counter> <channel file>
# (cliptones clamps to the limit, which changes the note; this keeps the pitch class)
# Original file is renamed with .fold.old extension.
import sys, os
maxc, fn = int(sys.argv[1], 0), sys.argv[2]
rows = [l.strip().split(',') for l in open(fn)]
os.replace(fn, fn + '.fold.old')
folded = 0
with open(fn, 'w', newline='\r\n') as fp:
    for f, v in rows:
        c = int(f, 0)
        flags, cnt = c & 0xffff0000, c & 0xffff
        if cnt > maxc and int(v, 0) > 0:
            while cnt > maxc: cnt = (cnt + 1) // 2
            folded += 1
        fp.write('0x%08X,%s\n' % (flags | cnt, v))
print('%d notes folded up into range' % folded)

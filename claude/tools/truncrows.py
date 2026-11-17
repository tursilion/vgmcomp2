# truncrows.py - cut channel files to a fixed number of rows (e.g. one pass of a looping SID)
# usage: python truncrows.py <rows> <file> [<file> ...]
import sys
rows = int(sys.argv[1])
for fn in sys.argv[2:]:
    lines = open(fn, newline='').readlines()
    if len(lines) > rows:
        open(fn, 'w', newline='').writelines(lines[:rows])
        print('%s: %d -> %d rows' % (fn, len(lines), rows))

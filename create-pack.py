#!/usr/bin/python

import os
import sys

fout = open('kaal.dat', 'w')
fout.write('POKE 59458,62\n')
listing = sys.argv[1:]
listing.sort()
for fname in listing:
	fout.write('%s %d\n' % (os.path.basename(fname), os.path.getsize(fname)))
fout.write('JMP FFFF:0000\n')
for fname in listing:
	f = open(fname)
	while True:
		buf = f.read(4096)
		if not buf: break
		fout.write(buf)
	f.close()
fout.close()

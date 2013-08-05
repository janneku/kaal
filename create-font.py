#!/usr/bin/python
"""
  KAAL

  Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
  Antti Rajamaki <amikaze@gmail.com>

  Program code and resources are licensed with GNU LGPL 2.1. See
  lgpl-2.1.txt file.
"""
import cairo
import sys
import math
import os

if len(sys.argv) < 4:
	print 'Usage: %s [-shadow] [font] [size] [output.fnt]' % sys.argv[0]
	sys.exit(0)

def power2(x):
	return 1 << int(math.ceil(math.log(x, 2)))

shadow = (sys.argv[1] == '-shadow')
padding = 1
if shadow:
	padding = 2
	sys.argv.pop(1)

surface = cairo.ImageSurface(cairo.FORMAT_ARGB32, 1, 1)
cr = cairo.Context(surface)
cr.select_font_face(sys.argv[1], cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
cr.set_font_size(float(sys.argv[2]))

f = open(sys.argv[3], 'w')
png_fname = sys.argv[3].replace('.fnt', '.png')
f.write('texture %s\n' % os.path.basename(png_fname))

total_width = 0
total_height = 0
for i in range(32, 127):
	width, height = cr.text_extents(chr(i))[2:4]
	total_width += width + padding*2
	total_height = max(total_height, height + padding*2)

surface = cairo.ImageSurface(cairo.FORMAT_ARGB32,
			     power2(total_width), power2(total_height))
cr = cairo.Context(surface)
cr.select_font_face(sys.argv[1], cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_NORMAL)
cr.set_font_size(float(sys.argv[2]))
x = 0
for i in range(32, 127):
	x_bearing, y_bearing, width, height, x_advance, y_advance = \
		cr.text_extents(chr(i))
	f.write('glyph %d %d %d %d %d %d %d\n' %
		(i, x_bearing - padding, y_bearing - padding,
		 width + padding*2, height + padding*2,
		 x_advance, y_advance))
	if shadow:
		cr.set_source_rgb(0.0, 0.0, 0.0)
		cr.set_line_width(2)
		cr.move_to(x - x_bearing + padding, -y_bearing + padding)
		cr.text_path(chr(i))
		cr.stroke()

	cr.set_source_rgb(1.0, 1.0, 1.0)
	cr.move_to(x - x_bearing + padding, -y_bearing + padding)
	cr.show_text(chr(i))
	x += width + padding*2
surface.write_to_png(png_fname)

f.close()

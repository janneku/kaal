/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#ifndef __utils_h__
#define __utils_h__

#define DISALLOW_COPY_AND_ASSIGN(x) \
private: \
	x(const x &from); \
	x &operator = (const x &from)

#define lengthof(x)	\
	(sizeof(x) / sizeof(x[0]))

#endif

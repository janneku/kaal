/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#ifndef __vec3_h__
#define __vec3_h__

#include <math.h>
#include <algorithm>

class vec3 {
public:
	float x, y, z;

	vec3() {}
	vec3(float i_x, float i_y, float i_z) :
		x(i_x),
		y(i_y),
		z(i_z)
	{
	}
};

class vec2 {
public:
	float x, y;

	vec2() {}
	vec2(float i_x, float i_y) :
		x(i_x),
		y(i_y)
	{
	}
};

class Matrix {
public:
	vec3 forward;
	vec3 right;
	vec3 up;

	Matrix() {}
	Matrix(const vec3 &i_forward, const vec3 &i_right, const vec3 &i_up) :
		forward(i_forward),
		right(i_right),
		up(i_up)
	{
	}
};

struct Plane {
	vec3 norm;
	double pos;
};

struct CollFace {
	vec3 vert[3], edges[3];
	vec3 norm;
};

static inline double dot(const vec3 &a, const vec3 &b)
{
	return (double) a.x*b.x + (double) a.y*b.y + (double) a.z*b.z;
}

static inline vec3 operator + (const vec3 &a, const vec3 &b)
{
	return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline vec3 operator - (const vec3 &a, const vec3 &b)
{
	return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline vec3 operator * (const vec3 &v, double s)
{
	return vec3(v.x * s, v.y * s, v.z * s);
}

/* Test whether the length of a given vector is less than l */
static inline bool operator < (const vec3 &v, double l)
{
	return dot(v, v) < l*l;
}

/* Test whether the length of a given vector is greater than l */
static inline bool operator > (const vec3 &v, double l)
{
	return dot(v, v) > l*l;
}

static inline void operator += (vec3 &a, const vec3 &b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
}

static inline void operator -= (vec3 &a, const vec3 &b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
}

static inline void operator *= (vec3 &v, double s)
{
	v.x *= s;
	v.y *= s;
	v.z *= s;
}

static inline bool operator == (const vec3 &a, const vec3 &b)
{
	vec3 delta = b - a;
	return fabs(dot(delta, delta)) < 1e-8;
}

static inline double length(const vec3 &v)
{
	return sqrt(dot(v, v));
}

static inline vec3 min(const vec3 &a, const vec3 &b)
{
	return vec3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}

static inline vec3 max(const vec3 &a, const vec3 &b)
{
	return vec3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

static inline vec3 abs(const vec3 &v)
{
	return vec3(fabs(v.x), fabs(v.y), fabs(v.z));
}

static inline vec2 operator + (const vec2 &a, const vec2 &b)
{
	return vec2(a.x + b.x, a.y + b.y);
}

static inline vec2 operator - (const vec2 &a, const vec2 &b)
{
	return vec2(a.x - b.x, a.y - b.y);
}

static inline vec2 operator * (const vec2 &v, double s)
{
	return vec2(v.x * s, v.y * s);
}

static inline bool operator == (const Matrix &a, const Matrix &b)
{
	return a.forward == b.forward &&
	       a.right == b.right &&
	       a.up == b.up;
}

vec3 cross(const vec3 &a, const vec3 &b);
vec3 normalize(const vec3 &v);
vec3 transform(const vec3 &v, const Matrix &m);
vec3 reverse_transform(const vec3 &v, const Matrix &m);

/* Construct a matrix from vectors that are not orthological */
Matrix look_at(const vec3 &forward, const vec3 &up);

bool inside(const CollFace &f, const vec3 &pos);
bool hittest(const CollFace &f, const vec3 &pos, double rad, vec3 *contact);

#endif

/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include "vec3.h"

vec3 cross(const vec3 &a, const vec3 &b)
{
	return vec3(a.y*b.z - a.z*b.y,
		    a.z*b.x - a.x*b.z,
		    a.x*b.y - a.y*b.x);
}

vec3 normalize(const vec3 &v)
{
	double l = length(v);
	if (fabs(l) < 1e-6) {
		return vec3(0, 0, 0);
	}
	return v * (1 / l);
}

vec3 transform(const vec3 &v, const Matrix &m)
{
	return m.right * v.x + m.up * v.y + m.forward * -v.z;
}

Matrix look_at(const vec3 &forward, const vec3 &up)
{
	vec3 unit_forward = normalize(forward);
	vec3 unit_right = normalize(cross(forward, up));
	vec3 unit_up = cross(unit_right, unit_forward);
	return Matrix(unit_forward, unit_right, unit_up);
}

vec3 reverse_transform(const vec3 &v, const Matrix &m)
{
	return vec3(dot(v, m.right), dot(v, m.up), -dot(v, m.forward));
}

bool inside(const CollFace &f, const vec3 &pos)
{
	/* We don't care what order the vertices are in. */
	int a = 0, b = 0;
	for (int i = 0; i < 3; ++i) {
		double dist = dot(pos - f.vert[i], f.edges[i]);
		if (dist > -1e-4) {
			a++;
		}
		if (dist < 1e-4) {
			b++;
		}
	}
	return a == 3 || b == 3;
}

bool hittest(const CollFace &f, const vec3 &pos, double rad, vec3 *contact)
{
	/* Early out - If the position is too far away from the plane */
	double dist_plane = dot(pos - f.vert[0], f.norm);
	if (fabs(dist_plane) > rad) return false;

	int a = 0, b = 0;
	double nearest = rad*rad;
	bool found = false;

	for (int i = 0; i < 3; ++i) {
		double dist_edge = dot(pos - f.vert[i], f.edges[i]);
		if (dist_edge > -1e-4) {
			a++;
		}
		if (dist_edge < 1e-4) {
			b++;
		}
		if (fabs(dist_edge) < rad) {
			/* Touches edge of the face - calculate what is the
			 * nearest point on the edge.
			 */
			vec3 edge = f.vert[(i + 1) % 3] - f.vert[i];
			double x = dot(pos - f.vert[i], edge) /
				   dot(edge, edge);
			x = std::max(std::min(x, 1.0), 0.0);
			vec3 delta = pos - (f.vert[i] + edge * x);
			if (dot(delta, delta) < nearest) {
				nearest = dot(delta, delta);
				if (contact != NULL) {
					*contact = delta;
				}
				found = true;
			}
		}
	}
	if (a == 3 || b == 3) {
		/* Inside */
		if (contact != NULL) {
			*contact = f.norm * dist_plane;
		}
		found = true;
	}
	return found;
}


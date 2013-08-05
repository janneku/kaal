/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#ifndef __world_h
#define __world_h

#include "gfx.h"
#include <unordered_set>

struct Camera {
	Matrix matrix;
	Plane frustum[5];
	vec3 pos;
};

class World;
class Model;
struct Material;

/* NOTE, can not be moved in memory once registered to the world */
class Light {
public:
	vec3 pos() const { return m_pos; }
	double brightness() const { return m_brightness; }

	Light();

	void set_color(const Color &c);
	void move(const vec3 &p);
	void set_brightness(double v);
	void program(int n) const;
	void set_world(World *world);

private:
	Color m_color;
	vec3 m_pos;
	double m_brightness;
	World *m_world;
};

/* NOTE, can not be moved in memory once registered to the world */
class Object {
public:
	vec3 pos() const { return m_pos; }
	const Matrix &matrix() const { return m_matrix; }
	vec3 vel() const { return m_vel; }
	World *world() const { return m_world; }
	vec3 offset() const { return m_offset; }
	bool grounded() const { return m_grounded; }
	bool lights_on() const { return m_lights_on; }
	double anim() const { return m_anim; }
	const Model *model() const { return m_model; }

	Object();

	void replace_material(const char *name, const Material *mat);
	void set_model(const Model *model, const Model *lowres = NULL,
			const vec3 &offset = vec3(0, 0, 0));
	void move(const vec3 &p);
	void set_matrix(const Matrix &m);
	void render(const Camera &camera, int flags, size_t counter,
		    const Color &ambient);
	bool raytrace(const vec3 &pos, const vec3 &ray, double *dist) const;
	void advance(double dt, double rad);
	void trust(const vec3 &accel);
	void set_anim(double anim);
	void set_lights_on(bool on);
	void update_lights();
	void set_world(World *world);
	bool visible(const Camera &camera, size_t counter);

private:
	vec3 m_pos;
	vec3 m_vel;
	Matrix m_matrix;
	vec3 m_offset;
	const Model *m_model;
	const Model *m_lowres;
	size_t m_visited;
	bool m_grounded;
	std::list<Light> m_lights;
	std::string m_replace_name;
	const Material *m_replace_mat;
	double m_anim;
	bool m_lights_on;
	World *m_world;
	vec3 m_world_pos;
};

class World {
public:
	struct Tree {
		vec3 box_min, box_max;
		Model model;
		std::list<Object *> objects;
		std::vector<Light *> remote_lights;
		std::list<Light> lights;
		Plane plane;
		Tree *children[2];
		bool sorted;
	};

	World();
	void set_ambient(const Color &c);
	void render(const Camera &camera, int flags);
	void sweep(const Camera &camera, std::unordered_set<Object *> *objs) const;
	void load(const char *fname);
	void register_lights();
	bool raytrace(const vec3 &pos, const vec3 &ray,
		      double *dist, const CollFace **face = NULL,
		      Object **obj = NULL, const Tree *tree = NULL) const;
	Material *get_material(const char *name);
	bool find_collisions(const vec3 &pos, double rad,
			     std::list<const CollFace *> *faces,
			     std::list<Object *> *objs,
			     const std::unordered_set<const void *> &ignore);
	void register_light(Light *light, bool add);
	void register_obj(Object *obj, const vec3 &pos, double rad, bool add);
	void remove_all_objects();

private:
	void build_leaf(Tree *tree, const Mesh *mesh,
			const std::list<Face> &faces);
	void split_faces(const std::list<Face> &faces, const Tree *tree,
			 int side, std::list<Face> *out);
	void build_tree(const Mesh *mesh, int max_depth);
	void render(Tree *tree, const Camera &camera, int flags);

	Color m_ambient;
	Tree m_root;
	std::unordered_map<std::string, Material *> m_materials;
};

void prepare_camera(Camera *camera, double fov_x, double fov_y, double dist);

#endif


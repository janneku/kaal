/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#include "world.h"
#include "gfx.h"
#include "system.h"

namespace {

size_t visibility_test;

}

Light::Light() :
	m_color(1, 1, 1),
	m_pos(0, 0, 0),
	m_brightness(0),
	m_world(NULL)
{
}

void Light::move(const vec3 &p)
{
	if (p == m_pos) return;

	/* Do the dance, baby */
	if (m_world != NULL) {
		m_world->register_light(this, false);
	}
	m_pos = p;
	if (m_world != NULL) {
		m_world->register_light(this, true);
	}
}

void Light::set_brightness(double v)
{
	if (v == m_brightness) return;

	if (m_world != NULL) {
		m_world->register_light(this, false);
	}
	m_brightness = v;
	if (m_world != NULL) {
		m_world->register_light(this, true);
	}
}

void Light::set_color(const Color &c)
{
	m_color = c;
}

void Light::program(int n) const
{
	assert(m_world != NULL);
	set_light(n, m_pos, m_color, m_brightness);
}

/* Setting the world to NULL unregisters the light from the world, making it
 * possible to free it!
 */
void Light::set_world(World *world)
{
	if (world == m_world) return;

	if (m_world != NULL) {
		m_world->register_light(this, false);
	}
	m_world = world;
	if (m_world != NULL) {
		m_world->register_light(this, true);
	}
}


Object::Object() :
	m_pos(0, 0, 0),
	m_vel(0, 0, 0),
	m_matrix(vec3(1, 0, 0), vec3(0, 1, 0), vec3(0, 0, 1)),
	m_model(NULL),
	m_visited(0),
	m_anim(0),
	m_lights_on(true),
	m_world(NULL)
{
}

void Object::update_lights()
{
	std::list<Model::Light> lights;
	if (m_world != NULL && m_model != NULL && m_lights_on) {
		if (!m_replace_name.empty()) {
			*m_model->get_material(m_replace_name.c_str()) = *m_replace_mat;
		}
		lights = m_model->get_lights();
	}
	auto iter = m_lights.begin();
	for (const Model::Light &l : lights) {
		Light *light = NULL;
		if (iter == m_lights.end()) {
			m_lights.push_back(Light());
			light = &m_lights.back();
			iter = m_lights.end();
		} else {
			light = &*iter;
			iter++;
		}
		light->set_color(l.mat->light_color);
		light->move(m_pos + m_offset +
			    transform(l.pos, m_matrix));
		light->set_brightness(l.mat->brightness);
		light->set_world(m_world);
	}
	/* Remove left over lights */
	while (iter != m_lights.end()) {
		Light *light = &*iter;
		auto this_iter = iter;
		iter++;

		light->set_world(NULL);
		m_lights.erase(this_iter);
	}
}

void Object::replace_material(const char *name, const Material *mat)
{
	m_replace_name = name;
	m_replace_mat = mat;
}

void Object::set_model(const Model *model, const Model *lowres, const vec3 &offset)
{
	if (m_world != NULL && m_model != NULL) {
		m_world->register_obj(this, m_world_pos, m_model->rad(), false);
	}
	m_model = model;
	m_lowres = lowres;
	m_offset = offset;

	if (m_world != NULL && m_model != NULL) {
		m_world_pos = m_pos + m_offset +
			      transform(m_model->midpos(), m_matrix);
		m_world->register_obj(this, m_world_pos, m_model->rad(), true);
	}
	update_lights();
}

/* Setting the world to NULL unregisters the object from the world, making it
 * possible to free it!
 */
void Object::set_world(World *world)
{
	if (world == m_world) return;

	if (m_world != NULL && m_model != NULL) {
		m_world->register_obj(this, m_world_pos, m_model->rad(), false);
	}
	m_world = world;
	if (m_world != NULL && m_model != NULL) {
		m_world_pos = m_pos + m_offset +
			      transform(m_model->midpos(), m_matrix);
		m_world->register_obj(this, m_world_pos, m_model->rad(), true);
	}
	update_lights();
}

bool Object::visible(const Camera &camera, size_t counter)
{
	assert(m_world != NULL && m_model != NULL);

	if (m_visited == counter) return false;
	m_visited = counter;

	for (int i = 0; i < 5; ++i) {
		if (dot(m_world_pos, camera.frustum[i].norm) <
		    camera.frustum[i].pos - m_model->rad()) {
			return false;
		}
	}
	return true;
}

/* "counter" is version number which is used to avoid drawing objects twice.
 * Objects with the given value are ignored, while older ones are drawn.
 */
void Object::render(const Camera &camera, int flags, size_t counter,
		    const Color &ambient)
{
	if (!visible(camera, counter)) return;

	if (m_lights_on) {
		flags |= RENDER_LIGHTS_ON;
	}

	vec3 delta = m_pos + m_offset - camera.pos;

	vec3 pos = m_pos + m_offset;
	glLoadIdentity();
	glTranslatef(pos.x, pos.y, pos.z);
	mult_matrix(m_matrix);
	if (delta > 100 && m_lowres != NULL) {
		/* The object is very far away - Draw lowpoly version */
		if (!m_replace_name.empty()) {
			*m_lowres->get_material(m_replace_name.c_str()) =
				*m_replace_mat;
		}
		m_lowres->render(flags, m_anim, ambient);
	} else {
		if (!m_replace_name.empty()) {
			*m_model->get_material(m_replace_name.c_str()) =
				*m_replace_mat;
		}
		m_model->render(flags, m_anim, ambient);
	}
}

bool Object::raytrace(const vec3 &pos, const vec3 &ray, double *dist) const
{
	assert(m_world != NULL);
	const Model *model = (m_lowres != NULL) ? m_lowres : m_model;

	/* Quickly check against the radius of the model */
	double model_dist = std::min(dot(m_world_pos - pos, ray) / dot(ray, ray),
				     *dist);
	vec3 off = m_world_pos - (pos + ray * model_dist);
	if (model_dist < 0 || off > m_model->rad()) {
		return false;
	}

	return model->raytrace(
			reverse_transform(pos - (m_pos + m_offset), m_matrix),
			reverse_transform(ray, m_matrix), m_anim, dist);
}

void Object::move(const vec3 &p)
{
	m_vel = vec3(0, 0, 0);

	if (p == m_pos) return;

	if (m_world != NULL && m_model != NULL) {
		m_world->register_obj(this, m_world_pos, m_model->rad(), false);
	}
	m_pos = p;
	if (m_world != NULL && m_model != NULL) {
		m_world_pos = m_pos + m_offset +
			      transform(m_model->midpos(), m_matrix);
		m_world->register_obj(this, m_world_pos, m_model->rad(), true);
	}
	update_lights();
}

void Object::set_matrix(const Matrix &m)
{
	if (m == m_matrix) return;

	if (m_world != NULL && m_model != NULL) {
		m_world->register_obj(this, m_world_pos, m_model->rad(), false);
	}
	m_matrix = m;
	if (m_world != NULL && m_model != NULL) {
		m_world_pos = m_pos + m_offset +
			      transform(m_model->midpos(), m_matrix);
		m_world->register_obj(this, m_world_pos, m_model->rad(), true);
	}
	update_lights();
}

void Object::trust(const vec3 &accel)
{
	m_vel += accel;
}

/* TODO: Perhaps make visible things and colliding things separate objects */
void Object::advance(double dt, double rad)
{
	std::unordered_set<const void *> collided;
	vec3 move = m_vel * dt;

	if (m_world != NULL && m_model != NULL) {
		m_world->register_obj(this, m_world_pos, m_model->rad(), false);
	}
	m_grounded = false;

	bool friction = false;
	int steps = 0;
	while (move > 1e-6 && steps < 50) {
		double left = 0;
		double right = 1 / length(move);
		if (right > 1) right = 1;

		vec3 pos = m_pos + move * right;
		std::list<const CollFace *> faces;
		std::list<Object *> objs;
		if (m_world->find_collisions(pos, rad, &faces, &objs, collided)) {
			/* Use binary search to find the exact collision time */
			double res = right * 0.001;
			while (right - left > res) {
				double t = (left + right) * 0.5;
				pos = m_pos + move * t;
				bool colliding = false;
				for (const CollFace *f : faces) {
					if (hittest(*f, pos, rad, NULL)) {
						colliding = true;
						break;
					}
				}
				if (!colliding) {
					left = t;
				} else {
					right = t;
				}
			}
		}
		m_pos += move * right;
		move -= move * right;

		/* Collided with something - Find out which faces and remove
		 * the compoment which caused the collision from our movement.
		 * Also, apply bounce and update whether the object is
		 * standing on ground.
		 */
		faces.clear();
		objs.clear();
		m_world->find_collisions(m_pos, rad, &faces, &objs, collided);
		for (const CollFace *f : faces) {
			vec3 contact;
			if (!hittest(*f, m_pos, rad, &contact)) {
				assert(0);
			}
			collided.insert(f);
			double l = length(contact);
			contact *= 1 / l;
			if (dot(move, contact) < 0) {
				move -= contact * dot(move, contact);
			}
			if (dot(m_vel, contact) < 1e-4) {
				if (!friction) {
					m_vel -= m_vel * (dt * 5);
				}
				friction = true;
			}
			if (dot(m_vel, contact) < 0) {
				m_vel -= contact * (dot(m_vel, contact) * 1.1);
			}
			m_vel += contact * ((rad - l) * 100 * dt);
			if (contact.y > 0.2) {
				m_grounded = true;
			}
		}
		steps++;
	}
	if (m_world != NULL && m_model != NULL) {
		m_world_pos = m_pos + m_offset +
			      transform(m_model->midpos(), m_matrix);
		m_world->register_obj(this, m_world_pos, m_model->rad(), true);
	}
	update_lights();
}

void Object::set_anim(double anim)
{
	m_anim = anim;
}

void Object::set_lights_on(bool on)
{
	m_lights_on = on;
	if (m_world != NULL) {
		update_lights();
	}
}

World::World()
	: m_ambient(0.1, 0.1, 0.1)
{
}

void World::set_ambient(const Color &c)
{
	m_ambient = c;
}

void World::render(const Camera &camera, int flags)
{
	if (flags & (RENDER_BLOOM | RENDER_SHADOW_VOL)) {
		/* Drawing bloom - Don't bother with lights */
		begin_rendering(0, flags, camera.pos);
	}

	render(NULL, camera, flags);

	if (flags & (RENDER_BLOOM | RENDER_SHADOW_VOL)) {
		end_rendering();
	}
}

void World::load(const char *fname)
{
	Mesh mesh;
	load_mesh(&mesh, fname);
	m_materials = mesh.materials;

	for (auto iter : m_materials) {
		if (iter.first == "invisiblewall") {
			iter.second->color.a = 0;
		}
	}
	build_tree(&mesh, 10);
}

void World::register_lights()
{
	/* Scan all nodes in the tree and register lights from all leaves */
	std::list<Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		Tree *tree = queue.front();
		queue.pop_front();

		std::list<Model::Light> lights = tree->model.get_lights();
		for (const Model::Light &l : lights) {
			tree->lights.push_back(Light());
			Light *light = &tree->lights.back();
			light->set_color(l.mat->light_color);
			light->set_brightness(l.mat->brightness);
			light->move(l.pos);
			light->set_world(this);
		}

		if (tree->children[0] != NULL) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL) {
			queue.push_back(tree->children[1]);
		}
	}
}

Material *World::get_material(const char *name)
{
	auto iter = m_materials.find(name);
	if (iter == m_materials.end()) {
		return NULL;
	}
	return iter->second;
}

void World::build_leaf(Tree *tree, const Mesh *mesh,
			const std::list<Face> &faces)
{
	tree->model.load(mesh, faces, true);
}

void World::split_faces(const std::list<Face> &faces, const Tree *tree,
		        int side, std::list<Face> *out)
{
	/* Split the given faces so that they complete lie on the desired side
	 * of the plane.
	 */
	for (const Face &f : faces) {
		std::vector<Vertex> points;
		for (int i = 0; i < 3; ++i) {
			const Vertex &a = f.vert[i];
			const Vertex &b = f.vert[(i + 1) % 3];
			double d1 = dot(a.vert, tree->plane.norm);
			double d2 = dot(b.vert, tree->plane.norm);
			if ((d1 > tree->plane.pos) == side) {
				points.push_back(a);
			}
			if ((d1 - tree->plane.pos) * (d2 - tree->plane.pos) < 0) {
				/* Crosses the plane - Calculate a new vertex
				 * that lines on the plane using interpolation.
				 */
				double x = (tree->plane.pos - d1) / (d2 - d1);
				assert(x >= 0 && x <= 1);
				Vertex v;
				v.vert = a.vert + (b.vert - a.vert) * x;
				v.norm = a.norm + (b.norm - a.norm) * x;
				v.tc = a.tc + (b.tc - a.tc) * x;
				points.push_back(v);
			}
		}
		/* Construct triangles from the points */
		for (size_t i = 2; i < points.size(); ++i) {
			Face new_face;
			new_face.vert[0] = points[0];
			new_face.vert[1] = points[i - 1];
			new_face.vert[2] = points[i];
			new_face.mat = f.mat;
			out->push_back(new_face);
		}
	}
}

void World::build_tree(const Mesh *mesh, int max_depth)
{
	struct Step {
		Tree *tree;
		std::list<Face> faces;
		int depth;
	};
	std::list<Step> queue;

	m_root.box_min = vec3(1e10, 1e10, 1e10);
	m_root.box_max = vec3(-1e10, -1e10, -1e10);
	for (const Face &f : mesh->faces) {
		for (int i = 0; i < 3; ++i) {
			m_root.box_min = min(m_root.box_min, f.vert[i].vert);
			m_root.box_max = max(m_root.box_max, f.vert[i].vert);
		}
	}

	printf("BSP depth %d\n", max_depth);
	Step step;
	step.faces = mesh->faces;
	step.depth = 0;
	step.tree = &m_root;
	queue.push_back(step);

	while (!queue.empty()) {
		step = queue.front();
		queue.pop_front();
		Tree *tree = step.tree;

		if (step.depth >= max_depth) {
			tree->children[0] = NULL;
			tree->children[1] = NULL;
			build_leaf(tree, mesh, step.faces);
			continue;
		}

		const vec3 choices[5] = {
			vec3(1, 0, 0),
			vec3(0, 0, 1),
			vec3(0, 1, 0),
			vec3(1, 0, 0),
			vec3(0, 0, 1),
		};
		tree->plane.norm = choices[step.depth % lengthof(choices)];
		tree->plane.pos = dot(tree->box_min + tree->box_max,
				      tree->plane.norm) * 0.5;

		for (int i = 0; i < 2; ++i) {
			tree->children[i] = new Tree;
			tree->children[i]->box_min = tree->box_min;
			tree->children[i]->box_max = tree->box_max;

			Step child;
			split_faces(step.faces, tree, i, &child.faces);
			child.tree = tree->children[i];
			child.depth = step.depth + 1;
			queue.push_back(child);
		}

		tree->children[0]->box_max += tree->plane.norm *
				(tree->plane.pos - dot(tree->plane.norm, tree->box_max));
		tree->children[1]->box_min += tree->plane.norm *
				(tree->plane.pos - dot(tree->plane.norm, tree->box_min));
	}
}

bool World::raytrace(const vec3 &pos, const vec3 &ray,
		     double *dist, const CollFace **face,
		     Object **obj, const Tree *tree) const
{
	if (ray < 1e-6) return false;

	bool hit = false;

	if (tree == NULL) {
		tree = &m_root;
	}

	vec3 ray_min = min(pos, pos + ray * *dist);
	vec3 ray_max = max(pos, pos + ray * *dist);

	if (ray_max.x < tree->box_min.x ||
	    ray_max.y < tree->box_min.y ||
	    ray_max.z < tree->box_min.z ||
	    ray_min.x > tree->box_max.x ||
	    ray_min.y > tree->box_max.y ||
	    ray_min.z > tree->box_max.z) {
		return false;
	}

	if (!tree->model.loaded()) {
		/* First go recursively to the leaf where the initial point lies */
		int first = dot(pos, tree->plane.norm) > tree->plane.pos;
		if (tree->children[first] != NULL) {
			if (raytrace(pos, ray, dist, face, obj, tree->children[first])) {
				hit = true;
			}
		}
		int second = !first;
		if (tree->children[second] != NULL) {
			if (raytrace(pos, ray, dist, face, obj, tree->children[second])) {
				hit = true;
			}
		}
		return hit;
	}

	if (tree->model.raytrace(pos, ray, 0, dist, face)) {
		if (obj != NULL) {
			*obj = NULL;
		}
		hit = true;
	}
	for (Object *object : tree->objects) {
		if (object->raytrace(pos, ray, dist)) {
			if (obj != NULL) {
				*obj = object;
			}
			if (face != NULL) {
				*face = NULL;
			}
			hit = true;
		}
	}
	return hit;
}

class LightSort {
public:
	LightSort(const vec3 &midpos) :
		m_midpos(midpos)
	{
	}
	bool operator ()(const Light *a, const Light *b)
	{
		double a_dist = length(a->pos() - m_midpos);
		double b_dist = length(b->pos() - m_midpos);
		return (1 - a_dist / (LIGHT_MAX_DIST * a->brightness())) >
		       (1 - b_dist / (LIGHT_MAX_DIST * b->brightness()));
	}

private:
	vec3 m_midpos;
};

void World::render(Tree *tree, const Camera &camera, int flags)
{
	if (tree == NULL) {
		tree = &m_root;
		visibility_test++;
	}

	for (int i = 0; i < 5; ++i) {
		vec3 p(camera.frustum[i].norm.x > 0 ? tree->box_max.x : tree->box_min.x,
		       camera.frustum[i].norm.y > 0 ? tree->box_max.y : tree->box_min.y,
		       camera.frustum[i].norm.z > 0 ? tree->box_max.z : tree->box_min.z);

		if (dot(p, camera.frustum[i].norm) < camera.frustum[i].pos) {
			return;
		}
	}

	if (!tree->model.loaded()) {
		/* First recursively go to the leaf where the camera is */
		int first = dot(camera.pos, tree->plane.norm) > tree->plane.pos;
		int second = first;

		if (flags & RENDER_GLASS) {
			/* When rendering glass, we need to render things that
			 * that are in the distance first - Just reverse the
			 * order.
			 */
			first = !first;
		} else {
			second = !first;
		}
		if (tree->children[first] != NULL) {
			render(tree->children[first], camera, flags);
		}
		if (tree->children[second] != NULL) {
			render(tree->children[second], camera, flags);
		}
		return;
	}

	GLState gl;
	if (!(flags & (RENDER_BLOOM | RENDER_SHADOW_VOL))) {
		/* We need to restart rendering for each leaf since the number
		 * of lights can change.
		 */
		if (!tree->sorted) {
			std::sort(tree->remote_lights.begin(),
				  tree->remote_lights.end(),
				  LightSort(tree->model.midpos()));
			tree->sorted = true;
		}
		size_t numlights = std::min(tree->remote_lights.size(),
					    MAX_LIGHTS);
		if (quality == 0) {
			numlights = std::min<size_t>(numlights, 8);
		}
		begin_rendering(numlights, flags);
		for (size_t i = 0; i < numlights; ++i) {
			tree->remote_lights[i]->program(i);
		}
	}

	glLoadIdentity();
	tree->model.render(flags | RENDER_LIGHTS_ON, 0, m_ambient);
	for (Object *obj : tree->objects) {
		obj->render(camera, flags, visibility_test, m_ambient);
	}
	if (!(flags & (RENDER_BLOOM | RENDER_SHADOW_VOL))) {
		end_rendering();
	}
}

void World::sweep(const Camera &camera, std::unordered_set<Object *> *objs) const
{
	visibility_test++;

	std::list<const Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		const Tree *tree = queue.front();
		queue.pop_front();

		bool in_frustum = true;
		for (int i = 0; i < 5; ++i) {
			vec3 p(camera.frustum[i].norm.x > 0 ? tree->box_max.x : tree->box_min.x,
			       camera.frustum[i].norm.y > 0 ? tree->box_max.y : tree->box_min.y,
			       camera.frustum[i].norm.z > 0 ? tree->box_max.z : tree->box_min.z);

			if (dot(p, camera.frustum[i].norm) < camera.frustum[i].pos) {
				in_frustum = false;
				break;
			}
		}
		if (!in_frustum) continue;

		for (Object *obj : tree->objects) {
			if (obj->visible(camera, visibility_test)) {
				vec3 d = obj->pos() + obj->offset() +
					 transform(obj->model()->midpos(), obj->matrix()) -
					 camera.pos;
				Object *hit = NULL;
				double dist = 1;
				if (!raytrace(camera.pos, d, &dist, NULL, &hit) || hit == obj) {
					objs->insert(obj);
				}
			}
		}

		if (tree->children[0] != NULL) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL) {
			queue.push_back(tree->children[1]);
		}
	}
}

void World::register_light(Light *light, bool add)
{
	double rad = light->brightness() * LIGHT_MAX_DIST;
	assert(rad > 0);

	std::list<Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		Tree *tree = queue.front();
		queue.pop_front();

		if (tree->model.loaded()) {
			if (add) {
				tree->remote_lights.push_back(light);
				tree->sorted = false;
			} else {
				auto i = std::find(tree->remote_lights.begin(),
						   tree->remote_lights.end(),
						   light);
				assert(i != tree->remote_lights.end());
				tree->remote_lights.erase(i);
			}
		}

		/* Recursively traverse the tree and visit all leaves that are
		 * within the given radius from the light.
		 */
		if (tree->children[0] != NULL &&
		    dot(light->pos(), tree->plane.norm) < tree->plane.pos + rad) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL &&
		    dot(light->pos(), tree->plane.norm) > tree->plane.pos - rad) {
			queue.push_back(tree->children[1]);
		}
	}
}

void World::register_obj(Object *obj, const vec3 &pos, double rad, bool add)
{
	assert(rad > 0);

	std::list<Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		Tree *tree = queue.front();
		queue.pop_front();

		if (tree->model.loaded()) {
			if (add) {
				tree->objects.push_back(obj);
			} else {
				auto i = std::find(tree->objects.begin(),
						   tree->objects.end(),
						   obj);
				assert (i != tree->objects.end());
				tree->objects.erase(i);
			}
		}

		/* Recursively traverse the tree and visit all leaves that are
		 * within the given radius from the object.
		 */
		if (tree->children[0] != NULL &&
		    dot(pos, tree->plane.norm) < tree->plane.pos + rad) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL &&
		    dot(pos, tree->plane.norm) > tree->plane.pos - rad) {
			queue.push_back(tree->children[1]);
		}
	}
}

bool World::find_collisions(const vec3 &pos, double rad,
			    std::list<const CollFace *> *faces,
			    std::list<Object *> *objs,
			    const std::unordered_set<const void *> &ignore)
{
	std::list<Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		Tree *tree = queue.front();
		queue.pop_front();

		for (const CollFace *f : tree->model.find_collisions(pos, rad)) {
			if (ignore.count(f) == 0) {
				faces->push_back(f);
			}
		}
		/* TODO
		for (Object *obj : tree->objects) {
			if (ignore.count(obj) == 0) {
			}
		}*/

		/* Recursively traverse the tree and visit all leaves that are
		 * within the given radius from the point. Add some margin
		 * because we suck.
		 */
		if (tree->children[0] != NULL &&
		    dot(pos, tree->plane.norm) < tree->plane.pos + rad + 0.1) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL &&
		    dot(pos, tree->plane.norm) > tree->plane.pos - rad - 0.1) {
			queue.push_back(tree->children[1]);
		}
	}
	return (!objs->empty() || !faces->empty());
}

/* Forcefully unregisters all objects from the world - It's no longer safe
 * manipulate the objects after this!
 */
void World::remove_all_objects()
{
	std::list<Tree *> queue;
	queue.push_back(&m_root);
	while (!queue.empty()) {
		Tree *tree = queue.front();
		queue.pop_front();

		while (!tree->objects.empty()) {
			Object *obj = tree->objects.front();
			assert(obj->world() == this);
			obj->set_world(NULL);
		}

		if (tree->children[0] != NULL) {
			queue.push_back(tree->children[0]);
		}
		if (tree->children[1] != NULL) {
			queue.push_back(tree->children[1]);
		}
	}
}

void prepare_camera(Camera *camera, double fov_x, double fov_y, double dist)
{
	camera->frustum[0].norm =
		normalize(camera->matrix.forward * fov_x + camera->matrix.right);
	camera->frustum[1].norm =
		normalize(camera->matrix.forward * fov_x - camera->matrix.right);
	camera->frustum[2].norm =
		normalize(camera->matrix.forward * fov_y + camera->matrix.up);
	camera->frustum[3].norm =
		normalize(camera->matrix.forward * fov_y - camera->matrix.up);
	camera->frustum[4].norm = camera->matrix.forward * -1;

	for (int i = 0; i < 5; ++i) {
		camera->frustum[i].pos = dot(camera->pos, camera->frustum[i].norm);
	}
	camera->frustum[4].pos -= dist;
}

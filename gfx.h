/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#ifndef __gfx_h__
#define __gfx_h__

#include "vec3.h"
#include "utils.h"
#include <list>
#include <vector>
#include <string>
#include <set>
#include <GL/glew.h>
#include <GL/gl.h>
#include <assert.h>
#include <unordered_map>

const size_t MAX_LIGHTS = 20;

#define RENDER_DIST 1500
#define LIGHT_MAX_DIST 250

enum {
	RENDER_BLOOM = 1,
	RENDER_GLASS = 2,
	RENDER_SHADOW_VOL = 4,
	RENDER_LIGHTS_ON = 8,
};

class Color {
public:
	float r, g, b, a;

	Color() {}
	Color(float i_r, float i_g, float i_b, float i_a = 1) :
		r(i_r),
		g(i_g),
		b(i_b),
		a(i_a)
	{
	}
};

static inline Color operator * (const Color &c, double s)
{
	return Color(c.r * s, c.g * s, c.b * s);
}

const GLuint INVALID_TEXTURE = -1;

class Texture {
public:
	GLuint id() { return m_id; }
	Color color() const { return m_color; }
	int width() const { return m_width; }
	int height() const { return m_height; }
	bool loaded() const { return m_id != INVALID_TEXTURE; }

	Texture();
	void load(const char *fname, bool mipmap = false, bool alpha = false);
	void load(int width, int height, GLuint type, const uint8_t *data,
		  bool mipmap = false);
	void bind() const;
	void draw() const;

private:
	GLuint m_id;
	Color m_color;
	int m_width;
	int m_height;

	DISALLOW_COPY_AND_ASSIGN(Texture);
};

struct CollFace;

struct Material {
	double brightness;
	Color color;
	Color light_color;
	const Texture *texture;

	/* Animation */
	int frame, num_frames, num_cols;
};

struct Vertex {
	vec3 vert;
	vec3 norm;
	vec2 tc;
};
struct Face {
	Vertex vert[3];
	Material *mat;
};
struct Mesh {
	std::list<Face> faces;
	std::unordered_map<std::string, Material *> materials;
};

/* RAII approach to OpenGL state machine - Use this to enable OpenGL states
 * and they get automatically disabled when you leave the scope.
 */
class GLState {
public:
	static const int MAX_STATES = 8;

	GLState() :
		m_num_states(0)
	{}
	~GLState()
	{
		for (int i = 0; i < m_num_states; ++i) {
			glDisable(m_states[i]);
		}
	}

	void enable(GLenum state)
	{
		assert(m_num_states < MAX_STATES);
		glEnable(state);
		m_states[m_num_states++] = state;
	}

private:
	GLenum m_states[MAX_STATES];
	int m_num_states;
};

class Model {
	struct GLAnimVertex {
		vec3 a_vert;
		vec3 a_norm;
		vec3 b_vert; /* MultiTexCoord1 */
		vec3 b_norm; /* MultiTexCoord2 */
		vec2 tc;
	};
	struct Group {
		const Material *mat;
		GLuint buffer;
		size_t count;
		vec3 midpos;
	};
	struct Frame {
		std::list<Group> groups;
		GLuint shadow_buffer;
		size_t shadow_count;
		std::list<CollFace> faces;
		std::list<CollFace> coll_faces;
	};

public:
	struct Light {
		vec3 pos;
		const Material *mat;
	};

	bool loaded() const { return !m_frames.empty(); }
	double rad() const { return m_radius; }
	vec3 midpos() const { return m_midpos; }
	std::unordered_map<std::string, Material *> materials() const { return m_materials; }

	Model();
	void load(const char *fname, double scale = 1, int first_frame = 1,
		  int num_frames = 1, bool noise = false);
	void load_frame(Frame *frame, const Mesh *mesh, const Mesh *next);
	void load(const Mesh *mesh, const std::list<Face> &faces,
		  bool noise = false);
	Material *get_material(const char *name) const;
	bool raytrace(const vec3 &pos, const vec3 &ray, double anim,
		      double *dist, const CollFace **face = NULL) const;
	std::list<const CollFace *> find_collisions(const vec3 &pos, double rad,
						    double anim = 0);
	void render(int flags = 0, double anim = 0,
		    const Color &ambient = Color(0, 0, 0)) const;
	std::list<Light> get_lights() const;

private:
	std::unordered_map<std::string, Material *> m_materials;
	std::vector<Frame> m_frames;
	std::list<Light> m_lights;
	double m_radius;
	vec3 m_midpos;

	DISALLOW_COPY_AND_ASSIGN(Model);
};

class FBO {
public:
	GLuint fbo() { return m_fbo; }
	GLuint texture() { return m_texture; }

	FBO();
	void init(int width, int height, bool bilinear, bool depth = false,
		  bool stencil = false);
	void begin_drawing() const;
	void bind_texture() const;

private:
	GLuint m_fbo;
	GLuint m_texture;
	GLuint m_depthbuf;

	DISALLOW_COPY_AND_ASSIGN(FBO);
};

class Program {
public:
	bool loaded() const { return m_id != 0; }

	Program();
	void load(const char *vs_source, const char *fs_source);
	void use() const;
	GLint uniform(const char *name);

private:
	GLuint m_id;
	std::unordered_map<std::string, GLint> m_lookup;
};

class Font {
	struct Glyph {
		int x, x_bearing, y_bearing, width, height;
		int x_advance, y_advance;
	};
public:
	Font();
	void load(const char *fname);
	void draw_text(const char *text) const;
	int text_width(const char *text) const;

private:
	Texture m_texture;
	Glyph m_glyphs[127 - 32];
};

extern size_t faces_drawn;
extern size_t gfx_memory;

GLuint load_png(const char *fname);
void check_gl_errors();
void set_light(int n, const vec3 &pos, const Color &c, double brightness = 1);
void load_mesh(Mesh *mesh, const char *fname, double scale = 1);
void open_pack(const char *fname);
void begin_rendering(size_t numlights, int flags, const vec3 &light = vec3(0, 0, 0));
void end_rendering();
void mult_matrix(const Matrix &m);
void mult_matrix_reverse(const Matrix &m);
const Texture *get_texture(const char *fname);
void load_texture(const char *fname, bool mipmap, bool alpha);
const Model *get_model(const char *fname);
void load_model(const char *fname, double scale, int first_frame,
		int num_frames, bool noise);

#endif

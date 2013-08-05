/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include "gfx.h"
#include "system.h"
#include <string.h>
#include <stdexcept>
#include <png.h>
#include <map>

#define str(s) __str(s)
#define __str(s) #s

size_t faces_drawn;
size_t gfx_memory;

namespace {

Program *current_program = NULL;
std::unordered_map<std::string, Texture *> texture_cache;
std::unordered_map<std::string, Model *> model_cache;

const char simple_vs[] =
"struct Light {\
	vec3 pos;\
	float brightness;\
	vec4 diffuse;\
};\
uniform Light lights[%d];\
varying vec4 color;\
uniform float x;\
\
void main(void)\
{\
	vec3 vert = gl_Vertex.xyz * (1.0 - x) + gl_MultiTexCoord1.xyz * x;\
	vec3 normal = gl_Normal * (1.0 - x) + gl_MultiTexCoord2.xyz * x;\
	int i;\
	vec3 d;\
	float dist, NdotL, att;\
	vec4 diffuse;\
	vec4 v = gl_ModelViewMatrix * vec4(vert, 1.0);\
	vec3 n = gl_NormalMatrix * normal;\
\
	gl_Position = gl_ProjectionMatrix * v;\
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\
\
	color = gl_LightModel.ambient * gl_FrontMaterial.ambient;\
	for (i = 0; i < %d; i++) {\
		d = lights[i].pos - (v.xyz / v.w);\
		dist = length(d);\
		NdotL = max(dot(n, d / dist), 0.2);\
		diffuse = gl_FrontMaterial.diffuse * lights[i].diffuse;\
		att = 1.0 - dist / (" str(LIGHT_MAX_DIST) ".0 * lights[i].brightness);\
		color += diffuse * (NdotL * clamp(att, 0.0, 1.0));\
	}\
	color = clamp(color, 0.0, 1.0);\
}";

const char simple_fs[] =
"varying vec4 color;\
uniform sampler2D tex;\
void main(void)\
{\
	gl_FragColor = texture2D(tex, gl_TexCoord[0].xy) * color;\
}";

const char perpixel_vs[] =
"varying vec4 v;\
varying vec3 n;\
uniform float x;\
void main(void)\
{\
	vec3 vert = gl_Vertex.xyz * (1.0 - x) + gl_MultiTexCoord1.xyz * x;\
	vec3 normal = gl_Normal * (1.0 - x) + gl_MultiTexCoord2.xyz * x;\
	v = gl_ModelViewMatrix * vec4(vert, 1.0);\
	n = gl_NormalMatrix * normal;\
\
	gl_Position = gl_ProjectionMatrix * v;\
	gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;\
\
}";

const char perpixel_fs[] =
"struct Light {\
	vec3 pos;\
	float brightness;\
	vec4 diffuse;\
};\
uniform Light lights[%d];\
varying vec4 v;\
varying vec3 n;\
uniform sampler2D tex;\
void main(void)\
{\
	int i;\
	vec3 d;\
	float dist, NdotL, att;\
	vec4 diffuse;\
	vec3 normal = normalize(n);\
	vec4 color = gl_LightModel.ambient * gl_FrontMaterial.ambient;\
	for (i = 0; i < %d; i++) {\
		d = lights[i].pos - (v.xyz / v.w);\
		dist = length(d);\
		NdotL = max(dot(normal, d / dist), 0.2);\
		diffuse = gl_FrontMaterial.diffuse * lights[i].diffuse;\
		att = 1.0 - dist / (" str(LIGHT_MAX_DIST) ".0 * lights[i].brightness);\
		color += diffuse * (NdotL * clamp(att, 0.0, 1.0));\
	}\
	color = clamp(color, 0.0, 1.0);\
	gl_FragColor = texture2D(tex, gl_TexCoord[0].xy) * color;\
}";

const char shadow_vs[] =
"uniform float x;\
uniform vec3 light;\
void main(void)\
{\
	vec3 vert = gl_Vertex.xyz * (1.0 - x) + gl_MultiTexCoord1.xyz * x;\
	vec3 normal = gl_Normal * (1.0 - x) + gl_MultiTexCoord2.xyz * x;\
	vec4 v = gl_ModelViewMatrix * vec4(vert, 1.0);\
	vec3 n = gl_NormalMatrix * normal;\
\
	vec3 delta = (v.xyz / v.w) - light;\
	if (dot(delta, n) > 0.0) {\
		v = vec4((v.xyz / v.w) + normalize(delta) * gl_MultiTexCoord0.x, 1.0);\
	}\
	gl_Position = gl_ProjectionMatrix * v;\
	gl_FrontColor = gl_Color;\
}";

const char shadow_fs[] =
"void main(void)\
{\
	gl_FragColor = gl_Color;\
}";

void png_error_func(png_structp png_ptr, png_const_charp error_msg)
{
	(void) png_ptr;
	throw std::runtime_error(std::string("Error loading PNG: ") +
				 error_msg);
}

void png_read_data(png_structp png_ptr, png_bytep data, png_size_t len)
{
	PackFile *f = (PackFile *) png_get_io_ptr(png_ptr);
	if (f->read(data, len) < len) {
		throw std::runtime_error("Partial PNG");
	}
}

}

Texture::Texture() :
	m_id(INVALID_TEXTURE),
	m_width(0),
	m_height(0)
{
}

void Texture::load(const char *fname, bool mipmap, bool alpha)
{
	printf("Loading %s\n", fname);
	PackFile f;
	f.open(fname);

	png_byte header[8] = {};
	f.read(header, sizeof header);
	if (png_sig_cmp(header, 0, sizeof header)) {
		throw std::runtime_error(std::string("Invalid PNG: ") + fname);
	}

	png_structp png_ptr =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	assert(png_ptr != NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	assert(info_ptr != NULL);

	png_set_error_fn(png_ptr, NULL, png_error_func, NULL);
	png_set_read_fn(png_ptr, &f, png_read_data);
	png_set_sig_bytes(png_ptr, sizeof header);

	png_read_info(png_ptr, info_ptr);
	png_set_expand(png_ptr);
	png_set_packing(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	int width = png_get_image_width(png_ptr, info_ptr);
	int height = png_get_image_height(png_ptr, info_ptr);
	int channels = png_get_channels(png_ptr, info_ptr);;

	GLuint type;
	switch (png_get_color_type(png_ptr, info_ptr)) {
	case PNG_COLOR_TYPE_GRAY:
		type = alpha ? GL_ALPHA : GL_LUMINANCE;
		break;
	case PNG_COLOR_TYPE_GRAY_ALPHA:
		type = GL_LUMINANCE_ALPHA;
		break;
	case PNG_COLOR_TYPE_RGB:
		type = GL_RGB;
		break;
	case PNG_COLOR_TYPE_RGB_ALPHA:
		type = GL_RGBA;
		break;
	default:
		throw std::runtime_error("PNG has unknown color type");
	}

	std::vector<uint8_t> pixels(width * height * channels, 0);
	std::vector<png_bytep> rows(height);
	for (int i = 0; i < height; ++i) {
		rows[i] = (png_bytep) &pixels[width * channels * i];
	}
	png_read_image(png_ptr, &rows[0]);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	load(width, height, type, &pixels[0], mipmap);
}

void Texture::load(int width, int height, GLuint type, const uint8_t *data,
		   bool mipmap)
{
	if ((width & (width - 1)) > 0 ||
	    (height & (height - 1)) > 0) {
		throw std::runtime_error("Non-power-of-two texture");
	}

	m_width = width;
	m_height = height;
	m_color = Color(0, 0, 0);

	if (type == GL_RGB) {
		Color sum(0, 0, 0);
		for (int i = 0; i < width * height; ++i) {
			sum.r += data[i * 3] / 255.0;
			sum.g += data[i * 3 + 1] / 255.0;
			sum.b += data[i * 3 + 2] / 255.0;
		}
		m_color = sum * (1.0 / (m_width*m_height));
	} else if (type == GL_RGBA) {
		Color sum(0, 0, 0);
		for (int i = 0; i < width * height; ++i) {
			sum.r += data[i * 4] / 255.0;
			sum.g += data[i * 4 + 1] / 255.0;
			sum.b += data[i * 4 + 2] / 255.0;
		}
		m_color = sum * (1.0 / (m_width*m_height));
	}

	if (m_id == INVALID_TEXTURE) {
		glGenTextures(1, &m_id);
		assert(m_id != INVALID_TEXTURE);
	}
	glBindTexture(GL_TEXTURE_2D, m_id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			mipmap ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, mipmap);
	glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0,
		     type, GL_UNSIGNED_BYTE, data);

	while (width > 0 && height > 0) {
		gfx_memory += width * height *
			(type == GL_RGBA ? 4 : 3);
		if (!mipmap) break;
		width /= 2;
		height /= 2;
	}
}

void Texture::bind() const
{
	assert(m_id != INVALID_TEXTURE);
	glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture::draw() const
{
	GLState gl;
	gl.enable(GL_TEXTURE_2D);
	gl.enable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	bind();

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0);
	glTexCoord2f(0, 1);
	glVertex3f(0, m_height, 0);
	glTexCoord2f(1, 1);
	glVertex3f(m_width, m_height, 0);
	glTexCoord2f(1, 0);
	glVertex3f(m_width, 0, 0);
	glEnd();
}

Model::Model() :
	m_radius(0),
	m_midpos(0, 0, 0)
{
}

void Model::load(const char *fname, double scale, int first_frame,
		 int num_frames, bool noise)
{
	assert(num_frames >= 1);

	if (num_frames == 1) {
		Mesh mesh;
		load_mesh(&mesh, fname, scale);
		load(&mesh, mesh.faces, noise);
		return;
	}

	assert(m_frames.empty());

	m_frames.resize(num_frames);
	m_radius = 0;

	Mesh meshes[2];
	Mesh first;
	for (int i = 0; i < num_frames; ++i) {
		char buf[64];
		sprintf(buf, "%s_%06d.obj", fname, first_frame + i);
		Mesh *mesh = &meshes[i & 1];
		load_mesh(mesh, buf, scale);
		if (i == 0) {
			m_materials = meshes[0].materials;
			meshes[1].materials = m_materials;
			first = meshes[0];
		} else {
			load_frame(&m_frames[i - 1], &meshes[(i - 1) & 1],
				   mesh);
		}
	}
	load_frame(&m_frames[num_frames - 1], &meshes[(num_frames - 1) & 1],
		   &first);

	m_radius = sqrt(m_radius);
}

Material *Model::get_material(const char *name) const
{
	auto iter = m_materials.find(name);
	if (iter == m_materials.end()) {
		return NULL;
	}
	return iter->second;
}

void Model::load_frame(Frame *frame, const Mesh *mesh, const Mesh *next)
{
	vec3 box_min(1e10, 1e10, 1e10);
	vec3 box_max(-1e10, -1e10, -1e10);

	/* Assumes both meshes have the same materials and the faces are
	 * in the same order. Also, texture coordinates are not animated.
	 */
	std::vector<GLAnimVertex> shadow;
	std::unordered_map<const Material *, std::vector<GLAnimVertex>> materials;
	auto nextiter = next->faces.begin();
	for (const Face &a : mesh->faces) {
		assert(nextiter != next->faces.end());
		const Face &b = *nextiter;
		++nextiter;

		CollFace coll;
		coll.norm = normalize(cross(
			a.vert[1].vert - a.vert[0].vert,
			a.vert[2].vert - a.vert[0].vert));
		if (coll.norm < 1e-4) {
			printf("an empty face\n");
			continue;
		}

		/* Calculate edge vectors, which are used to find out whether a
		 * point is inside the face.
		 */
		for (int i = 0; i < 3; ++i) {
			coll.vert[i] = a.vert[i].vert;
			vec3 d = a.vert[(i + 1) % 3].vert - a.vert[i].vert;
			coll.edges[i] = normalize(cross(coll.norm, d));
		}

		frame->faces.push_back(coll);

		for (int i = 0; i < 3; ++i) {
			GLAnimVertex v;
			v.a_vert = a.vert[i].vert;
			v.b_vert = b.vert[i].vert;

			box_min = min(box_min, v.a_vert);
			box_max = max(box_max, v.a_vert);

			v.tc = a.vert[i].tc;
			v.a_norm = a.vert[i].norm;
			v.b_norm = b.vert[i].norm;
			materials[a.mat].push_back(v);
		}

		vec3 b_norm = normalize(cross(
			b.vert[1].vert - b.vert[0].vert,
			b.vert[2].vert - b.vert[0].vert));

		/* Shadow volume which extrudes from each edge */
		for (int i = 0; i < 3; ++i) {
			GLAnimVertex v1;
			GLAnimVertex v2;
			v1.a_vert = a.vert[i].vert;
			v1.b_vert = b.vert[i].vert;
			v2.a_vert = a.vert[(i + 1) % 3].vert;
			v2.b_vert = b.vert[(i + 1) % 3].vert;
			v1.a_norm = coll.norm;
			v1.b_norm = b_norm;
			v2.a_norm = coll.norm;
			v2.b_norm = b_norm;
			v1.tc = vec2(0, 0);
			v2.tc = vec2(0, 0);
			shadow.push_back(v2);
			shadow.push_back(v1);
			v1.tc = vec2(RENDER_DIST, 0);
			v2.tc = vec2(RENDER_DIST, 0);
			shadow.push_back(v1);
			shadow.push_back(v2);
		}
	}
	assert(nextiter == next->faces.end());

	if (frame == &m_frames[0]) {
		m_midpos = (box_min + box_max) * 0.5;
	}

	for (const Face &f : mesh->faces) {
		if (f.mat->color.a <= 0) continue;

		for (int i = 0; i < 3; ++i) {
			vec3 delta = f.vert[i].vert - m_midpos;
			if (dot(delta, delta) > m_radius) {
				m_radius = dot(delta, delta);
			}
		}
	}

	for (const auto &i : materials) {
		Group group;
		group.mat = i.first;

		gfx_memory += i.second.size() * sizeof(GLAnimVertex);

		group.count = i.second.size();
		glGenBuffers(1, &group.buffer);
		glBindBuffer(GL_ARRAY_BUFFER, group.buffer);
		glBufferData(GL_ARRAY_BUFFER,
			     i.second.size() * sizeof(GLAnimVertex),
			     &i.second[0],  GL_STATIC_DRAW);
		frame->groups.push_back(group);
	}

	gfx_memory += shadow.size() * sizeof(GLAnimVertex);

	frame->shadow_count = shadow.size();
	glGenBuffers(1, &frame->shadow_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, frame->shadow_buffer);
	glBufferData(GL_ARRAY_BUFFER,
		     shadow.size() * sizeof(GLAnimVertex),
		     &shadow[0],  GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Model::load(const Mesh *mesh, const std::list<Face> &faces,
		 bool noise)
{
	static const Texture *noise_tex;
	if (noise_tex == NULL) {
		noise_tex = get_texture("noise.png");
	}

	assert(m_frames.empty());
	m_frames.resize(1);
	m_materials = mesh->materials;

	for (auto iter : m_materials) {
		if (noise && iter.second->texture == NULL) {
			iter.second->texture = noise_tex;
		}
	}

	/* Calculate the middle pos and radius for the mesh while at it */
	vec3 box_min(1e10, 1e10, 1e10);
	vec3 box_max(-1e10, -1e10, -1e10);

	std::unordered_map<Material *, std::vector<Vertex>> materials;
	std::vector<Vertex> shadow;
	for (const Face &f : faces) {
		CollFace coll;
		coll.norm = normalize(cross(
			f.vert[1].vert - f.vert[0].vert,
			f.vert[2].vert - f.vert[0].vert));
		if (coll.norm < 1e-4) {
			printf("an empty face\n");
			continue;
		}

		/* Calculate edge vectors, which are used to find out whether a
		 * point is inside the face.
		 */
		for (int i = 0; i < 3; ++i) {
			coll.vert[i] = f.vert[i].vert;
			vec3 d = f.vert[(i + 1) % 3].vert - f.vert[i].vert;
			coll.edges[i] = normalize(cross(coll.norm, d));
		}

		if (f.mat->color.a > 0) {
			m_frames[0].faces.push_back(coll);
		} else {
			/* The face is only used for collisions */
			m_frames[0].coll_faces.push_back(coll);
			continue;
		}

		for (int i = 0; i < 3; ++i) {
			Vertex v = f.vert[i];
			if (f.mat->texture == noise_tex) {
				v.tc = vec2((v.vert.x + v.vert.y) * 0.1,
					    (v.vert.z + v.vert.y) * 0.1);
			}
			materials[f.mat].push_back(v);

			box_min = min(box_min, v.vert);
			box_max = max(box_max, v.vert);
		}
		/* Shadow volume which extrudes from each edge */
		for (int i = 0; i < 3; ++i) {
			Vertex v1 = f.vert[i];
			Vertex v2 = f.vert[(i + 1) % 3];
			v1.norm = coll.norm;
			v2.norm = coll.norm;
			v1.tc = vec2(0, 0);
			v2.tc = vec2(0, 0);
			shadow.push_back(v2);
			shadow.push_back(v1);
			v1.tc = vec2(RENDER_DIST, 0);
			v2.tc = vec2(RENDER_DIST, 0);
			shadow.push_back(v1);
			shadow.push_back(v2);
		}
	}
	m_midpos = (box_min + box_max) * 0.5;
	m_radius = 0;
	for (const Face &f : faces) {
		if (f.mat->color.a <= 0) continue;

		for (int i = 0; i < 3; ++i) {
			vec3 delta = f.vert[i].vert - m_midpos;
			if (dot(delta, delta) > m_radius) {
				m_radius = dot(delta, delta);
			}
		}
	}
	m_radius = sqrt(m_radius);

	for (const auto &i : materials) {
		Group group;
		group.mat = i.first;

		/* TODO: Move this to somewhere else. Separate light init? */
		vec3 box_min(1e10, 1e10, 1e10);
		vec3 box_max(-1e10, -1e10, -1e10);
		for (const Vertex &v : i.second) {
			box_min = min(box_min, v.vert);
			box_max = max(box_max, v.vert);
		}
		group.midpos = (box_min + box_max) * 0.5;

		gfx_memory += i.second.size() * sizeof(Vertex);

		group.count = i.second.size();
		glGenBuffers(1, &group.buffer);
		glBindBuffer(GL_ARRAY_BUFFER, group.buffer);
		glBufferData(GL_ARRAY_BUFFER,
			     i.second.size() * sizeof(Vertex),
			     &i.second[0],  GL_STATIC_DRAW);
		m_frames[0].groups.push_back(group);
	}

	gfx_memory += shadow.size() * sizeof(Vertex);

	m_frames[0].shadow_count = shadow.size();
	glGenBuffers(1, &m_frames[0].shadow_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_frames[0].shadow_buffer);
	glBufferData(GL_ARRAY_BUFFER,
		     shadow.size() * sizeof(Vertex),
		     &shadow[0],  GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

bool Model::raytrace(const vec3 &pos, const vec3 &ray, double anim,
		     double *dist, const CollFace **face) const
{
	if (m_frames.empty()) return false;

	const Frame *frame = &m_frames[(int) floor(anim) % m_frames.size()];

	bool hit = false;
	for (const CollFace &f : frame->faces) {
		/* First, calculate the distance to the plane, and then verify
		 * whether that point is inside the face.
		 * http://en.wikipedia.org/wiki/Line-plane_intersection
		 */
		double div = dot(f.norm, ray);
		if (fabs(div) < 1e-8) continue;
		double d = dot(f.vert[0] - pos, f.norm) / div;
		if (d >= 0 && d < *dist && inside(f, pos + ray * d)) {
			if (face != NULL) {
				*face = &f;
			}
			*dist = d;
			hit = true;
		}
	}
	return hit;
}

std::list<const CollFace *> Model::find_collisions(const vec3 &pos, double rad,
						   double anim)
{
	assert(rad > 0);

	std::list<const CollFace *> contacts;
	if (m_frames.empty()) return contacts;

	const Frame *frame = &m_frames[(int) floor(anim) % m_frames.size()];

	for (const CollFace &f : frame->faces) {
		if (hittest(f, pos, rad, NULL)) {
			contacts.push_back(&f);
		}
	}
	for (const CollFace &f : frame->coll_faces) {
		if (hittest(f, pos, rad, NULL)) {
			contacts.push_back(&f);
		}
	}
	return contacts;
}

/* Assumes we are in a render mode, see begin_rendering(). */
void Model::render(int flags, double anim, const Color &ambient) const
{
	static const Texture *blank;
	const float black[] = {0, 0, 0, 1};
	const float white[] = {1, 1, 1, 1};

	if (m_frames.empty()) return;

	assert(current_program != NULL);

	/* It's easier to switch to a blank texture for textures without one */
	if (blank == NULL) {
		blank = get_texture("blank.png");
	}

	if (m_frames.size() > 1) {
		GLint loc = current_program->uniform("x");
		glUniform1f(loc, anim - (int) floor(anim));

		glClientActiveTexture(GL_TEXTURE1);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE2);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE0);
	} else {
		/* We always use the animation shader, so better set this */
		GLint loc = current_program->uniform("x");
		glUniform1f(loc, 0);
	}

	const Frame *frame = &m_frames[(int) floor(anim) % m_frames.size()];

	if (flags & RENDER_SHADOW_VOL) {
		glBindBuffer(GL_ARRAY_BUFFER, frame->shadow_buffer);
		if (m_frames.size() == 1) {
			Vertex *v = NULL;
			glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &v->vert);
			glNormalPointer(GL_FLOAT, sizeof(Vertex), &v->norm);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &v->tc);
		} else {
			GLAnimVertex *v = NULL;
			glVertexPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->a_vert);
			glNormalPointer(GL_FLOAT, sizeof(GLAnimVertex), &v->a_norm);
			glTexCoordPointer(2, GL_FLOAT, sizeof(GLAnimVertex), &v->tc);
			glClientActiveTexture(GL_TEXTURE1);
			glTexCoordPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->b_vert);
			glClientActiveTexture(GL_TEXTURE2);
			glTexCoordPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->b_norm);
			glClientActiveTexture(GL_TEXTURE0);
		}
		glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		glFrontFace(GL_CCW);
		glDrawArrays(GL_QUADS, 0, frame->shadow_count);

		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		glFrontFace(GL_CW);
		glDrawArrays(GL_QUADS, 0, frame->shadow_count);

		faces_drawn += frame->shadow_count / 2;
	} else
	for (const Group &g : frame->groups) {
		GLState gl;
		if (g.mat->color.a < 1) {
			if (!(flags & RENDER_GLASS)) continue;
		} else {
			if (flags & RENDER_GLASS) continue;
		}
		if (g.mat->num_frames > 0) {
			glMatrixMode(GL_TEXTURE);
			glScalef(1.0 / g.mat->num_cols,
				 1.0 / (g.mat->num_frames / g.mat->num_cols),
				 1);
			glTranslatef(g.mat->frame / (g.mat->num_frames / g.mat->num_cols),
				     g.mat->frame, 0);
			glMatrixMode(GL_MODELVIEW);
		}
		if (flags & RENDER_BLOOM) {
			/* We don't have any lights so everything is based on
			 * the ambient we set here.
			 */
			if (g.mat->brightness > 0 && (flags & RENDER_LIGHTS_ON)) {
				if (g.mat->texture != NULL) {
					g.mat->texture->bind();
				} else {
					blank->bind();
				}
				float ambient[] = {(float) g.mat->brightness,
						   (float) g.mat->brightness,
						   (float) g.mat->brightness, 1};
				glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);
			} else {
				glLightModelfv(GL_LIGHT_MODEL_AMBIENT, black);
				blank->bind();
			}
		} else {
			if (g.mat->texture != NULL) {
				g.mat->texture->bind();
			} else {
				blank->bind();
			}
			if (g.mat->brightness > 0 && (flags & RENDER_LIGHTS_ON)) {
				glLightModelfv(GL_LIGHT_MODEL_AMBIENT, white);
			} else {
				glLightModelfv(GL_LIGHT_MODEL_AMBIENT,
						&ambient.r);
			}
		}
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE,
			     &g.mat->color.r);

		glBindBuffer(GL_ARRAY_BUFFER, g.buffer);
		if (m_frames.size() == 1) {
			Vertex *v = NULL;
			glVertexPointer(3, GL_FLOAT, sizeof(Vertex), &v->vert);
			glNormalPointer(GL_FLOAT, sizeof(Vertex), &v->norm);
			glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &v->tc);
		} else {
			GLAnimVertex *v = NULL;
			glVertexPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->a_vert);
			glNormalPointer(GL_FLOAT, sizeof(GLAnimVertex), &v->a_norm);
			glTexCoordPointer(2, GL_FLOAT, sizeof(GLAnimVertex), &v->tc);
			glClientActiveTexture(GL_TEXTURE1);
			glTexCoordPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->b_vert);
			glClientActiveTexture(GL_TEXTURE2);
			glTexCoordPointer(3, GL_FLOAT, sizeof(GLAnimVertex), &v->b_norm);
			glClientActiveTexture(GL_TEXTURE0);
		}
		glDrawArrays(GL_TRIANGLES, 0, g.count);
		faces_drawn += g.count / 3;

		if (g.mat->num_frames > 0) {
			glMatrixMode(GL_TEXTURE);
			glLoadIdentity();
			glMatrixMode(GL_MODELVIEW);
		}
	}
	if (m_frames.size() > 1) {
		glClientActiveTexture(GL_TEXTURE1);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE2);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE0);
	}
}

std::list<Model::Light> Model::get_lights() const
{
	std::list<Light> lights;
	if (m_frames.empty()) {
		return lights;
	}
	for (const Group & g : m_frames[0].groups) {
		if (g.mat->brightness > 0) {
			Light l;
			l.pos = g.midpos;
			l.mat = g.mat;
			lights.push_back(l);
		}
	}
	return lights;
}

void check_gl_errors()
{
	static int modelview_stack_depth = -1;
	static int proj_stack_depth = -1;

	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		char buf[64];
		sprintf(buf, "OpenGL error: %x", error);
		throw std::runtime_error(buf);
	}

	int depth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &depth);
	if (modelview_stack_depth >= 0 && depth > modelview_stack_depth) {
		throw std::runtime_error("OpenGL modelview matrix stack leak");
	}
	modelview_stack_depth = depth;

	glGetIntegerv(GL_PROJECTION_STACK_DEPTH, &depth);
	if (proj_stack_depth >= 0 && depth > proj_stack_depth) {
		throw std::runtime_error("OpenGL projection matrix stack leak");
	}
	proj_stack_depth = depth;
}

FBO::FBO() :
	m_fbo(0),
	m_texture(INVALID_TEXTURE),
	m_depthbuf(0)
{
}

void FBO::init(int width, int height, bool bilinear, bool depth, bool stencil)
{
	printf("creating FBO %d x %d\n", width, height);

	if (m_fbo == 0) {
		glGenFramebuffers(1, &m_fbo);
		glGenTextures(1, &m_texture);
		assert(m_fbo > 0);
		assert(m_texture != INVALID_TEXTURE);
	}
	if (depth && m_depthbuf == 0) {
		glGenRenderbuffers(1, &m_depthbuf);
	}

	GLenum filter = bilinear ? GL_LINEAR : GL_NEAREST;
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_texture);
	glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S,
			GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T,
			GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
			filter);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
			filter);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGB8, width, height,
		     0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			       GL_TEXTURE_RECTANGLE_ARB, m_texture, 0);

	if (depth) {
		glBindRenderbuffer(GL_RENDERBUFFER, m_depthbuf);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
				      width, height);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				GL_DEPTH_ATTACHMENT,
				GL_RENDERBUFFER, m_depthbuf);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER,
				GL_STENCIL_ATTACHMENT,
				GL_RENDERBUFFER, m_depthbuf);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	switch (glCheckFramebufferStatus(GL_FRAMEBUFFER)) {
	case GL_FRAMEBUFFER_COMPLETE:
		break;
	case GL_FRAMEBUFFER_UNSUPPORTED:
		throw std::runtime_error("FBO config unsupported");
	default:
		char buf[64];
		sprintf(buf, "Can not create FBO: %x",
			glCheckFramebufferStatus(GL_FRAMEBUFFER));
		throw std::runtime_error(buf);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	check_gl_errors();
}

void FBO::begin_drawing() const
{
	assert(m_fbo > 0);
	glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void FBO::bind_texture() const
{
	assert(m_texture != INVALID_TEXTURE);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_texture);
}

Font::Font()
{
}

void Font::load(const char *fname)
{
	printf("Loading %s\n", fname);
	PackFile f;
	f.open(fname);

	char line[256];
	size_t avail = 0;
	int x = 0;
	while (1) {
		line[avail] = 0;
		char *end = strchr(line, '\n');
		if (end == NULL) {
			avail += f.read(&line[avail], sizeof line - avail - 1);
			line[avail] = 0;
			end = strchr(line, '\n');
		}
		if (end != NULL) *end = 0;
		char *p = line;

		if (match(p, "texture")) {
			std::string fname = token(p);
			m_texture.load(fname.c_str());

		} else if (match(p, "glyph")) {
			Glyph glyph;
			int i;
			if (sscanf(p, "%d%d%d%d%d%d%d", &i, &glyph.x_bearing,
				   &glyph.y_bearing, &glyph.width, &glyph.height,
				   &glyph.x_advance, &glyph.y_advance) != 7) {
				throw std::runtime_error("Invalid glyph");
			}
			assert(i >= 32 && i < 127);
			glyph.x = x;
			m_glyphs[i - 32] = glyph;
			x += glyph.width;
		}

		if (end == NULL) break;
		++end;
		avail -= end - line;
		memmove(line, end, avail);
	}
}

void Font::draw_text(const char *text) const
{
	GLState gl;
	gl.enable(GL_TEXTURE_2D);
	gl.enable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	m_texture.bind();

	glBegin(GL_QUADS);
	int x = 0, y = 0;
	while (*text) {
		int i = (unsigned char) *text;

		if (i == '\n') {
			x = 0;
			y += m_glyphs['W' - 32].height * 1.5;

		} else if (i >= 32 && i < 127) {
			const Glyph *g = &m_glyphs[i - 32];

			glTexCoord2f((double) g->x / m_texture.width(), 0);
			glVertex2f(x + g->x_bearing, y + g->y_bearing);

			glTexCoord2f((double) g->x / m_texture.width(),
				     (double) g->height / m_texture.height());
			glVertex2f(x + g->x_bearing,
				   y + g->y_bearing + g->height);

			glTexCoord2f((double) (g->x + g->width) /
					m_texture.width(),
				     (double) g->height / m_texture.height());
			glVertex2f(x + g->width + g->x_bearing,
				   y + g->y_bearing + g->height);

			glTexCoord2f((double) (g->x + g->width) / m_texture.width(),
				     0);
			glVertex2f(x + g->width + g->x_bearing,
				   y + g->y_bearing);

			faces_drawn++;

			x += g->x_advance;
		}
		++text;
	}
	glEnd();
}

int Font::text_width(const char *text) const
{
	int x = 0;
	int width = 0;
	while (*text) {
		int i = (unsigned char) *text;

		if (i >= 32 && i < 127) {
			const Glyph *g = &m_glyphs[i - 32];
			x += g->x_advance;
		} else if (i == '\n') {
			if (x > width) width = x;
			x = 0;
		}
		++text;
	}
	if (x > width) width = x;
	return width;
}

void print_shader_log(GLuint obj)
{
	int len;
	if (glIsShader(obj)) {
		glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len);
	} else {
		glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
	}

	std::string buf(len, 0);
	int real_len = 0;
	if (glIsShader(obj)) {
		glGetShaderInfoLog(obj, len, &real_len, &buf[0]);
	} else {
		glGetProgramInfoLog(obj, len, &real_len, &buf[0]);
	}
	buf.resize(real_len);

	if (real_len > 0) {
		printf("---- SHADER LOG ----\n%s----------------\n",
			buf.c_str());
	}
}

Program::Program() :
	m_id(0)
{
}

void Program::load(const char *vertex_shader, const char *frag_shader)
{
	m_lookup.clear();

	GLuint vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vs, 1, &vertex_shader, NULL);
	glCompileShader(vs);
	print_shader_log(vs);

	GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fs, 1, &frag_shader, NULL);
	glCompileShader(fs);
	print_shader_log(fs);

	if (m_id == 0) {
		m_id = glCreateProgram();
		assert(m_id > 0);
	}

	glAttachShader(m_id, vs);
	glAttachShader(m_id, fs);
	glLinkProgram(m_id);
	print_shader_log(m_id);
	int status;
	glGetProgramiv(m_id, GL_LINK_STATUS, &status);
	if (!status) {
		throw std::runtime_error("Loading OpenGL program failed");
	}

	/* Many OpenGL implementations load shaders lazily, so force parsing
	 * the shader here to find out problem early and to avoid small pauses.
	 */
	glUseProgram(m_id);
	glUseProgram(0);
}

void Program::use() const
{
	assert(m_id > 0);
	glUseProgram(m_id);
}

GLint Program::uniform(const char *name)
{
	assert(m_id > 0);
	auto iter = m_lookup.find(name);
	GLint location;
	if (iter == m_lookup.end()) {
		location = glGetUniformLocation(m_id, name);
		if (location == -1) {
			throw std::runtime_error(std::string("Uniform not found: ") +
					   name);
		}
		m_lookup[name] = location;
	} else {
		location = iter->second;
	}
	return location;
}

void begin_rendering(size_t numlights, int flags, const vec3 &light)
{
	static Program simple_programs[MAX_LIGHTS + 1];
	static Program perpixel_programs[MAX_LIGHTS + 1];
	static Program shadow_program;
	if (!simple_programs[0].loaded()) {
		/* Hey, look! Self-modifying (shader) code */
		for (size_t i = 0; i <= MAX_LIGHTS; ++i) {
			char buf[sizeof simple_vs + 16];
			/* Some NVIDIA drivers can not arrays of length 1 */
			sprintf(buf, simple_vs, std::max<int>(i, 2), (int) i);
			simple_programs[i].load(buf, simple_fs);
		}
		for (size_t i = 0; i <= MAX_LIGHTS; ++i) {
			char buf[sizeof perpixel_fs + 16];
			sprintf(buf, perpixel_fs, std::max<int>(i, 2), (int) i);
			perpixel_programs[i].load(perpixel_vs, buf);
		}
		shadow_program.load(shadow_vs, shadow_fs);
	}
	assert(numlights <= MAX_LIGHTS);
	if (flags & RENDER_SHADOW_VOL) {
		current_program = &shadow_program;
		current_program->use();
		GLint loc = current_program->uniform("light");
		glUniform3fv(loc, 1, &light.x);

	} else if (quality >= 2 && !(flags & RENDER_BLOOM)) {
		current_program = &perpixel_programs[numlights];
		current_program->use();
	} else {
		current_program = &simple_programs[numlights];
		current_program->use();
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
}

void end_rendering()
{
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
	current_program = NULL;
}

void set_light(int n, const vec3 &pos, const Color &color, double brightness)
{
	assert(n >= 0 && n < (int) MAX_LIGHTS);
	assert(brightness > 0);

	char buf[64];
	sprintf(buf, "lights[%d].pos", n);
	GLint loc = current_program->uniform(buf);
	glUniform3fv(loc, 1, &pos.x);

	sprintf(buf, "lights[%d].diffuse", n);
	loc = current_program->uniform(buf);
	glUniform4fv(loc, 1, &color.r);

	sprintf(buf, "lights[%d].brightness", n);
	loc = current_program->uniform(buf);
	glUniform1f(loc, brightness);
}

void load_mtl(Mesh *mesh, const char *fname)
{
	printf("Loading materials: %s\n", fname);
	PackFile f;
	f.open(fname);

	Material *material = NULL;
	char line[256];
	size_t avail = 0;
	while (1) {
		line[avail] = 0;
		char *end = strchr(line, '\n');
		if (end == NULL) {
			avail += f.read(&line[avail], sizeof line - avail - 1);
			line[avail] = 0;
			end = strchr(line, '\n');
		}
		if (end != NULL) *end = 0;
		char *p = line;

		if (match(p, "newmtl")) {
			std::string name = token(p);
			auto iter = mesh->materials.find(name);
			if (iter == mesh->materials.end()) {
				material = new Material;
				material->frame = 0;
				material->num_frames = 0;
				material->num_cols = 0;
				material->brightness = 0;
				material->color = Color(1, 1, 1);
				material->light_color = Color(1, 1, 1);
				material->texture = NULL;
				mesh->materials[name] = material;
			} else {
				material = iter->second;
			}

		} else if (match(p, "Kd")) {
			assert(material != NULL);
			Color color(1, 1, 1);
			if (sscanf(p, "%f%f%f", &color.r, &color.g, &color.b) != 3) {
				throw std::runtime_error("Invalid color");
			}
			if (material->texture == NULL) {
				material->color = color;
				material->light_color = color;
			}

		} else if (match(p, "map_Kd")) {
			assert(material != NULL);
			std::string fname = token(p);
			material->texture = get_texture(fname.c_str());
			material->color = Color(1, 1, 1);
			material->light_color = material->texture->color();
		}

		if (end == NULL) break;
		++end;
		avail -= end - line;
		memmove(line, end, avail);
	}
}

void load_mesh(Mesh *mesh, const char *fname, double scale)
{
	printf("Loading %s\n", fname);
	PackFile f;
	f.open(fname);

	mesh->faces.clear();

	Material *material = NULL;
	std::vector<vec3> vert;
	std::vector<vec3> norm;
	std::vector<vec2> tc;

	char line[256];
	size_t avail = 0;
	while (1) {
		line[avail] = 0;
		char *end = strchr(line, '\n');
		if (end == NULL) {
			avail += f.read(&line[avail], sizeof line - avail - 1);
			line[avail] = 0;
			end = strchr(line, '\n');
		}
		if (end != NULL) *end = 0;
		char *p = line;

		if (match(p, "v")) {
			vec3 v;
			if (sscanf(p, "%f%f%f", &v.x, &v.y, &v.z) != 3) {
				throw std::runtime_error("invalid vertex");
			}
			vert.push_back(v * scale);

		} else if (match(p, "vn")) {
			vec3 v;
			if (sscanf(p, "%f%f%f", &v.x, &v.y, &v.z) != 3) {
				throw std::runtime_error("invalid vertex");
			}
			norm.push_back(v);

		} else if (match(p, "vt")) {
			vec2 v;
			if (sscanf(p, "%f%f", &v.x, &v.y) != 2) {
				throw std::runtime_error("invalid vertex");
			}
			v.y = 1 - v.y;
			tc.push_back(v);

		} else if (match(p, "mtllib")) {
			std::string fname = token(p);
			load_mtl(mesh, fname.c_str());
			material = NULL;

		} else if (match(p, "usemtl")) {
			auto iter = mesh->materials.find(token(p));
			if (iter == mesh->materials.end()) {
				throw std::runtime_error("Undefined material");
			}
			material = iter->second;

		} else if (match(p, "f")) {
			assert(material != NULL);
			Face f;
			f.mat = material;
			for (int i = 0; i < 3; ++i) {
				size_t idx = strtoul(p, &p, 10);
				if (idx <= 0 || idx > vert.size()) {
					throw std::runtime_error(std::string("Invalid vertex: ") +
								 line);
				}
				f.vert[i].vert = vert[idx - 1];
				if (*p == '/') {
					++p;
					if (*p != '/') {
						idx = strtoul(p, &p, 10);
						assert(idx > 0 &&
							idx <= tc.size());
						f.vert[i].tc = tc[idx - 1];
					} else {
						f.vert[i].tc = vec2(0, 0);
					}
				}
				if (*p == '/') {
					++p;
					idx = strtoul(p, &p, 10);
					assert(idx > 0 && idx <= norm.size());
					f.vert[i].norm = norm[idx - 1];
				} else {
					f.vert[i].norm = vec3(0, 0, 0);
				}
			}
			while (isspace(*p)) ++p;
			if (*p != 0) {
				throw std::runtime_error("Triangles expected");
			}
			mesh->faces.push_back(f);
		}

		if (end == NULL) break;
		++end;
		avail -= end - line;
		memmove(line, end, avail);
	}
}

void mult_matrix(const Matrix &m)
{
	float glm[] = {
		m.right.x, m.right.y, m.right.z, 0,
		m.up.x,    m.up.y,    m.up.z,    0,
		-m.forward.x, -m.forward.y, -m.forward.z, 0,
		0, 0, 0, 1
	};
	glMultMatrixf(glm);
}

void mult_matrix_reverse(const Matrix &m)
{
	float glm[] = {
		m.right.x, m.up.x, -m.forward.x, 0,
		m.right.y, m.up.y, -m.forward.y, 0,
		m.right.z, m.up.z, -m.forward.z, 0,
		0, 0, 0, 1
	};
	glMultMatrixf(glm);
}

const Texture *get_texture(const char *fname)
{
	auto iter = texture_cache.find(fname);
	if (iter == texture_cache.end()) {
		throw std::runtime_error(std::string("Texture not found: ") +
					 fname);
	}
	return iter->second;
}

void load_texture(const char *fname, bool mipmap, bool alpha)
{
	Texture *texture = new Texture;
	texture->load(fname, mipmap, alpha);
	texture_cache[fname] = texture;
}

const Model *get_model(const char *fname)
{
	auto iter = model_cache.find(fname);
	if (iter == model_cache.end()) {
		throw std::runtime_error(std::string("model not found: ") +
					 fname);
	}
	return iter->second;
}

void load_model(const char *fname, double scale, int first_frame,
		int num_frames, bool noise)
{
	Model *model = new Model;
	model->load(fname, scale, first_frame, num_frames, noise);
	model_cache[fname] = model;
}

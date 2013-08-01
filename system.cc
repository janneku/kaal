/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
/*
 * Game system. Ground level stuff like pack file I/O and window creation.
 */
#include "system.h"
#include "gfx.h"
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <map>

int scr_width, scr_height;
int quality = -1;
bool antialiasing;
Font small_font;
Font large_font;
Font order_font;
Font todo_font;

namespace {

struct PackEntry {
	size_t offset;
	size_t size;
};

std::unordered_map<std::string, PackEntry> pack_directory;
FILE *pack_file;
size_t pack_base;
SDL_Surface *screen;
bool show_fps;

}

double frand()
{
	return rand() * (1.0 / (RAND_MAX + 1.0)) +
	       rand() * (1.0 / (RAND_MAX + 1.0) / RAND_MAX);
}

bool get_event(SDL_Event *e)
{
	while (SDL_PollEvent(e)) {
		switch (e->type) {
		case SDL_KEYDOWN:
			switch (e->key.keysym.sym) {
			case SDLK_F4:
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				break;
			case SDLK_F5:
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				break;
			case SDLK_F6:
				quality = (quality + 1) % 3;
				break;
			case SDLK_F7:
				show_fps = !show_fps;
				break;
			default:
				break;
			}
			return true;

		case SDL_VIDEORESIZE:
			scr_width = e->resize.w;
			scr_height = e->resize.h;
			/* You can not resize the window on Windows, since you
			 * will end up losing OpenGL context along with all
			 * textures. But, seems that just using the new
			 * window size on resize works fine.
			 */
#ifndef _WIN32
			screen = SDL_SetVideoMode(scr_width, scr_height, 0,
						  SDL_OPENGL|SDL_RESIZABLE);
			if (screen == NULL) {
				throw std::runtime_error(
					std::string("Can not create a window: ") +
						    SDL_GetError());
			}
#endif
			glViewport(0, 0, scr_width, scr_height);
			break;
		case SDL_QUIT:
			throw std::runtime_error("Window closed");
		default:
			return true;
		}
	}
	return false;
}

/* A simple parser: Takes the next word from the stream (a byte array) */
std::string token(char *&p)
{
	while (isspace(*p)) ++p;
	size_t len = 0;
	while (!isspace(p[len]) && p[len] != 0) ++len;

	std::string token(p, len);
	p += len;
	return token;
}

/* A simple parser: Checks whether the next word in the stream matches */
bool match(char *&p, const char *token)
{
	while (isspace(*p)) ++p;
	size_t len = strlen(token);
	if (memcmp(p, token, len) == 0 &&
	    (isspace(p[len]) || p[len] == 0)) {
		p += len;
		return true;
	}
	return false;
}

PackFile::PackFile()
	: m_offset(0),
	m_base(0),
	m_size(0)
{
}

void PackFile::open(const char *fname)
{
	auto iter = pack_directory.find(fname);
	if (iter == pack_directory.end()) {
		throw std::runtime_error(std::string("Can not find ") +
					 fname);
	}
	PackEntry &entry = iter->second;
	m_offset = 0;
	m_base = pack_base + entry.offset;
	m_size = entry.size;
}

size_t PackFile::read(void *buf, size_t len)
{
	static SDL_mutex *lock;
	if (lock == NULL) {
		/* Protection for reading the pack file from multiple threads */
		lock = SDL_CreateMutex();
	}

	len = std::min(len, m_size - m_offset);
	if (len > 0) {
		SDL_mutexP(lock);
		fseek(pack_file, m_base + m_offset, SEEK_SET);
		if (fread(buf, 1, len, pack_file) < len) {
			SDL_mutexV(lock);
			throw std::runtime_error("Unable to read from pack file");
		}
		SDL_mutexV(lock);
	}
	m_offset += len;
	return len;
}

void PackFile::seek(size_t off)
{
	assert(off <= m_size);
	m_offset = off;
}

void open_pack(const char *fname)
{
	pack_file = fopen(fname, "rb");
	if (pack_file == NULL) {
		throw std::runtime_error(std::string("Can not open ") +
					 fname);
	}

	char line[256];
	fgets(line, sizeof line, pack_file);
	char *p = line;
	if (!match(p, "POKE 59458,62")) {
		throw std::runtime_error(std::string("Invalid pack file: ") +
					 fname);
	}

	size_t offset = 0;
	while (fgets(line, sizeof line, pack_file)) {
		char *p = line;
		if (match(p, "JMP FFFF:0000")) break;
		std::string name = token(p);

		PackEntry entry;
		entry.offset = offset;
		entry.size = strtoul(p, &p, 10);
		assert(entry.size > 0);
		pack_directory[name] = entry;

		offset += entry.size;
	}
	pack_base = ftell(pack_file);
}

void finish_draw()
{
	static int fps;
	static int frames;
	static Uint32 fps_tics;

	frames++;
	if (SDL_GetTicks() >= fps_tics + 1000) {
		fps = frames;
		frames = 0;
		fps_tics = SDL_GetTicks();
	}

	if (show_fps) {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, scr_width, scr_height, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glTranslatef(0, scr_height, 0);
		glColor3f(1, 1, 1);
		char buf[129];
		sprintf(buf, "%d faces %5d fps %5d MB memory", (int) faces_drawn, fps,
			((int) gfx_memory >> 20) + 1);
		small_font.draw_text(buf);
	}
	faces_drawn = 0;

	SDL_GL_SwapBuffers();
	check_gl_errors();
}

void init_system(bool windowed)
{
	int flags = SDL_OPENGL|SDL_RESIZABLE;
	int w = 0, h = 0;
	if (windowed) {
		w = 1000;
		h = 800;
	} else {
		flags |= SDL_FULLSCREEN;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	if (antialiasing) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
	}
	screen = SDL_SetVideoMode(w, h, 0, flags);
	if (screen == NULL) {
		throw std::runtime_error(std::string("Can not create a window: ") +
					 SDL_GetError());
	}

	std::string version = (const char *) glGetString(GL_VERSION);
	printf("GL version: %s\n", version.c_str());

	int samples;
	SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &samples);
	printf("Antialising: %d\n", samples);

	int depth, stencil;
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depth);
	if (depth < 16) {
		throw std::runtime_error("Unable to obtain a depth buffer");
	}
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil);
	if (stencil < 8) {
		throw std::runtime_error("Unable to obtain a stencil buffer");
	}
	printf("depth buffer: %d\n", depth);
	printf("stencil buffer: %d\n", stencil);

	scr_width = screen->w;
	scr_height = screen->h;
	glViewport(0, 0, scr_width, scr_height);

	small_font.load("small.fnt");
	large_font.load("large.fnt");
	order_font.load("orderfont.fnt");
	todo_font.load("todofont.fnt");
}

void load_settings()
{
	FILE *f = fopen("kaal.cfg", "rb");
	if (f == NULL) {
		return;
	}

	char line[256];
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		if (match(p, "quality")) {
			quality = strtol(p, &p, 10);

		} else if (match(p, "antialiasing")) {
			antialiasing = strtol(p, &p, 10) > 0;
		}
	}
	fclose(f);
}

void write_settings()
{
	FILE *f = fopen("kaal.cfg", "wb");
	if (f == NULL) {
		return;
	}
	fprintf(f, "quality %d\n", quality);
	fprintf(f, "antialiasing %d\n", antialiasing);
	fclose(f);
}

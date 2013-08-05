/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#ifndef __system_h
#define __system_h

#include <SDL.h>
#include <string>

class Font;

class PackFile {
public:
	size_t offset() const { return m_offset; }
	size_t size() const { return m_size; }

	PackFile();
	void open(const char *fname);
	size_t read(void *buf, size_t len);
	void seek(size_t off);

private:
	size_t m_offset;
	size_t m_base;
	size_t m_size;
};

extern int scr_width, scr_height;
extern int quality;
extern bool antialiasing;
extern bool invert_mouse;
extern Font small_font;
extern Font large_font;
extern Font order_font;
extern Font todo_font;

double frand();
bool get_event(SDL_Event *e);
std::string token(char *&p);
bool match(char *&p, const char *token);
void open_pack(const char *fname);
void finish_draw();
void init_system(bool windowed);
void load_settings();
void write_settings();

#endif

/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#include "effects.h"
#include "gfx.h"
#include "system.h"

namespace {

FBO bloom1, bloom2, bloom_out;
FBO large_bloom1, large_bloom2, large_bloom_out;

const char simple_vs[] =
"varying vec2 tc;\
void main(void)\
{\
	gl_Position = ftransform();\
	tc = gl_MultiTexCoord0.xy;\
}";

const char bloom1_source[] =
"#extension GL_ARB_texture_rectangle : enable\n\
uniform sampler2DRect tex;\
varying vec2 tc;\
void main(void)\
{\
	vec4 sum = vec4(0.0);\
	int i;\
	float w;\
	for (i = -14; i < 15; i++) {\
		w = 1.0 - abs(float(i) * 0.065);\
		sum = max(sum, texture2DRect(tex, tc + vec2(float(i), 0.0)) * w);\
	}\
	gl_FragColor = sum;\
	gl_FragColor.a = 1.0;\
}";

const char bloom2_source[] =
"#extension GL_ARB_texture_rectangle : enable\n\
uniform sampler2DRect tex;\
varying vec2 tc;\
void main(void)\
{\
	vec4 sum = vec4(0.0);\
	int i;\
	float w;\
	for (i = -14; i < 15; i++) {\
		w = 1.0 - abs(float(i) * 0.065);\
		sum = max(sum, texture2DRect(tex, tc + vec2(0.0, float(i))) * w);\
	}\
	gl_FragColor = sum;\
	gl_FragColor.a = 1.0;\
}";

void run_filter(int width, int height)
{
	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex3f(0, 0, 0);
	glTexCoord2f(0, height);
	glVertex3f(0, 1, 0);
	glTexCoord2f(width, height);
	glVertex3f(1, 1, 0);
	glTexCoord2f(width, 0);
	glVertex3f(1, 0, 0);
	glEnd();
}

}

void begin_bloom()
{
	static int bloom_w = 0, bloom_h = 0;
	if (bloom_w != scr_width || bloom_h != scr_height) {
		bloom1.init(scr_width/2, scr_height/2, false, true, true);
		bloom2.init(scr_width/2, scr_height/2, false);
		bloom_out.init(scr_width/2, scr_height/2, true);
		large_bloom1.init(scr_width/8, scr_height/8, false);
		large_bloom2.init(scr_width/8, scr_height/8, false);
		large_bloom_out.init(scr_width/8, scr_height/8, true);
		bloom_w = scr_width;
		bloom_h = scr_height;
	}

	bloom1.begin_drawing();
	glViewport(0, 0, scr_width/2, scr_height/2);
}

/* Will mess matrixes and viewport */
void end_bloom()
{
	static Program bloom1_program, bloom2_program;
	if (!bloom1_program.loaded()) {
		bloom1_program.load(simple_vs, bloom1_source);
		bloom2_program.load(simple_vs, bloom2_source);
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	bloom1_program.use();
	bloom1.bind_texture();
	bloom2.begin_drawing();
	run_filter(scr_width/2, scr_height/2);

	bloom2_program.use();
	bloom2.bind_texture();
	bloom_out.begin_drawing();
	run_filter(scr_width/2, scr_height/2);
	glUseProgram(0);

	glViewport(0, 0, scr_width/8, scr_height/8);
	large_bloom1.begin_drawing();
	{
		GLState gl;
		glColor4f(1, 1, 1, 1);
		bloom_out.bind_texture();
		gl.enable(GL_TEXTURE_RECTANGLE_ARB);
		run_filter(scr_width/2, scr_height/2);
	}

	bloom1_program.use();
	large_bloom1.bind_texture();
	large_bloom2.begin_drawing();
	run_filter(scr_width/8, scr_height/8);

	bloom2_program.use();
	large_bloom2.bind_texture();
	large_bloom_out.begin_drawing();
	run_filter(scr_width/8, scr_height/8);

	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, scr_width, scr_height);
}

void draw_bloom()
{
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 1, 0, 1, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	GLState gl;
	gl.enable(GL_BLEND);
	gl.enable(GL_TEXTURE_RECTANGLE_ARB);
	bloom_out.bind_texture();
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(1, 1, 1, 0.4);
	run_filter(scr_width/2, scr_height/2);

	large_bloom_out.bind_texture();
	glColor4f(1, 1, 1, 0.4);
	run_filter(scr_width/8, scr_height/8);
}

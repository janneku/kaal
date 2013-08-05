/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * lgpl-2.1.txt file.
 */
#ifndef __video_h__
#define __video_h__

#include <theora/theoradec.h>
#include "system.h"
#include "gfx.h"

/* NOTE: Not safe to copy when decoding a video */
struct Video {
	static const size_t FRAMES = 32; /* must be power-of-two */

public:
	int width() const { return m_width; }
	int height() const { return m_height; }
	bool playing() const { return m_ctx != NULL; }
	const Texture *get_frame() { return &m_texture; }

	Video();
	void play(const char *fname);
	void stop();
	void advance();

private:
	ogg_sync_state m_sync;
	ogg_stream_state m_stream;
	th_dec_ctx *m_ctx;
	PackFile m_file;
	std::vector<uint8_t> m_queue[FRAMES];
	volatile size_t m_head, m_tail;
	int m_width, m_height;
	volatile bool m_end;
	volatile bool m_reset;
	SDL_Thread *m_thread;
	Texture m_texture;

	bool get_packet(ogg_packet *packet);
	void decode_frame(ogg_packet *packet);
	void decode_thread();
	static int thread_wrapper(void *ptr);
};

#endif


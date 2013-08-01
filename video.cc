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
 * A class that decodes Ogg Vorbis video in a background thread and
 * hands out the frames in an OpenGL texture. The decoded frames are held
 * in a ring buffer.
 */
#include "video.h"
#include <stdexcept>
#include <assert.h>

Video::Video() :
	m_ctx(NULL),
	m_thread(NULL)
{
	memset(&m_stream, 0, sizeof m_stream);
}

void Video::play(const char *fname)
{
	stop();

	printf("Playing %s\n", fname);
	m_file.open(fname);
	ogg_sync_init(&m_sync);

	th_info info;
	th_comment comment;
	th_setup_info *setup = NULL;
	th_info_init(&info);
	th_comment_init(&comment);

	ogg_packet packet;
	while (1) {
		if (!get_packet(&packet)) {
			throw std::runtime_error("Unexpected OGG end of file");
		}
		int res = th_decode_headerin(&info, &comment, &setup, &packet);
		if (res == 0) {
			break;
		} else if (res < 0) {
			throw std::runtime_error("Invalid Theora headers");
		}
	}

	m_ctx = th_decode_alloc(&info, setup);
	assert(m_ctx != NULL);
	m_width = info.pic_width;
	m_height = info.pic_height;
	th_comment_clear(&comment);
	th_setup_free(setup);

	m_head = 0;
	m_tail = 0;
	decode_frame(&packet);

	/* Hand over the decoder context to the thread */
	m_end = false;
	m_reset = false;
	m_thread = SDL_CreateThread(thread_wrapper, this);
	assert(m_thread != NULL);
}

void Video::stop()
{
	if (m_ctx != NULL) {
		m_reset = true;
		SDL_WaitThread(m_thread, NULL);
		th_decode_free(m_ctx);
		ogg_stream_clear(&m_stream);
		ogg_sync_clear(&m_sync);
	}
	m_ctx = NULL;
}

bool Video::get_packet(ogg_packet *packet)
{
	/* This is how FFmpeg handles libogg */
	while (ogg_stream_packetout(&m_stream, packet) != 1) {
		ogg_page page;
		while (ogg_sync_pageout(&m_sync, &page) != 1) {
			char *buffer = ogg_sync_buffer(&m_sync, 4096);
			size_t got = m_file.read(buffer, 4096);
			if (got == 0) {
				return false;
			}
			ogg_sync_wrote(&m_sync, got);
		}
		if (m_stream.body_data == NULL) {
			/* Assumes the first stream is the video */
			int serial = ogg_page_serialno(&page);
			if (ogg_stream_init(&m_stream, serial)) {
				assert(0);
			}
		}
		if (ogg_stream_pagein(&m_stream, &page)) {
			throw std::runtime_error("Invalid OGG stream");
		}
	}
	return true;
}

void Video::decode_frame(ogg_packet *packet)
{
	th_img_plane buffer[3];
	if (th_decode_packetin(m_ctx, packet, NULL)) {
		throw std::runtime_error("Unable decode a Theora frame");
	}
	th_decode_ycbcr_out(m_ctx, buffer);

	assert(buffer[0].width == m_width && buffer[0].height == m_height);
	assert(buffer[1].width*2 == m_width && buffer[1].height*2 == m_height);
	assert(buffer[2].width*2 == m_width && buffer[2].height*2 == m_height);

	std::vector<uint8_t> &frame = m_queue[m_head];
	frame.resize(m_width * m_height * 3);

	for (int y = 0; y < m_height; y++) {
		const uint8_t *ptry = &buffer[0].data[y * buffer[0].stride];
		const uint8_t *ptru = &buffer[1].data[y/2 * buffer[1].stride];
		const uint8_t *ptrv = &buffer[2].data[y/2 * buffer[2].stride];
		uint8_t *out = &frame[y * m_width * 3];

		for (int x = 0; x < m_width/2; x++) {
			int pr = (-56992 + ptrv[x] * 409) >> 8;
			int pg = (34784 - ptru[x] * 100 - ptrv[x] * 208) >> 8;
			int pb = (-70688 + ptru[x] * 516) >> 8;

			int y = 298*ptry[x*2] >> 8;
			int r = y + pr;
			int g = y + pg;
			int b = y + pb;
			out[x*6 + 0] = std::max(std::min(r, 255), 0);
			out[x*6 + 1] = std::max(std::min(g, 255), 0);
			out[x*6 + 2] = std::max(std::min(b, 255), 0);

			y = 298*ptry[x*2 + 1] >> 8;
			r = y + pr;
			g = y + pg;
			b = y + pb;
			out[x*6 + 3] = std::max(std::min(r, 255), 0);
			out[x*6 + 4] = std::max(std::min(g, 255), 0);
			out[x*6 + 5] = std::max(std::min(b, 255), 0);
		}
	}
	/* Increment the write pointer atomically */
	m_head = (m_head + 1) % FRAMES;
}

void Video::advance()
{
	assert(m_ctx != NULL);
	/* Do we have a frame? */
	if (m_head == m_tail) {
		if (m_end) {
			stop();
		}
		return;
	}

	std::vector<uint8_t> &frame = m_queue[m_tail];
	m_texture.load(m_width, m_height, GL_RGB, &frame[0]);

	/* Increment the read pointer atomically */
	m_tail = (m_tail + 1) % FRAMES;
}

void Video::decode_thread()
{
	while (!m_reset) {
		/* Wait until there is more room */
		while ((m_head + 1) % FRAMES == m_tail) {
			SDL_Delay(10);
			if (m_reset) return;
		}

		ogg_packet packet;
		if (!get_packet(&packet)) {
			/* Set the end flag atomically */
			m_end = true;
			break;
		}
		decode_frame(&packet);
	}

}

/* A wrapper that converts a callback into a call of a C++ object method */
int Video::thread_wrapper(void *ptr)
try {
	Video *video = (Video *) ptr;

	video->decode_thread();
	return 0;

} catch (const std::runtime_error &e) {
	fprintf(stderr, "ERROR in video decoder: %s\n", e.what());
	return 0;
}

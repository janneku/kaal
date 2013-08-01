/*
 * KAAL
 *
 * Copyright 2013 Janne Kulmala <janne.t.kulmala@iki.fi>,
 * Antti Rajamäki <amikaze@gmail.com>
 *
 * Program code and resources are licensed with GNU LGPL 2.1. See
 * COPYING.LGPL file.
 */
#ifndef __sound_h__
#define __sound_h__

#include <stdint.h>
#include <vector>
#include <vorbis/vorbisfile.h>
#include "system.h"

const int SAMPLERATE = 44100;

struct Sample {
	int16_t left, right;
};

/* NOTE, not safe to copy when the music is playing! */
class Music {
	static const size_t BUFFER_SIZE = 2048;

public:
	double length() const { return m_length; }
	bool playing() const { return m_vf.datasource != NULL; }

	void set_volume(double v);
	void set_pan(double v);

	Music();
	void play(const char *fname, bool looping, double volume = 1);
	void stop();
	void seek(double t);

	void mix(Sample *buf, size_t len);

private:
	PackFile m_file;
	OggVorbis_File m_vf;
	double m_volume;
	double m_pan;
	bool m_looping;
	Sample m_buffer[BUFFER_SIZE];
	size_t m_avail;
	double m_length;
	void fill_buffer();
};

/* NOTE, not safe to move or destroy when the sound is playing */
class Sound {
public:
	bool loaded() const { return !m_data.empty(); }
	double length() const { return (double) m_data.size() / SAMPLERATE; }

	void load(const char *fname);

	size_t mix(size_t pos, Sample *buf, size_t len, double volume,
		   double pan) const;

private:
	std::vector<Sample> m_data;
};

double get_music_time();
void play_sound(const Sound *sound, double volume = 1, double pan = 1);
void kill_sounds();
void init_sound();
const Sound *get_sound(const char *fname);
void load_sound(const char *fname);

#endif

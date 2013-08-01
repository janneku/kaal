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
 * The sound system. Can play multiple sounds and music tracks at the
 * same time. The system is driven by a background thread that mixes
 * playing sounds together in to a buffer that is written the audio
 * device. Sounds are stored in memory as undecoded for quick access,
 * while music is decoded on the fly.
 */
#include "sfx.h"
#include "system.h"
#include <SDL.h>
#include <unordered_map>
#include <list>
#include <assert.h>
#include <stdexcept>

namespace {

struct Playing {
	const Sound *sound;
	size_t pos;
	double volume;
	double pan;
};

const size_t MAX_DELAY = 50;

std::list<Playing> playing;
std::list<Music *> music;
std::unordered_map<std::string, Sound *> sound_cache;

/* Note, this is called from a background thread with the audio lock held. */
void fill_audio(void *userdata, Uint8 *bytebuf, int len)
try {
	(void) userdata;
	assert(len % sizeof(Sample) == 0);
	len /= sizeof(Sample);
	Sample *buf = (Sample *) bytebuf;

	memset(buf, 0, len * sizeof(Sample));

	/* Music objects can remove themselves from the list */
	for (auto iter = music.begin(); iter != music.end();) {
		Music *mus = *iter;
		iter++;

		mus->mix(buf, len);
	}

	for (auto iter = playing.begin(); iter != playing.end();) {
		auto this_iter = iter;
		Playing &play = *iter;
		iter++;

		size_t got = play.sound->mix(play.pos, buf, len, play.volume,
					     play.pan);
		play.pos += got;
		if (got < (size_t) len) {
			playing.erase(this_iter);
		}
	}

} catch (const std::runtime_error &e) {
	fprintf(stderr, "ERROR in audio fill: %s\n", e.what());
}

size_t read_cb(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	PackFile *f = (PackFile *) datasource;
	return f->read(ptr, size * nmemb) / size;
}

int seek_cb(void *datasource, ogg_int64_t off, int whence)
{
	PackFile *f = (PackFile *) datasource;
	switch (whence) {
	case SEEK_SET:
		f->seek(off);
		break;
	case SEEK_CUR:
		f->seek(off + f->offset());
		break;
	case SEEK_END:
		f->seek(off + f->size());
		break;
	default:
		assert(0);
	}
	return f->offset();
}

long tell_cb(void *datasource)
{
	PackFile *f = (PackFile *) datasource;
	return f->offset();
}

const ov_callbacks callbacks = {
	read_cb,
	seek_cb,
	NULL,
	tell_cb,
};

}

Music::Music() :
	m_volume(1),
	m_pan(1),
	m_avail(0),
	m_length(0)
{
	memset(&m_vf, 0, sizeof m_vf);
}

void Music::set_volume(double v)
{
	assert(v >= 0);
	m_volume = v;
}

void Music::set_pan(double v)
{
	assert(v >= 0 && v <= 2);
	m_pan = v;
}

void Music::play(const char *fname, bool looping, double volume)
{
	stop();

	printf("Playing %s\n", fname);
	m_file.open(fname);
	if (ov_open_callbacks(&m_file, &m_vf, NULL, 0, callbacks)) {
		throw std::runtime_error(std::string("Invalid vorbis file: ") +
					 fname);
	}
	m_avail = 0;
	m_pan = 1;
	m_volume = volume;
	m_looping = looping;
	m_length = ov_time_total(&m_vf, -1);
	SDL_LockAudio();
	music.push_back(this);
	SDL_UnlockAudio();
}

void Music::stop()
{
	SDL_LockAudio();
	music.remove(this);
	SDL_UnlockAudio();

	if (m_vf.datasource != NULL) {
		ov_clear(&m_vf);
	}
}

void Music::seek(double t)
{
	SDL_LockAudio();
	ov_time_seek(&m_vf, t);
	SDL_UnlockAudio();
}

/* Assumes audio lock is held */
void Music::fill_buffer()
{
	while (m_avail < BUFFER_SIZE) {
		int bitstream = 0;
		long got = ov_read(&m_vf, (char *) &m_buffer[m_avail],
				   (BUFFER_SIZE - m_avail) * sizeof(Sample),
				   0, 2, 1, &bitstream);
		if (got < 0) {
			throw std::runtime_error("Corrupted vorbis file");
		} else if (got == 0) {
			if (!m_looping) {
				music.remove(this);
				ov_clear(&m_vf);
				return;
			}
			ov_time_seek(&m_vf, 0);
			return;
		}
		vorbis_info *vi = ov_info(&m_vf, -1);
		if (vi->channels != 2 || vi->rate != SAMPLERATE) {
			throw std::runtime_error("Invalid music format");
		}
		got /= sizeof(Sample);
		m_avail += got;
	}
}

void Music::mix(Sample *buf, size_t len)
{
	assert(m_vf.datasource != NULL);

	double l = std::min(2 - m_pan, 1.0);
	double r = std::min(m_pan, 1.0);

	/* Avoid floating point inside the loop */
	int left_vol = l * m_volume * 256;
	int right_vol = r * m_volume * 256;
	int left_delay = l * (MAX_DELAY - 1);
	int right_delay = r * (MAX_DELAY - 1);

	while (len > 0) {
		while (m_avail <= MAX_DELAY) {
			if (m_vf.datasource == NULL) return;
			fill_buffer();
		}

		size_t chunk = std::min(len, m_avail - MAX_DELAY);
		for (size_t i = 0; i < chunk; ++i) {
			int v = buf[i].left +
				m_buffer[i + left_delay].left * left_vol / 256;
			buf[i].left = std::max(std::min(v, 0x7fff), -0x8000);
			v = buf[i].right +
				m_buffer[i + right_delay].right * right_vol / 256;
			buf[i].right = std::max(std::min(v, 0x7fff), -0x8000);
		}
		m_avail -= chunk;
		memmove(m_buffer, &m_buffer[chunk], m_avail * sizeof(Sample));
		buf += chunk;
		len -= chunk;
	}
}

void play_sound(const Sound *sound, double volume, double pan)
{
	Playing p;
	p.sound = sound;
	p.pos = 0;
	p.volume = volume;
	p.pan = pan;
	SDL_LockAudio();
	playing.push_back(p);
	SDL_UnlockAudio();
}

void Sound::load(const char *fname)
{
	printf("Loading %s\n", fname);
	PackFile f;
	f.open(fname);
	OggVorbis_File vf;
	if (ov_open_callbacks(&f, &vf, NULL, 0, callbacks)) {
		throw std::runtime_error(std::string("Invalid vorbis file: ") +
					 fname);
	}
	m_data.clear();
	size_t pos = 0;
	m_data.resize(4096);
	while (1) {
		if (pos >= m_data.size()) {
			m_data.resize(pos * 2);
		}

		int bitstream = 0;
		long got = ov_read(&vf, (char *) &m_data[pos],
				   (m_data.size() - pos) * sizeof(Sample),
				   0, 2, 1, &bitstream);
		if (got < 0) {
			throw std::runtime_error("Corrupted vorbis file");
		} else if (got == 0) {
			break;
		}
		vorbis_info *vi = ov_info(&vf, -1);
		if (vi->channels != 2 || vi->rate != SAMPLERATE) {
			throw std::runtime_error(std::string("Invalid sound format: ") +
						 fname);
		}
		got /= sizeof(Sample);
		pos += got;
	}
	m_data.resize(pos);
	assert(m_data.size() >= MAX_DELAY);
	ov_clear(&vf);
}

size_t Sound::mix(size_t pos, Sample *buf, size_t len, double volume,
		  double pan) const
{
	assert(pan >= 0 && pan <= 2);
	assert(pos <= m_data.size() - MAX_DELAY);

	double l = std::min(2 - pan, 1.0);
	double r = std::min(pan, 1.0);

	/* Avoid floating point inside the loop */
	int left_vol = l * volume * 256;
	int right_vol = r * volume * 256;
	int left_delay = l * (MAX_DELAY - 1);
	int right_delay = r * (MAX_DELAY - 1);

	len = std::min(len, m_data.size() - MAX_DELAY - pos);
	for (size_t i = 0; i < len; ++i) {
		int v = buf[i].left +
			m_data[pos + i + left_delay].left * left_vol / 256;
		buf[i].left = std::max(std::min(v, 0x7fff), -0x8000);
		v = buf[i].right +
			m_data[pos + i + right_delay].right * right_vol / 256;
		buf[i].right = std::max(std::min(v, 0x7fff), -0x8000);
	}
	return len;
}

void kill_sounds()
{
	SDL_LockAudio();
	std::list<Music *> saved_music = music;
	music.clear();
	playing.clear();
	SDL_UnlockAudio();

	for (Music *mus : saved_music) {
		mus->stop();
	}
}

void init_sound()
{
	if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		throw std::runtime_error(
			std::string("Can not intialize audio: ") +
			SDL_GetError());
	}

	SDL_AudioSpec spec;
	spec.freq = SAMPLERATE;
	spec.format = AUDIO_S16SYS;
	spec.channels = 2;
	spec.samples = SAMPLERATE / 40;
	spec.callback = fill_audio;
	if (SDL_OpenAudio(&spec, NULL)) {
		throw std::runtime_error(
			std::string("Can not intialize audio: ") +
			SDL_GetError());
	}
	SDL_PauseAudio(0);
}

const Sound *get_sound(const char *fname)
{
	auto iter = sound_cache.find(fname);
	if (iter == sound_cache.end()) {
		throw std::runtime_error(std::string("Sound not found: ") +
					 fname);
	}
	return iter->second;
}

void load_sound(const char *fname)
{
	Sound *sound = new Sound;
	sound->load(fname);
	sound_cache[fname] = sound;
}

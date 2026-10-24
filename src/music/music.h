#ifndef MUSIC_H
#define MUSIC_H

#include <unordered_map>
#include <vector>
#include "music/audio.h"
#include "common/String.h"

class SoundHandler;

namespace NOTE {
	// music.cpp
	extern bool enabled;

	// Map of piano key note: frequency
	extern const std::unordered_map<int, float> key_map;

    float get_frequency_from_key(int key);
	String get_note_name_from_key(int key);

	// audio.cpp
	extern SoundHandler * sound_handler;
	SoundHandler * get_sound();

	const size_t MAX_NOTES_AT_SAME_TIME = 2; // Higher might cause cutting off and crackling noises
	const int MIN_LENGTH = 5; 	 // Min length of note allowed (in number of callbacks)
	const int MAX_LENGTH = 500;  // Max length of note allowed
}

#endif
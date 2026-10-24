#ifndef AUDIO_H
#define AUDIO_H

//#include <SDL2/SDL_audio.h> *ULTIMATA97*
#include <SDL2/SDL.h>
#include <list>
#include <vector>

// Imagine trying to figure out why int won't convert to enum
// yeah me neither

// Instrument type checking is done in luatpt_play_sound in lua/LegacyLuaAPI.cpp
// If this list changes update there too
typedef int InstrumentType;
const int SQUARE = 0;
const int TRIANGLE = 1;
const int SAW = 2;
const int SINE = 3;
const int VIOLIN = 4;

class Note {
public:
    unsigned int start, end;
    float freq;
    InstrumentType type;
    int count = 0;
    Note(int start, int end, float freq, InstrumentType instrument_type) :
        start(start), end(end), freq(freq), type(instrument_type) {}
};

class SoundHandler {
public:
    SoundHandler();
    ~SoundHandler();

    void add_sound(float freq, int length, InstrumentType instrument);
    void play();
    void stop();
private:
    static void SDLAudioCallback(void *data, Uint8 *buffer, int length);

    SDL_AudioDeviceID m_device = 0;
    Uint8 m_silence = 128;

    double m_sampleFreq;
    unsigned int callbacks = 0;
    std::vector<Note *> frequencies;
};

#endif

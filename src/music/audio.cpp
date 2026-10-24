#include "music/audio.h"
#include "music/music.h"
#include "music/synth/violin.h"

#include <cmath>
//#include <SDL2/SDL_audio.h> *ULTIMATA97*
#include <SDL2/SDL.h>
#include <iostream>
#include <algorithm> // VS is retarded and can't find this shit

#include "common/tpt-compat.h"

float interpolate(float a, float b, float i) {
    return  (a * i + b * (1 - i)) / 2;
}

namespace NOTE {
    SoundHandler * sound_handler = nullptr;

    // We can't assign sound_handler now because SDL audio isn't
    // initialized yet, so we only create it when it's needed (by then
    // the sim will be up and audio will be initialized)
    SoundHandler * get_sound() {
        if (!sound_handler)
            sound_handler = new SoundHandler();
        return sound_handler;
    }
}

SoundHandler::SoundHandler()
    : m_sampleFreq(44100) {
    // TPT Ultimata 97 left audio output commented out (*ULTIMATA97*), so NOTE was silent; this port opens
    // the device again. SDL's audio subsystem is initialised here, on first use, not at startup.
    if (!SDL_WasInit(SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        std::cout << "Sound unavailable: " << SDL_GetError() << std::endl;
        return;
    }
    SDL_AudioSpec wantSpec, haveSpec;
    SDL_zero(wantSpec);
    wantSpec.freq = (int)m_sampleFreq;
    wantSpec.format = AUDIO_U8;
    wantSpec.channels = 1;
    wantSpec.samples = 1024;
    wantSpec.callback = SDLAudioCallback;
    wantSpec.userdata = this;
    m_device = SDL_OpenAudioDevice(NULL, 0, &wantSpec, &haveSpec, 0);
    if (m_device == 0) {
        std::cout << "Sound unavailable: " << SDL_GetError() << std::endl;
        return;
    }
    m_silence = haveSpec.silence;
    SDL_PauseAudioDevice(m_device, 0);
}

SoundHandler::~SoundHandler() {
    this->stop();
    if (m_device)
        SDL_CloseAudioDevice(m_device);
    for (auto *note : frequencies)
        delete note;
}

void SoundHandler::play() {
}

void SoundHandler::stop() {
    if (m_device)
        SDL_LockAudioDevice(m_device);
    for (auto *note : frequencies)
        delete note;
    frequencies.clear();
    this->callbacks = 0;
    if (m_device)
        SDL_UnlockAudioDevice(m_device);
}

void SoundHandler::add_sound(float freq, int length, InstrumentType instrument) {
    if (!NOTE::enabled || !m_device || freq <= 0) return;
    if (length <= NOTE::MIN_LENGTH)
        length = NOTE::MIN_LENGTH;
    if (length >= NOTE::MAX_LENGTH)
        length = NOTE::MAX_LENGTH;

    // the callback runs on SDL's audio thread on desktop builds
    SDL_LockAudioDevice(m_device);
    bool found = false;
    for (size_t i = 0; i < frequencies.size(); i++)
        if (frequencies[i]->freq == freq && frequencies[i]->type == instrument) {
            frequencies[i]->end = this->callbacks + length;
            found = true;
            break;
        }
    if (!found)
        frequencies.push_back(new Note(this->callbacks, this->callbacks + length, freq, instrument));
    SDL_UnlockAudioDevice(m_device);
}

void SoundHandler::SDLAudioCallback(void *data, Uint8 *buffer, int length) {
    SoundHandler * sound = reinterpret_cast<SoundHandler*>(data);
    std::fill(&buffer[0], &buffer[length], sound->m_silence);

    for (int i = sound->frequencies.size() - 1; i >= 0; i--) {
        if (sound->frequencies[i]->end <= sound->callbacks) {
            delete sound->frequencies[i];
            sound->frequencies.erase(sound->frequencies.begin() + i);
        }
    }
    sound->callbacks++;
    if (sound->frequencies.size() == 0)
        return;

    // Each voice is in -1..1; the mix is scaled well below full scale (Ultimata's description warned
    // that it was very loud).
    const float volume = 0.18f;
    for (int i = 0; i < length; ++i) {
        float tone = 0.0f;
        for (size_t j = 0; j < std::min(NOTE::MAX_NOTES_AT_SAME_TIME, sound->frequencies.size()); j++) {
            Note * note = sound->frequencies[j];
            float freq = note->freq;
            float samples_per_wave = sound->m_sampleFreq / freq;
            float phase = note->count / samples_per_wave - std::floor(note->count / samples_per_wave);

            switch (note->type) {
                case VIOLIN: {
                    float scaled = note->count * freq / 440.0f;
                    float a = violin_buffer[(int)scaled % violin_size], b = violin_buffer[((int)scaled + 1) % violin_size];
                    float f = scaled - (int)scaled;
                    tone += ((a * (1 - f) + b * f) - 128.0f) / 128.0f;
                    break;
                }
                case SAW:
                    tone += phase * 2.0f - 1.0f;
                    break;
                case SQUARE:
                    tone += phase < 0.5f ? 1.0f : -1.0f;
                    break;
                case TRIANGLE:
                    tone += 1.0f - 4.0f * std::fabs(phase - 0.5f);
                    break;
                case SINE:
                default:
                    tone += std::sin(phase * float(M_PI) * 2.0f);
                    break;
            }
            note->count++;
        }
        int sample = int(sound->m_silence) + int(tone * volume * 127.0f);
        buffer[i] = (Uint8)std::max(0, std::min(255, sample));
    }
}

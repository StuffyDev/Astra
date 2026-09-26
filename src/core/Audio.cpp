#include "core/Audio.h"
#include <miniaudio.h>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace {

struct Voice {
    ma_sound sound;
    bool alive = false;
    bool loop = false;
};

ma_engine g_Engine;
bool g_EngineReady = false;
bool g_Muted = false;
float g_MasterVolume = 1.0f;
uint32_t g_NextId = 1;
std::unordered_map<uint32_t, Voice> g_Voices;
std::string g_LastError;

void StopVoice(Voice& v) {
    if (!v.alive) return;
    ma_sound_stop(&v.sound);
    ma_sound_uninit(&v.sound);
    v.alive = false;
}

} // namespace

bool Audio::Init() {
    if (g_EngineReady) return true;
    ma_engine_config cfg = ma_engine_config_init();
    if (ma_engine_init(&cfg, &g_Engine) != MA_SUCCESS) {
        g_LastError = "ma_engine_init failed";
        std::cerr << "[Audio] " << g_LastError << "\n";
        return false;
    }
    ma_engine_set_volume(&g_Engine, g_Muted ? 0.0f : g_MasterVolume);
    g_EngineReady = true;
    return true;
}

void Audio::Shutdown() {
    StopAll();
    if (g_EngineReady) {
        ma_engine_uninit(&g_Engine);
        g_EngineReady = false;
    }
}

uint32_t Audio::Play(const std::string& path, float volume, float pitch, bool loop) {
    if (!Init()) return 0;
    Voice v;
    if (ma_sound_init_from_file(&g_Engine, path.c_str(), 0, nullptr, nullptr, &v.sound) != MA_SUCCESS) {
        g_LastError = "Failed to load sound: " + path;
        std::cerr << "[Audio] " << g_LastError << "\n";
        return 0;
    }
    v.alive = true;
    v.loop = loop;
    uint32_t id = g_NextId++;
    ma_sound_set_volume(&v.sound, volume);
    ma_sound_set_pitch(&v.sound, pitch);
    ma_sound_start(&v.sound);
    g_Voices[id] = v;
    return id;
}

uint32_t Audio::PlayOneShot(const std::string& path, float volume, float pitch) {
    return Play(path, volume, pitch, false);
}

uint32_t Audio::PlayLooped(const std::string& path, float volume, float pitch) {
    return Play(path, volume, pitch, true);
}

void Audio::Stop(uint32_t id) {
    auto it = g_Voices.find(id);
    if (it == g_Voices.end()) return;
    StopVoice(it->second);
    g_Voices.erase(it);
}

void Audio::SetVolume(uint32_t id, float volume) {
    auto it = g_Voices.find(id);
    if (it != g_Voices.end() && it->second.alive)
        ma_sound_set_volume(&it->second.sound, volume);
}

void Audio::SetPitch(uint32_t id, float pitch) {
    auto it = g_Voices.find(id);
    if (it != g_Voices.end() && it->second.alive)
        ma_sound_set_pitch(&it->second.sound, pitch);
}

void Audio::StopAll() {
    for (auto& [id, v] : g_Voices) StopVoice(v);
    g_Voices.clear();
}

void Audio::NewFrame() {
    if (!g_EngineReady) return;
    std::vector<uint32_t> done;
    for (auto& [id, v] : g_Voices) {
        if (!v.alive) continue;
        if (ma_sound_at_end(&v.sound)) {
            if (v.loop) {
                ma_sound_seek_to_pcm_frame(&v.sound, 0);
                ma_sound_start(&v.sound);
            } else {
                done.push_back(id);
            }
        }
    }
    for (uint32_t id : done) Stop(id);
}

void Audio::SetMasterVolume(float volume) {
    g_MasterVolume = volume;
    if (g_EngineReady) ma_engine_set_volume(&g_Engine, g_Muted ? 0.0f : g_MasterVolume);
}

float Audio::MasterVolume() {
    return g_MasterVolume;
}

void Audio::SetMuted(bool muted) {
    g_Muted = muted;
    if (g_EngineReady) ma_engine_set_volume(&g_Engine, muted ? 0.0f : g_MasterVolume);
}

bool Audio::IsMuted() {
    return g_Muted;
}

const std::string& Audio::LastError() {
    return g_LastError;
}

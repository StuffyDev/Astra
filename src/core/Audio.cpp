#include "core/Audio.h"
#include "utils/AssetIO.h"
#include <miniaudio.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <iostream>

namespace {

struct Voice {
    ma_sound sound;
    std::vector<unsigned char> bytes;   // расшифрованные данные (шрифт/звук в билде)
    std::unique_ptr<ma_decoder> decoder;
    float userVolume = 1.0f;
    int group = 0;                      // 0=SFX, 1=Music
    bool alive = false;
    bool loop = false;
};

ma_engine g_Engine;
bool g_EngineReady = false;
bool g_Muted = false;
float g_MasterVolume = 1.0f;
float g_GroupVolume[2] = { 1.0f, 1.0f };
uint32_t g_NextId = 1;
std::unordered_map<uint32_t, Voice> g_Voices;
std::string g_LastError;

void StopVoice(Voice& v) {
    if (!v.alive) return;
    ma_sound_stop(&v.sound);
    ma_sound_uninit(&v.sound);   // раньше decoder: он ссылается на bytes
    v.decoder.reset();
    v.bytes.clear();
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

uint32_t Audio::Play(const std::string& path, float volume, float pitch, bool loop, int group) {
    if (!Init()) return 0;
    Voice v;
    if (!AssetIO::ReadBytes(path, v.bytes) || v.bytes.empty()) {
        g_LastError = "Failed to load sound: " + path;
        std::cerr << "[Audio] " << g_LastError << "\n";
        return 0;
    }
    v.decoder = std::make_unique<ma_decoder>();
    if (ma_decoder_init_memory(v.bytes.data(), static_cast<size_t>(v.bytes.size()),
                               nullptr, v.decoder.get()) != MA_SUCCESS ||
        ma_sound_init_from_data_source(&g_Engine, v.decoder.get(), 0, nullptr, &v.sound) != MA_SUCCESS) {
        g_LastError = "Failed to load sound: " + path;
        std::cerr << "[Audio] " << g_LastError << "\n";
        return 0;
    }
    v.alive = true;
    v.loop = loop;
    v.userVolume = volume;
    v.group = (group == 1) ? 1 : 0;
    uint32_t id = g_NextId++;
    ma_sound_set_volume(&v.sound, volume * g_GroupVolume[v.group]);
    ma_sound_set_pitch(&v.sound, pitch);
    ma_sound_start(&v.sound);
    g_Voices.emplace(id, std::move(v));
    return id;
}

uint32_t Audio::PlayOneShot(const std::string& path, float volume, float pitch, int group) {
    return Play(path, volume, pitch, false, group);
}

uint32_t Audio::PlayLooped(const std::string& path, float volume, float pitch, int group) {
    return Play(path, volume, pitch, true, group);
}

void Audio::Stop(uint32_t id) {
    auto it = g_Voices.find(id);
    if (it == g_Voices.end()) return;
    StopVoice(it->second);
    g_Voices.erase(it);
}

void Audio::SetVolume(uint32_t id, float volume) {
    auto it = g_Voices.find(id);
    if (it != g_Voices.end()) {
        it->second.userVolume = volume;
        ma_sound_set_volume(&it->second.sound, volume * g_GroupVolume[it->second.group]);
    }
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

void Audio::SetGroupVolume(int group, float volume) {
    if (group < 0 || group > 1) return;
    g_GroupVolume[group] = volume;
    for (auto& [id, v] : g_Voices)
        if (v.alive && v.group == group) ma_sound_set_volume(&v.sound, v.userVolume * volume);
}

float Audio::GroupVolume(int group) {
    return (group >= 0 && group <= 1) ? g_GroupVolume[group] : 1.0f;
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

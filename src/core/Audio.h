// Аудиосистема движка (miniaudio). Все клипы — относительные пути ассетов проекта.
// Доступна и из редактора (превью), и из скриптов.
#pragma once
#include <cstdint>
#include <string>

class Audio {
public:
    // Инициализация ленивая: первый Play создаёт устройство.
    // Возвращает false, если аудиовыхода нет (движок продолжит работу без звука).
    static bool Init();
    static void Shutdown();
    // Раз в кадр: чистим завершённые one-shot звуки
    static void NewFrame();

    // Один сигнал — id источника (0 = ошибка). Loop-версия играет до Stop.
    static uint32_t PlayOneShot(const std::string& path, float volume = 1.0f, float pitch = 1.0f);
    static uint32_t PlayLooped(const std::string& path, float volume = 1.0f, float pitch = 1.0f);
    static void Stop(uint32_t id);
    static void SetVolume(uint32_t id, float volume);
    static void SetPitch(uint32_t id, float pitch);
    static void StopAll();

    // Микшер (как Unity Audio Mixer, упрощённо: мастер-шина)
    static void SetMasterVolume(float volume); // 0..1
    static float MasterVolume();
    static void SetMuted(bool muted);
    static bool IsMuted();

    static const std::string& LastError();

private:
    static uint32_t Play(const std::string& path, float volume, float pitch, bool loop);
};

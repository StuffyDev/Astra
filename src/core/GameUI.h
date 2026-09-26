#pragma once
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

// Реестр событий runtime-UI (кнопки/слайдеры из Game-view).
// Оверлей вызывает Report* во время Play, игровой код (будущие C++-скрипты) читает WasClicked/GetValue.
class GameUI {
public:
    static void BeginFrame();
    static void ReportClick(uint32_t entityId);
    static void ReportValue(uint32_t entityId, float value);
    static bool WasClicked(uint32_t entityId);
    static float GetValue(uint32_t entityId, float fallback = 0.0f);

private:
    static std::unordered_set<uint32_t>& Clicked();
    static std::unordered_map<uint32_t, float>& Values();
};

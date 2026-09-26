#include "core/GameUI.h"

std::unordered_set<uint32_t>& GameUI::Clicked() {
    static std::unordered_set<uint32_t> s;
    return s;
}

std::unordered_map<uint32_t, float>& GameUI::Values() {
    static std::unordered_map<uint32_t, float> s;
    return s;
}

void GameUI::BeginFrame() {
    Clicked().clear();
}

void GameUI::ReportClick(uint32_t entityId) {
    Clicked().insert(entityId);
}

void GameUI::ReportValue(uint32_t entityId, float value) {
    Values()[entityId] = value;
}

bool GameUI::WasClicked(uint32_t entityId) {
    return Clicked().count(entityId) > 0;
}

float GameUI::GetValue(uint32_t entityId, float fallback) {
    auto it = Values().find(entityId);
    return it != Values().end() ? it->second : fallback;
}

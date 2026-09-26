#pragma once
#include <filesystem>
#include <string>
#include <vector>

class ProjectManager {
public:
    ProjectManager();

    // Проверяет структуру проекта, делает cwd == dir, сохраняет recent
    bool OpenProject(const std::filesystem::path& dir);
    // Копирует пресет TEMPLATE_DIR в parentDir/name и открывает его
    bool CreateProject(const std::filesystem::path& parentDir, const std::string& name);

    bool HasLastProject() const { return !m_Last.empty(); }
    const std::string& GetLastProject() const { return m_Last; }
    const std::vector<std::string>& GetRecent() const { return m_Recent; }
    std::string CurrentProjectName() const;
    std::string LastError() const { return m_LastError; }

private:
    void LoadConfig();
    void SaveConfig() const;
    void AddRecent(const std::filesystem::path& dir);

    std::vector<std::string> m_Recent;
    std::string m_Last;
    mutable std::string m_LastError;
};

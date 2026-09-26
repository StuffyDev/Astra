#include "core/ProjectManager.h"
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <system_error>

#ifndef TEMPLATE_DIR
#define TEMPLATE_DIR "templates/default_project"
#endif

namespace fs = std::filesystem;

static fs::path HomeDir() {
    if (const char* home = std::getenv("HOME")) return home;
    return fs::current_path();
}

static fs::path ConfigDir() {
    return HomeDir() / ".micro-world";
}

static fs::path ConfigFile() {
    return ConfigDir() / "projects.conf";
}

ProjectManager::ProjectManager() {
    LoadConfig();
}

void ProjectManager::LoadConfig() {
    m_Recent.clear();
    m_Last.clear();

    std::ifstream file(ConfigFile());
    if (!file.is_open()) return;

    std::string line;
    bool inRecent = false;
    while (std::getline(file, line)) {
        if (line.rfind("Last:", 0) == 0) {
            m_Last = line.substr(5);
            while (!m_Last.empty() && m_Last[0] == ' ') m_Last.erase(0, 1);
            inRecent = false;
        } else if (line == "Recent:") {
            inRecent = true;
        } else if (!line.empty()) {
            if (inRecent) m_Recent.push_back(line);
        }
    }
}

void ProjectManager::SaveConfig() const {
    std::error_code ec;
    fs::create_directories(ConfigDir(), ec);

    std::ofstream file(ConfigFile());
    if (!file.is_open()) return;
    file << "Last: " << m_Last << "\n";
    file << "Recent:\n";
    for (const auto& p : m_Recent) file << p << "\n";
}

void ProjectManager::AddRecent(const fs::path& dir) {
    std::string s = dir.string();
    m_Recent.erase(std::remove(m_Recent.begin(), m_Recent.end(), s), m_Recent.end());
    m_Recent.insert(m_Recent.begin(), s);
    if (m_Recent.size() > 10) m_Recent.pop_back();
}

bool ProjectManager::OpenProject(const fs::path& dir) {
    m_LastError.clear();

    std::error_code ec;
    fs::path abs = fs::absolute(dir, ec);
    if (ec || !fs::is_directory(abs)) {
        m_LastError = "Directory not found: " + dir.string();
        return false;
    }
    if (!fs::exists(abs / "assets") && !fs::exists(abs / "project.json")) {
        m_LastError = "Not an Astra project: " + abs.string();
        return false;
    }

    fs::current_path(abs, ec);
    if (ec) {
        m_LastError = "Failed to enter project directory: " + abs.string();
        return false;
    }

    m_Last = abs.string();
    AddRecent(abs);
    SaveConfig();
    return true;
}

bool ProjectManager::CreateProject(const fs::path& parentDir, const std::string& name) {
    m_LastError.clear();

    if (name.empty()) {
        m_LastError = "Project name is empty";
        return false;
    }

    std::error_code ec;
    fs::path parent = fs::absolute(parentDir, ec);
    if (ec || !fs::is_directory(parent)) {
        m_LastError = "Location does not exist: " + parentDir.string();
        return false;
    }

    fs::path target = parent / name;
    if (fs::exists(target)) {
        m_LastError = "Already exists: " + target.string();
        return false;
    }

    fs::path templateDir = TEMPLATE_DIR;
    if (!fs::exists(templateDir / "assets")) {
        m_LastError = "Template not found at: " + templateDir.string();
        return false;
    }

    fs::create_directories(target, ec);
    if (ec) {
        m_LastError = "Cannot create: " + target.string();
        return false;
    }

    fs::copy(templateDir / "assets", target / "assets",
             fs::copy_options::recursive, ec);
    if (ec) {
        m_LastError = "Failed to copy template: " + ec.message();
        return false;
    }

    {
        std::ofstream json(target / "project.json");
        json << "{\n  \"name\": \"" << name << "\",\n  \"engine\": \"Micro-World 0.1\"\n}\n";
    }

    return OpenProject(target);
}

std::string ProjectManager::CurrentProjectName() const {
    std::error_code ec;
    fs::path json = fs::current_path(ec) / "project.json";
    std::ifstream file(json);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        size_t pos = content.find("\"name\"");
        if (pos != std::string::npos) {
            pos = content.find(':', pos);
            size_t q1 = content.find('"', pos + 1);
            size_t q2 = content.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos)
                return content.substr(q1 + 1, q2 - q1 - 1);
        }
    }
    return fs::current_path(ec).filename().string();
}

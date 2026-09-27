#include "core/Application.h"
#include "core/GUI.h"
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    AppOptions opts;
    std::string buildScene, buildOut;
    int buildMode = 0; // 0 = лаунчер+либка, 1 = один exe, 2 = папка
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--play" || a == "-p") opts.player = true;
        else if (a == "--project" && i + 1 < argc) opts.projectDir = argv[++i];
        else if (a == "--scene" && i + 1 < argc) opts.scenePath = argv[++i];
        else if (a == "--build" && i + 1 < argc) buildScene = argv[++i];
        else if (a == "--out" && i + 1 < argc) buildOut = argv[++i];
        else if (a == "--single") buildMode = 1;
        else if (a == "--folder") buildMode = 2;
    }

    // Головной билд без GUI: ./Astra --build assets/scenes/x.scene --out ./game [--single|--folder]
    if (!buildScene.empty()) {
        if (!opts.projectDir.empty()) {
            std::error_code ec;
            std::filesystem::current_path(opts.projectDir, ec);
        }
        if (buildOut.empty()) buildOut = "game_build";
        std::string status;
        bool ok = AstraBuildGame("/proc/self/exe", buildScene, buildOut, buildMode, true, status);
        std::cout << status << "\n";
        return ok ? 0 : 1;
    }

    Application app(opts);
    app.Run();
    return 0;
}

#include "core/Application.h"
#include <string>

int main(int argc, char** argv) {
    AppOptions opts;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--play" || a == "-p") opts.player = true;
        else if (a == "--project" && i + 1 < argc) opts.projectDir = argv[++i];
        else if (a == "--scene" && i + 1 < argc) opts.scenePath = argv[++i];
    }
    Application app(opts);
    app.Run();
    return 0;
}

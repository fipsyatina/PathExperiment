#include "Application.hpp"
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    try {
        pathx::Application application(pathx::findResources(std::filesystem::absolute(argv[0])));
        if (argc == 3 && std::string(argv[1]) == "--smoke-test") {
            return application.smokeTest(argv[2]);
        }
        if (argc == 3 && std::string(argv[1]) == "--record-demo") {
            return application.recordDemo(argv[2]);
        }
        if (argc != 1) {
            std::cerr << "Usage: PathExperiment [--smoke-test DIR | --record-demo DIR]\n";
            return 2;
        }
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "PathExperiment: " << error.what() << '\n';
        // A GUI build may have no console. Keep a readable diagnostic beside the executable.
        const auto logPath = std::filesystem::absolute(argv[0]).parent_path() / "PathExperiment-error.log";
        std::ofstream log(logPath, std::ios::app);
        log << error.what() << '\n';
        return 1;
    }
}

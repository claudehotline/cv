#include "AnalysisAPI.h"
#include "SignalingServer.h"
#include "app/application.hpp"

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
}

} // namespace

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::string config_dir = "config";
    if (argc > 1 && argv[1]) {
        config_dir = argv[1];
    }

    va::app::Application app;
    if (!app.initialize(config_dir)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    if (!app.start()) {
        std::cerr << "Failed to start application services" << std::endl;
        return 1;
    }

    AnalysisAPI api(&app);
    if (!api.start(8082)) {
        std::cerr << "Failed to start HTTP API" << std::endl;
        return 1;
    }

    SignalingServer signaling;
    if (!signaling.start(8083)) {
        std::cerr << "Failed to start signaling server" << std::endl;
        return 1;
    }

    std::cout << "Video analyzer running." << std::endl;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    signaling.stop();
    api.stop();
    app.shutdown();

    return 0;
}

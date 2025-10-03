#include "app/application.hpp"
#include "core/logger.hpp"

#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int /*signum*/) {
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
        VA_LOG_ERROR() << "Failed to initialize application";
        return 1;
    }

    if (!app.start()) {
        VA_LOG_ERROR() << "Failed to start application services";
        return 1;
    }

    VA_LOG_INFO() << "Video analyzer running.";

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    VA_LOG_INFO() << "Shutdown signal received, stopping services.";
    app.shutdown();

    return 0;
}

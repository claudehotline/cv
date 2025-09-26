#include "VideoSourceManager.h"
#include "SourceAPI.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

static VideoSourceManager* g_manager = nullptr;
static SourceAPI* g_source_api = nullptr;

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    if (g_source_api) {
        g_source_api->stop();
    }
    if (g_manager) {
        g_manager->stopAll();
    }
    exit(signum);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);

    std::string config_file = "config/video_sources.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    VideoSourceManager manager;
    g_manager = &manager;

    SourceAPI source_api(&manager);
    g_source_api = &source_api;

    // Set frame callback function
    manager.setFrameCallback([](const cv::Mat& frame, const std::string& source_id) {
        std::cout << "Received frame data from " << source_id << ", size: "
                  << frame.cols << "x" << frame.rows << std::endl;

        // Frame data can be sent to video analysis module here
        // Note: Video analysis module will receive data via RTSP stream
    });

    // Load configuration and start
    if (!manager.loadConfig(config_file)) {
        std::cerr << "Failed to load configuration file" << std::endl;
        return -1;
    }

    if (!manager.startAll()) {
        std::cerr << "Failed to start video sources" << std::endl;
        return -1;
    }

    // Start source management API server
    if (!source_api.start(8081)) {
        std::cerr << "Failed to start source management API server" << std::endl;
        return -1;
    }

    std::cout << "Video source manager and API server started, press Ctrl+C to exit" << std::endl;

    // Keep program running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto active_sources = manager.getActiveSources();
        if (active_sources.empty()) {
            std::cout << "No active video sources" << std::endl;
            break;
        }
    }

    return 0;
}
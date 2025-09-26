#include "VideoSourceManager.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>

static VideoSourceManager* g_manager = nullptr;

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
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

    std::cout << "Video Source Manager - Simple Version" << std::endl;
    std::cout << "OpenCV Version: " << CV_VERSION << std::endl;

    VideoSourceManager manager;
    g_manager = &manager;

    // Set frame callback
    manager.setFrameCallback([](const cv::Mat& frame, const std::string& source_id) {
        std::cout << "Received frame from " << source_id
                  << " - Size: " << frame.cols << "x" << frame.rows << std::endl;
    });

    // Test with default camera
    std::cout << "Testing default camera..." << std::endl;

    // Create a simple test configuration
    Json::Value config;
    Json::Value sources(Json::arrayValue);

    Json::Value camera_source;
    camera_source["id"] = "default_camera";
    camera_source["source_path"] = "0";  // Default camera
    camera_source["type"] = "camera";
    camera_source["enabled"] = true;
    camera_source["fps"] = 30;

    sources.append(camera_source);
    config["video_sources"] = sources;

    // Try to load config file first, if it fails, use test config
    if (!manager.loadConfig(config_file)) {
        std::cout << "Config file not found, using test configuration..." << std::endl;
        manager.loadFromJson(config);
    }

    // Start all sources
    manager.startAll();

    std::cout << "Video Source Manager is running..." << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;

    // Keep running
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Print stats every 5 seconds
        static int counter = 0;
        if (++counter % 5 == 0) {
            std::cout << "Still running... (counter: " << counter << ")" << std::endl;
        }
    }

    return 0;
}
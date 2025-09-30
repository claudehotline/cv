#include "VideoAnalyzer.h"
#include "SignalingServer.h"
#include "AnalysisAPI.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <ctime>
#include <iostream>
#include <thread>

static std::atomic<bool> g_running(true);
static VideoAnalyzer* g_analyzer = nullptr;
static SignalingServer* g_signaling_server = nullptr;
static AnalysisAPI* g_analysis_api = nullptr;

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
    if (g_signaling_server) {
        g_signaling_server->stop();
    }
    if (g_analysis_api) {
        g_analysis_api->stop();
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    std::string config_file = "config/analyzer_config.json";
    if (argc > 1 && argv[1]) {
        config_file = argv[1];
    }

    VideoAnalyzer analyzer;
    g_analyzer = &analyzer;

    SignalingServer signaling_server;
    g_signaling_server = &signaling_server;

    AnalysisAPI analysis_api(&analyzer);
    g_analysis_api = &analysis_api;

    analyzer.setResultCallback([&signaling_server](const AnalysisResult& result) {
        if (!result.success) {
            std::cerr << "[analysis] failed: " << result.error_message << std::endl;
            return;
        }

        std::cout << "Analysis complete - source: " << result.source_id
                  << ", request: " << result.request_id
                  << ", detections: " << result.detections.size() << std::endl;

        Json::Value message;
        message["type"] = "analysis_result";
        message["source_id"] = result.source_id;
        message["request_id"] = result.request_id;
        message["analysis_type"] = (result.type == AnalysisType::OBJECT_DETECTION)
                                        ? "object_detection"
                                        : "instance_segmentation";
        message["timestamp"] = static_cast<int64_t>(std::time(nullptr));

        Json::Value detections(Json::arrayValue);
        for (const auto& detection : result.detections) {
            Json::Value det;
            det["class_name"] = detection.class_name;
            det["confidence"] = detection.confidence;
            det["bbox"]["x"] = detection.bbox.x;
            det["bbox"]["y"] = detection.bbox.y;
            det["bbox"]["width"] = detection.bbox.width;
            det["bbox"]["height"] = detection.bbox.height;
            detections.append(det);
        }
        message["detections"] = detections;

        signaling_server.broadcastMessage(message);
    });

    signaling_server.setVideoAnalyzerCallback([&analyzer](const std::string& client_id, const Json::Value& message) {
        const std::string msg_type = message.get("type", "").asString();
        std::cout << "Received signaling message from " << client_id << ": " << msg_type << std::endl;

        if (msg_type == "request_offer") {
            // Check if source_id is specified in the request
            if (message.isMember("data") && message["data"].isMember("source_id")) {
                std::string requested_source = message["data"]["source_id"].asString();
                std::cout << "Client " << client_id << " requested source: " << requested_source << std::endl;
                analyzer.setWebRTCClientSource(client_id, requested_source);
            }

            std::string sdp_offer;
            if (!analyzer.createWebRTCOffer(client_id, sdp_offer)) {
                std::cerr << "Failed to create WebRTC offer" << std::endl;
            }
        } else if (msg_type == "answer") {
            if (message.isMember("data") && message["data"].isMember("sdp")) {
                const std::string sdp = message["data"]["sdp"].asString();
                if (!analyzer.handleWebRTCAnswer(client_id, sdp)) {
                    std::cerr << "Failed to handle WebRTC answer" << std::endl;
                }
            }
        } else if (msg_type == "ice_candidate") {
            if (message.isMember("data")) {
                if (!analyzer.addWebRTCIceCandidate(client_id, message["data"])) {
                    std::cerr << "Failed to add ICE candidate" << std::endl;
                }
            }
        }
    });

    if (!analyzer.initialize(config_file)) {
        std::cerr << "Failed to initialize video analyzer" << std::endl;
        return -1;
    }
    if (!signaling_server.start(8083)) {
        std::cerr << "Failed to start signaling server" << std::endl;
        return -1;
    }
    if (!analysis_api.start(8082)) {
        std::cerr << "Failed to start analysis API server" << std::endl;
        return -1;
    }
    if (!analyzer.startWebRTCStreaming(8080)) {
        std::cerr << "Failed to start WebRTC streaming" << std::endl;
        return -1;
    }

    analyzer.setWebRTCSignalingCallback([&signaling_server](const std::string& client_id, const Json::Value& message) {
        std::thread([&signaling_server, client_id, message]() {
            signaling_server.sendToClient(client_id, message);
        }).detach();
    });

    analyzer.start();
    std::cout << "Video analyzer ready (HTTP:8082, signaling:8083, WebRTC:8080)." << std::endl;

    bool source1_added = analyzer.addRTSPSource("camera_01", "rtsp://localhost:8554/camera_01", AnalysisType::OBJECT_DETECTION);
    bool source2_added = analyzer.addRTSPSource("camera_02", "rtsp://localhost:8554/camera_02", AnalysisType::OBJECT_DETECTION);

    if (source1_added) std::cout << "RTSP source camera_01 added." << std::endl;
    if (source2_added) std::cout << "RTSP source camera_02 added." << std::endl;

    if (source1_added || source2_added) {
        analyzer.startRTSPProcessing();
    } else {
        std::cout << "Warning: failed to add RTSP source. Falling back to synthetic frames." << std::endl;

        std::thread generator([&analyzer]() {
            cv::Mat frame(480, 640, CV_8UC3);
            int frame_count = 0;

            while (analyzer.isRunning()) {
                for (int y = 0; y < frame.rows; ++y) {
                    for (int x = 0; x < frame.cols; ++x) {
                        frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                            static_cast<unsigned char>((frame_count + x) % 255),
                            static_cast<unsigned char>((frame_count + y) % 255),
                            static_cast<unsigned char>((frame_count * 2) % 255));
                    }
                }

                const int circle_x = (frame_count * 5) % frame.cols;
                const int circle_y = static_cast<int>(frame.rows / 2 + 50.0 * std::sin(frame_count * 0.1));
                cv::circle(frame, cv::Point(circle_x, circle_y), 30, cv::Scalar(255, 255, 255), -1);

                const std::string text = "Test Frame #" + std::to_string(frame_count);
                cv::putText(frame, text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);

                const auto now = std::chrono::system_clock::now();
                const std::time_t ts = std::chrono::system_clock::to_time_t(now);
                std::string timestamp = std::ctime(&ts);
                if (!timestamp.empty() && timestamp.back() == '\n') {
                    timestamp.pop_back();
                }
                cv::putText(frame, timestamp, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);

                analyzer.pushFrameToWebRTC(frame, "camera_01");
                analyzer.processFrame(frame, "camera_01", AnalysisType::OBJECT_DETECTION, frame_count);

                ++frame_count;
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
            }
        });
        generator.detach();
        std::cout << "Synthetic frame generator started." << std::endl;
    }

    while (g_running && analyzer.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    analyzer.stop();
    std::cout << "Video analyzer stopped." << std::endl;
    return 0;
}

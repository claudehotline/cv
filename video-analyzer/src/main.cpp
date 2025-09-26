#include "VideoAnalyzer.h"
#include "SignalingServer.h"
#include "AnalysisAPI.h"
#include <iostream>
#include <signal.h>
#include <atomic>
#include <thread>

static std::atomic<bool> g_running(true);
static VideoAnalyzer* g_analyzer = nullptr;
static SignalingServer* g_signaling_server = nullptr;
static AnalysisAPI* g_analysis_api = nullptr;

void signalHandler(int signum) {
    std::cout << "\n收到信号 " << signum << "，正在关闭..." << std::endl;
    g_running = false;
    if (g_signaling_server) {
        g_signaling_server->stop();
    }
    if (g_analysis_api) {
        g_analysis_api->stop();
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);

    std::string config_file = "config/analyzer_config.json";
    if (argc > 1) {
        config_file = argv[1];
    }

    VideoAnalyzer analyzer;
    g_analyzer = &analyzer;

    SignalingServer signaling_server;
    g_signaling_server = &signaling_server;

    AnalysisAPI analysis_api(&analyzer);
    g_analysis_api = &analysis_api;

    // 设置结果回调
    analyzer.setResultCallback([&signaling_server](const AnalysisResult& result) {
        if (result.success) {
            std::cout << "分析完成 - 源ID: " << result.source_id
                     << ", 请求ID: " << result.request_id
                     << ", 检测到 " << result.detections.size() << " 个对象" << std::endl;

            // 将分析结果发送给Web前端
            Json::Value result_msg;
            result_msg["type"] = "analysis_result";
            result_msg["source_id"] = result.source_id;
            result_msg["request_id"] = result.request_id;
            result_msg["analysis_type"] = (result.type == AnalysisType::OBJECT_DETECTION) ? "object_detection" : "instance_segmentation";
            result_msg["timestamp"] = static_cast<int64_t>(time(nullptr));

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
            result_msg["detections"] = detections;

            signaling_server.broadcastMessage(result_msg);
        } else {
            std::cerr << "分析失败: " << result.error_message << std::endl;
        }
    });

    // WebRTC回调将在启动WebRTC流媒体服务器后设置

    // 设置信令服务器的回调 - 简化消息处理
    signaling_server.setVideoAnalyzerCallback([&analyzer, &signaling_server](const std::string& client_id, const Json::Value& message) {
        std::string msg_type = message.get("type", "").asString();
        std::cout << "收到来自客户端的消息: " << msg_type << " (" << client_id << ")" << std::endl;

        if (msg_type == "request_offer") {
            // 客户端请求视频流
            std::string sdp_offer;
            if (analyzer.createWebRTCOffer(client_id, sdp_offer)) {
                std::cout << "成功创建WebRTC offer" << std::endl;
                // Note: offer发送将通过VideoAnalyzer的WebRTC信令回调自动处理 (main.cpp:76-79)
            } else {
                std::cout << "创建WebRTC offer失败" << std::endl;
            }
        } else if (msg_type == "answer") {
            // 处理客户端的answer
            if (message.isMember("data") && message["data"].isMember("sdp")) {
                std::string sdp_answer = message["data"]["sdp"].asString();
                if (analyzer.handleWebRTCAnswer(client_id, sdp_answer)) {
                    std::cout << "WebRTC answer处理成功" << std::endl;
                } else {
                    std::cout << "WebRTC answer处理失败" << std::endl;
                }
            }
        } else if (msg_type == "ice_candidate") {
            // 处理ICE候选
            if (message.isMember("data")) {
                if (analyzer.addWebRTCIceCandidate(client_id, message["data"])) {
                    std::cout << "ICE候选添加成功" << std::endl;
                } else {
                    std::cout << "ICE候选添加失败" << std::endl;
                }
            }
        } else if (msg_type == "start_analysis" || msg_type == "stop_analysis") {
            std::cout << "分析控制命令: " << msg_type << " 来自客户端: " << client_id << std::endl;
        } else {
            std::cout << "未处理的消息类型: " << msg_type << std::endl;
        }
    });

    // 初始化分析器
    if (!analyzer.initialize(config_file)) {
        std::cerr << "视频分析器初始化失败" << std::endl;
        return -1;
    }

    // 启动信令服务器
    if (!signaling_server.start(8083)) {
        std::cerr << "信令服务器启动失败" << std::endl;
        return -1;
    }

    // 启动分析API服务器
    if (!analysis_api.start(8082)) {
        std::cerr << "分析API服务器启动失败" << std::endl;
        return -1;
    }

    // 启动WebRTC流媒体服务器
    if (!analyzer.startWebRTCStreaming(8080)) {
        std::cerr << "WebRTC流媒体服务器启动失败" << std::endl;
        return -1;
    }

    // 设置VideoAnalyzer的WebRTC回调 - 异步发送避免死锁
    analyzer.setWebRTCSignalingCallback([&signaling_server](const std::string& client_id, const Json::Value& message) {
        std::cout << "发送WebRTC消息到客户端: " << client_id << ", 类型: " << message.get("type", "").asString() << std::endl;

        // 使用异步执行避免死锁
        std::thread([&signaling_server, client_id, message]() {
            signaling_server.sendToClient(client_id, message);
        }).detach();
    });

    // 启动分析器
    analyzer.start();
    std::cout << "视频分析模块、WebRTC信令服务器(8083)、分析API服务器(8082)和WebRTC流媒体(8080)已启动，等待连接..." << std::endl;

    // 添加RTSP源并启动处理
    // 这里可以从配置文件中读取，暂时硬编码示例
    // 假设视频源管理模块在 rtsp://localhost:8554/camera_01 推流
    if (analyzer.addRTSPSource("camera_01", "rtsp://localhost:8554/camera_01", AnalysisType::OBJECT_DETECTION)) {
        std::cout << "已添加RTSP源: camera_01" << std::endl;
        analyzer.startRTSPProcessing();
    } else {
        std::cout << "添加RTSP源失败，启动测试视频生成器..." << std::endl;

        // 启动测试视频生成器
        std::thread test_generator([&analyzer]() {
            cv::Mat frame(480, 640, CV_8UC3);
            int frame_count = 0;

            while (analyzer.isRunning()) {
                // 创建渐变背景
                for (int y = 0; y < frame.rows; y++) {
                    for (int x = 0; x < frame.cols; x++) {
                        frame.at<cv::Vec3b>(y, x) = cv::Vec3b(
                            (frame_count + x) % 255,      // B
                            (frame_count + y) % 255,      // G
                            (frame_count * 2) % 255        // R
                        );
                    }
                }

                // 添加移动的圆形
                int circle_x = (frame_count * 5) % frame.cols;
                int circle_y = frame.rows / 2 + 50 * sin(frame_count * 0.1);
                cv::circle(frame, cv::Point(circle_x, circle_y), 30, cv::Scalar(255, 255, 255), -1);

                // 添加文字
                std::string text = "Test Frame #" + std::to_string(frame_count);
                cv::putText(frame, text, cv::Point(10, 30),
                           cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);

                // 推送帧到WebRTC
                analyzer.pushFrameToWebRTC(frame, "camera_01");

                frame_count++;

                // 控制帧率 (30 FPS)
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
            }
        });
        test_generator.detach();
        std::cout << "测试视频生成器已启动" << std::endl;
    }

    // 保持程序运行
    while (g_running && analyzer.isRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    analyzer.stop();
    std::cout << "视频分析模块已停止" << std::endl;

    return 0;
}
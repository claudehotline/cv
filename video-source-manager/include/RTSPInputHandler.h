#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>

class RTSPInputHandler {
public:
    struct RTSPConfig {
        std::string rtsp_url;
        int timeout_ms = 5000;          // 连接超时
        int max_reconnect_attempts = 3; // 最大重连次数
        int reconnect_delay_ms = 2000;  // 重连延迟
        bool enable_buffering = false;  // 是否启用缓冲
        int buffer_size = 1;            // 缓冲区大小
    };

    struct RTSPStats {
        bool is_connected = false;
        int64_t frames_received = 0;
        int64_t frames_dropped = 0;
        double current_fps = 0.0;
        int64_t bytes_received = 0;
        std::chrono::time_point<std::chrono::steady_clock> last_frame_time;
        int reconnect_count = 0;
    };

    RTSPInputHandler(const RTSPConfig& config);
    ~RTSPInputHandler();

    bool initialize();
    bool start();
    void stop();

    void setFrameCallback(std::function<void(const cv::Mat&)> callback);
    void setStatusCallback(std::function<void(bool connected, const std::string& error)> callback);

    RTSPStats getStats() const;
    bool isConnected() const;
    bool isRunning() const;

private:
    RTSPConfig config_;
    cv::VideoCapture capture_;

    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::thread capture_thread_;

    std::function<void(const cv::Mat&)> frame_callback_;
    std::function<void(bool, const std::string&)> status_callback_;

    mutable std::mutex stats_mutex_;
    RTSPStats stats_;

    void captureLoop();
    bool connect();
    void disconnect();
    bool reconnect();
    void updateStats(bool frame_received = false);
    void notifyStatus(bool connected, const std::string& error = "");
};
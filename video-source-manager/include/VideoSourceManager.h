#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <functional>
#include "RTSPStreamer.h"

struct VideoSourceConfig {
    std::string id;
    std::string source_path;
    std::string type; // "camera", "file", "stream"
    bool enabled;
    int fps;

    // RTSP推流配置
    bool enable_rtsp;
    std::string rtsp_url;
    int rtsp_port;
};

class VideoSource {
public:
    VideoSource(const VideoSourceConfig& config);
    ~VideoSource();

    bool initialize();
    bool start();
    void stop();
    bool isRunning() const;

    void setFrameCallback(std::function<void(const cv::Mat&, const std::string&)> callback);

    // RTSP推流功能
    bool startRTSPStream();
    void stopRTSPStream();
    bool isRTSPStreaming() const;

    const VideoSourceConfig& getConfig() const { return config_; }

private:
    void captureLoop();

    VideoSourceConfig config_;
    cv::VideoCapture cap_;
    std::thread capture_thread_;
    std::atomic<bool> running_;
    std::function<void(const cv::Mat&, const std::string&)> frame_callback_;
    std::mutex callback_mutex_;

    // RTSP推流相关
    std::unique_ptr<RTSPStreamer> rtsp_streamer_;
    std::atomic<bool> rtsp_streaming_;
};

class VideoSourceManager {
public:
    VideoSourceManager();
    ~VideoSourceManager();

    bool loadConfig(const std::string& config_file);
    bool addVideoSource(const VideoSourceConfig& config);
    bool removeVideoSource(const std::string& source_id);

    bool startAll();
    void stopAll();

    void setFrameCallback(std::function<void(const cv::Mat&, const std::string&)> callback);

    std::vector<VideoSourceConfig> getActiveSources() const;

private:
    std::vector<std::unique_ptr<VideoSource>> video_sources_;
    std::function<void(const cv::Mat&, const std::string&)> frame_callback_;
    mutable std::mutex sources_mutex_;

    void onFrameReceived(const cv::Mat& frame, const std::string& source_id);
};
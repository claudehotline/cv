#include "VideoSourceManager.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

// VideoSource 实现
VideoSource::VideoSource(const VideoSourceConfig& config)
    : config_(config), running_(false), rtsp_streaming_(false) {
    if (config_.enable_rtsp) {
        rtsp_streamer_ = std::make_unique<RTSPStreamer>();
    }
}

VideoSource::~VideoSource() {
    stop();
    stopRTSPStream();
}

bool VideoSource::initialize() {
    if (config_.type == "camera") {
        cap_.open(std::stoi(config_.source_path));
    } else {
        cap_.open(config_.source_path);
    }

    if (!cap_.isOpened()) {
        std::cerr << "无法打开视频源: " << config_.source_path << std::endl;
        return false;
    }

    if (config_.fps > 0) {
        cap_.set(cv::CAP_PROP_FPS, config_.fps);
    }

    return true;
}

bool VideoSource::start() {
    if (running_) {
        return true;
    }

    if (!cap_.isOpened() && !initialize()) {
        return false;
    }

    running_ = true;
    capture_thread_ = std::thread(&VideoSource::captureLoop, this);

    return true;
}

void VideoSource::stop() {
    if (running_) {
        running_ = false;
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
    }

    if (cap_.isOpened()) {
        cap_.release();
    }
}

bool VideoSource::isRunning() const {
    return running_;
}

void VideoSource::setFrameCallback(std::function<void(const cv::Mat&, const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    frame_callback_ = callback;
}

void VideoSource::captureLoop() {
    cv::Mat frame;

    while (running_) {
        if (!cap_.read(frame)) {
            std::cerr << "无法读取帧数据: " << config_.id << std::endl;
            break;
        }

        if (!frame.empty()) {
            // 推送到RTSP流
            if (rtsp_streaming_ && rtsp_streamer_) {
                rtsp_streamer_->pushFrame(frame);
            }

            // 调用帧回调
            std::lock_guard<std::mutex> lock(callback_mutex_);
            if (frame_callback_) {
                frame_callback_(frame, config_.id);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }

    running_ = false;
}

// VideoSourceManager 实现
VideoSourceManager::VideoSourceManager() {
}

VideoSourceManager::~VideoSourceManager() {
    stopAll();
}

bool VideoSourceManager::loadConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << config_file << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;

    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        std::cerr << "JSON解析错误: " << errs << std::endl;
        return false;
    }

    const Json::Value& sources = root["video_sources"];
    for (const auto& source : sources) {
        VideoSourceConfig config;
        config.id = source["id"].asString();
        config.source_path = source["source_path"].asString();
        config.type = source["type"].asString();
        config.enabled = source["enabled"].asBool();
        config.fps = source.get("fps", 30).asInt();

        // 读取RTSP配置
        config.enable_rtsp = source.get("enable_rtsp", false).asBool();
        config.rtsp_url = source.get("rtsp_url", "").asString();
        config.rtsp_port = source.get("rtsp_port", 8554).asInt();

        if (config.enabled) {
            addVideoSource(config);
        }
    }

    return true;
}

bool VideoSourceManager::addVideoSource(const VideoSourceConfig& config) {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    auto video_source = std::make_unique<VideoSource>(config);
    if (!video_source->initialize()) {
        return false;
    }

    video_source->setFrameCallback(
        [this](const cv::Mat& frame, const std::string& source_id) {
            onFrameReceived(frame, source_id);
        }
    );

    video_sources_.push_back(std::move(video_source));
    return true;
}

bool VideoSourceManager::removeVideoSource(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    auto it = std::remove_if(video_sources_.begin(), video_sources_.end(),
        [&source_id](const std::unique_ptr<VideoSource>& source) {
            return source->getConfig().id == source_id;
        });

    if (it != video_sources_.end()) {
        video_sources_.erase(it, video_sources_.end());
        return true;
    }

    return false;
}

bool VideoSourceManager::startAll() {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    bool all_started = true;
    for (auto& source : video_sources_) {
        if (!source->start()) {
            all_started = false;
        }

        // 启动RTSP流（如果启用）
        if (source->getConfig().enable_rtsp) {
            if (!source->startRTSPStream()) {
                std::cerr << "启动RTSP流失败: " << source->getConfig().id << std::endl;
                all_started = false;
            }
        }
    }

    return all_started;
}

void VideoSourceManager::stopAll() {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    for (auto& source : video_sources_) {
        source->stop();
    }
}

void VideoSourceManager::setFrameCallback(std::function<void(const cv::Mat&, const std::string&)> callback) {
    frame_callback_ = callback;
}

std::vector<VideoSourceConfig> VideoSourceManager::getActiveSources() const {
    std::lock_guard<std::mutex> lock(sources_mutex_);

    std::vector<VideoSourceConfig> active_sources;
    for (const auto& source : video_sources_) {
        if (source->isRunning()) {
            active_sources.push_back(source->getConfig());
        }
    }

    return active_sources;
}

void VideoSourceManager::onFrameReceived(const cv::Mat& frame, const std::string& source_id) {
    if (frame_callback_) {
        frame_callback_(frame, source_id);
    }
}

// RTSP相关方法实现
bool VideoSource::startRTSPStream() {
    if (!config_.enable_rtsp || !rtsp_streamer_) {
        std::cerr << "RTSP 未启用或初始化失败" << std::endl;
        return false;
    }

    if (rtsp_streaming_) {
        std::cout << "RTSP 流已在运行" << std::endl;
        return true;
    }

    // 构建RTSP URL
    std::string rtsp_url = config_.rtsp_url;
    if (rtsp_url.empty()) {
        rtsp_url = "rtsp://0.0.0.0:" + std::to_string(config_.rtsp_port) + "/" + config_.id;
    }

    // 获取视频分辨率
    int width = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_WIDTH));
    int height = static_cast<int>(cap_.get(cv::CAP_PROP_FRAME_HEIGHT));

    if (width <= 0 || height <= 0) {
        width = 640;  // 默认分辨率
        height = 480;
    }

    // 初始化RTSP推流器
    if (!rtsp_streamer_->initialize(rtsp_url, width, height, config_.fps)) {
        std::cerr << "RTSP 推流器初始化失败" << std::endl;
        return false;
    }

    rtsp_streaming_ = true;
    std::cout << "RTSP 推流已启动: " << rtsp_url << std::endl;
    return true;
}

void VideoSource::stopRTSPStream() {
    if (rtsp_streamer_ && rtsp_streaming_) {
        rtsp_streamer_->shutdown();
        rtsp_streaming_ = false;
        std::cout << "RTSP 推流已停止" << std::endl;
    }
}

bool VideoSource::isRTSPStreaming() const {
    return rtsp_streaming_;
}
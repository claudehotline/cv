#include "RTSPInputHandler.h"
#include <iostream>
#include <thread>

RTSPInputHandler::RTSPInputHandler(const RTSPConfig& config)
    : config_(config), running_(false), connected_(false) {
    stats_.last_frame_time = std::chrono::steady_clock::now();
}

RTSPInputHandler::~RTSPInputHandler() {
    stop();
}

bool RTSPInputHandler::initialize() {
    // 设置OpenCV VideoCapture参数
    if (!config_.enable_buffering) {
        // 禁用缓冲以减少延迟
        capture_.set(cv::CAP_PROP_BUFFERSIZE, config_.buffer_size);
    }

    return true;
}

bool RTSPInputHandler::start() {
    if (running_) {
        return true;
    }

    running_ = true;
    capture_thread_ = std::thread(&RTSPInputHandler::captureLoop, this);

    std::cout << "RTSP输入处理器已启动: " << config_.rtsp_url << std::endl;
    return true;
}

void RTSPInputHandler::stop() {
    if (running_) {
        running_ = false;

        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }

        disconnect();
        std::cout << "RTSP输入处理器已停止" << std::endl;
    }
}

void RTSPInputHandler::setFrameCallback(std::function<void(const cv::Mat&)> callback) {
    frame_callback_ = callback;
}

void RTSPInputHandler::setStatusCallback(std::function<void(bool, const std::string&)> callback) {
    status_callback_ = callback;
}

RTSPInputHandler::RTSPStats RTSPInputHandler::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

bool RTSPInputHandler::isConnected() const {
    return connected_;
}

bool RTSPInputHandler::isRunning() const {
    return running_;
}

void RTSPInputHandler::captureLoop() {
    cv::Mat frame;
    auto last_fps_calc = std::chrono::steady_clock::now();
    int64_t frames_in_period = 0;

    while (running_) {
        // 尝试连接（如果未连接）
        if (!connected_ && !connect()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
            continue;
        }

        // 读取帧
        bool frame_read = capture_.read(frame);

        if (!frame_read || frame.empty()) {
            std::cerr << "RTSP流读取失败或帧为空" << std::endl;
            updateStats(false);

            // 尝试重连
            if (!reconnect()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
            }
            continue;
        }

        // 更新统计信息
        updateStats(true);
        frames_in_period++;

        // 调用帧回调
        if (frame_callback_) {
            frame_callback_(frame);
        }

        // 计算FPS（每秒更新一次）
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_fps_calc);

        if (duration.count() >= 1) {
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.current_fps = static_cast<double>(frames_in_period) / duration.count();
            }

            frames_in_period = 0;
            last_fps_calc = now;
        }

        // 小延迟以避免过度占用CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool RTSPInputHandler::connect() {
    if (connected_) {
        return true;
    }

    std::cout << "正在连接RTSP流: " << config_.rtsp_url << std::endl;

    // 设置连接参数
    capture_.set(cv::CAP_PROP_OPEN_TIMEOUT_MSEC, config_.timeout_ms);
    capture_.set(cv::CAP_PROP_READ_TIMEOUT_MSEC, config_.timeout_ms);

    if (!config_.enable_buffering) {
        capture_.set(cv::CAP_PROP_BUFFERSIZE, config_.buffer_size);
    }

    // 尝试打开RTSP流
    if (!capture_.open(config_.rtsp_url)) {
        std::string error = "无法连接到RTSP流: " + config_.rtsp_url;
        std::cerr << error << std::endl;
        notifyStatus(false, error);
        return false;
    }

    // 验证连接
    cv::Mat test_frame;
    if (!capture_.read(test_frame) || test_frame.empty()) {
        std::string error = "RTSP流连接成功但无法读取帧";
        std::cerr << error << std::endl;
        capture_.release();
        notifyStatus(false, error);
        return false;
    }

    connected_ = true;
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.is_connected = true;
        stats_.last_frame_time = std::chrono::steady_clock::now();
    }

    std::cout << "RTSP流连接成功: " << config_.rtsp_url << std::endl;
    notifyStatus(true, "");
    return true;
}

void RTSPInputHandler::disconnect() {
    if (capture_.isOpened()) {
        capture_.release();
    }

    connected_ = false;
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.is_connected = false;
    }

    notifyStatus(false, "断开连接");
}

bool RTSPInputHandler::reconnect() {
    std::cout << "RTSP流断开，尝试重连..." << std::endl;

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.reconnect_count++;
    }

    disconnect();

    // 等待一段时间后重连
    std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));

    for (int attempt = 0; attempt < config_.max_reconnect_attempts; ++attempt) {
        if (connect()) {
            std::cout << "RTSP流重连成功" << std::endl;
            return true;
        }

        std::cout << "重连尝试 " << (attempt + 1) << "/" << config_.max_reconnect_attempts << " 失败" << std::endl;

        if (attempt < config_.max_reconnect_attempts - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.reconnect_delay_ms));
        }
    }

    std::cerr << "RTSP流重连失败，已达最大尝试次数" << std::endl;
    return false;
}

void RTSPInputHandler::updateStats(bool frame_received) {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    auto now = std::chrono::steady_clock::now();

    if (frame_received) {
        stats_.frames_received++;
        stats_.last_frame_time = now;

        // 估算接收的字节数（粗略估算）
        stats_.bytes_received += 1920 * 1080 * 3; // 假设1080p RGB
    } else {
        stats_.frames_dropped++;
    }
}

void RTSPInputHandler::notifyStatus(bool connected, const std::string& error) {
    if (status_callback_) {
        status_callback_(connected, error);
    }
}
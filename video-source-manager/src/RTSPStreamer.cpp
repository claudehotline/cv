#include "RTSPStreamer.h"
#include <iostream>
#include <sstream>
#include <chrono>

#ifdef _WIN32
#include <io.h>
#define popen _popen
#define pclose _pclose
#endif

RTSPStreamer::RTSPStreamer()
    : width_(0), height_(0), fps_(25)
    , streaming_(false), initialized_(false)
    , start_time_(0), last_frame_time_(0)
#ifndef USE_FFMPEG_LIB
    , ffmpeg_pipe_(nullptr)
#endif
{
    // 初始化统计信息
    stats_.frames_sent = 0;
    stats_.avg_fps = 0.0;
    stats_.bytes_sent = 0;
    stats_.is_connected = false;
}

RTSPStreamer::~RTSPStreamer() {
    shutdown();
}

bool RTSPStreamer::initialize(const std::string& rtsp_url, int width, int height, int fps) {
    if (initialized_) {
        std::cerr << "RTSPStreamer 已经初始化" << std::endl;
        return false;
    }

    rtsp_url_ = rtsp_url;
    width_ = width;
    height_ = height;
    fps_ = fps;

    std::cout << "初始化 RTSP 推流器:" << std::endl;
    std::cout << "  URL: " << rtsp_url_ << std::endl;
    std::cout << "  分辨率: " << width_ << "x" << height_ << std::endl;
    std::cout << "  帧率: " << fps_ << " FPS" << std::endl;

#ifdef USE_FFMPEG_LIB
    if (!initializeFFmpegLibrary()) {
        return false;
    }
#else
    if (!initializeFFmpegPipe()) {
        return false;
    }
#endif

    initialized_ = true;
    start_time_ = getCurrentTimeMs();

    // 重置统计信息
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.frames_sent = 0;
    stats_.avg_fps = 0.0;
    stats_.bytes_sent = 0;
    stats_.is_connected = true;

    std::cout << "RTSP 推流器初始化成功" << std::endl;
    return true;
}

void RTSPStreamer::shutdown() {
    if (!initialized_) {
        return;
    }

    streaming_ = false;
    std::cout << "正在关闭 RTSP 推流器..." << std::endl;

#ifdef USE_FFMPEG_LIB
    cleanupFFmpegLibrary();
#else
    cleanupFFmpegPipe();
#endif

    initialized_ = false;

    // 更新统计信息
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.is_connected = false;

    std::cout << "RTSP 推流器已关闭" << std::endl;
}

bool RTSPStreamer::pushFrame(const cv::Mat& frame) {
    if (!initialized_ || !streaming_) {
        if (!streaming_ && initialized_) {
            streaming_ = true; // 第一帧时开始流
        } else {
            return false;
        }
    }

    if (frame.empty()) {
        std::cerr << "输入帧为空" << std::endl;
        return false;
    }

    // 检查帧尺寸
    if (frame.cols != width_ || frame.rows != height_) {
        std::cerr << "帧尺寸不匹配: 期望 " << width_ << "x" << height_
                  << ", 实际 " << frame.cols << "x" << frame.rows << std::endl;
        return false;
    }

#ifdef USE_FFMPEG_LIB
    // TODO: 实现FFmpeg库方案的帧推送
    return false;
#else
    return pushFramePipe(frame);
#endif
}

#ifndef USE_FFMPEG_LIB
bool RTSPStreamer::initializeFFmpegPipe() {
    // 构建FFmpeg命令行
    std::stringstream cmd;
    cmd << "ffmpeg -y -f rawvideo -vcodec rawvideo -pix_fmt bgr24"
        << " -s " << width_ << "x" << height_
        << " -r " << fps_
        << " -i - -c:v libx264 -pix_fmt yuv420p"
        << " -preset ultrafast -tune zerolatency"
        << " -f rtsp " << rtsp_url_;

    ffmpeg_command_ = cmd.str();

    std::cout << "FFmpeg 命令: " << ffmpeg_command_ << std::endl;

    // 打开管道
    ffmpeg_pipe_ = popen(ffmpeg_command_.c_str(), "wb");
    if (!ffmpeg_pipe_) {
        std::cerr << "无法启动 FFmpeg 进程" << std::endl;
        return false;
    }

    return true;
}

void RTSPStreamer::cleanupFFmpegPipe() {
    if (ffmpeg_pipe_) {
        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
    }
}

bool RTSPStreamer::pushFramePipe(const cv::Mat& frame) {
    if (!ffmpeg_pipe_) {
        return false;
    }

    // 控制帧率
    int64_t current_time = getCurrentTimeMs();
    int64_t expected_interval = 1000 / fps_;

    if (last_frame_time_ > 0) {
        int64_t actual_interval = current_time - last_frame_time_;
        if (actual_interval < expected_interval) {
            // 帧率过快，跳过这一帧
            return true;
        }
    }
    last_frame_time_ = current_time;

    // 写入帧数据到管道
    size_t frame_size = frame.total() * frame.elemSize();
    size_t written = fwrite(frame.data, 1, frame_size, ffmpeg_pipe_);

    if (written != frame_size) {
        std::cerr << "写入帧数据失败: 期望 " << frame_size
                  << " 字节，实际写入 " << written << " 字节" << std::endl;
        return false;
    }

    // 刷新管道缓冲区
    fflush(ffmpeg_pipe_);

    // 更新统计信息
    updateStats();

    return true;
}
#endif

#ifdef USE_FFMPEG_LIB
bool RTSPStreamer::initializeFFmpegLibrary() {
    // TODO: 实现FFmpeg库初始化
    std::cerr << "FFmpeg 库方案尚未实现，请使用命令行方案" << std::endl;
    return false;
}

void RTSPStreamer::cleanupFFmpegLibrary() {
    // TODO: 实现FFmpeg库清理
}
#endif

RTSPStreamer::StreamStats RTSPStreamer::getStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void RTSPStreamer::updateStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);

    stats_.frames_sent++;
    stats_.bytes_sent += width_ * height_ * 3; // BGR 3通道

    // 计算平均帧率
    int64_t current_time = getCurrentTimeMs();
    if (current_time > start_time_) {
        double elapsed_seconds = (current_time - start_time_) / 1000.0;
        stats_.avg_fps = stats_.frames_sent / elapsed_seconds;
    }
}

int64_t RTSPStreamer::getCurrentTimeMs() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}
#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

#ifdef USE_FFMPEG_LIB
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#endif

class RTSPStreamer {
public:
    RTSPStreamer();
    ~RTSPStreamer();

    // 初始化RTSP流
    bool initialize(const std::string& rtsp_url, int width, int height, int fps = 25);
    void shutdown();

    // 推送视频帧
    bool pushFrame(const cv::Mat& frame);
    bool isStreaming() const { return streaming_; }

    // 获取流媒体统计信息
    struct StreamStats {
        int frames_sent;
        double avg_fps;
        int64_t bytes_sent;
        bool is_connected;
    };
    StreamStats getStats() const;

private:
    std::string rtsp_url_;
    int width_, height_, fps_;

    std::atomic<bool> streaming_;
    std::atomic<bool> initialized_;

    mutable std::mutex stats_mutex_;
    StreamStats stats_;

    int64_t start_time_;
    int64_t last_frame_time_;

#ifdef USE_FFMPEG_LIB
    // FFmpeg库方案
    AVFormatContext* format_context_;
    AVCodecContext* codec_context_;
    AVStream* video_stream_;
    SwsContext* sws_context_;
    AVFrame* av_frame_;
    AVPacket* packet_;

    bool initializeFFmpegLibrary();
    void cleanupFFmpegLibrary();
#else
    // FFmpeg命令行方案
    FILE* ffmpeg_pipe_;
    std::string ffmpeg_command_;

    bool initializeFFmpegPipe();
    void cleanupFFmpegPipe();
    bool pushFramePipe(const cv::Mat& frame);
#endif

    void updateStats();
    int64_t getCurrentTimeMs();
};
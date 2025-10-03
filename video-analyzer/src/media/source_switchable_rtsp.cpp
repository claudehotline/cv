#include "media/source_switchable_rtsp.hpp"

#include <opencv2/imgproc.hpp>

#include "core/logger.hpp"

namespace va::media {

SwitchableRtspSource::SwitchableRtspSource(std::string uri)
    : uri_(std::move(uri)) {}

bool SwitchableRtspSource::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return true;
    }
    if (!openCapture()) {
        VA_LOG_ERROR() << "[RTSP] failed to open initial capture for URI " << uri_;
        return false;
    }
    running_ = true;
    frame_counter_ = 0;
    avg_latency_ms_ = 0.0;
    started_at_ = std::chrono::steady_clock::now();
    last_frame_time_ = started_at_;
    return true;
}

void SwitchableRtspSource::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }
    running_ = false;
    closeCapture();
}

bool SwitchableRtspSource::read(core::Frame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return false;
    }
    if (!capture_.isOpened()) {
        if (!openCapture()) {
            VA_LOG_WARN() << "[RTSP] reopen failed for URI " << uri_;
            return false;
        }
    }

    cv::Mat mat;
    if (!capture_.read(mat) || mat.empty()) {
        VA_LOG_WARN() << "[RTSP] failed to read frame for URI " << uri_;
        return false;
    }

    frame_counter_++;
    last_frame_time_ = std::chrono::steady_clock::now();

    avg_latency_ms_ = 0.0;

    frame.width = mat.cols;
    frame.height = mat.rows;
    frame.pts_ms = core::ms_now();
    if (!mat.isContinuous()) {
        mat = mat.clone();
    }
    frame.bgr.assign(mat.datastart, mat.dataend);

    return true;
}

SourceStats SwitchableRtspSource::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SourceStats stats;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_).count();
    if (elapsed > 0.0) {
        stats.fps = static_cast<double>(frame_counter_) * 1000.0 / elapsed;
    }
    stats.avg_latency_ms = avg_latency_ms_;
    stats.last_frame_id = frame_counter_;
    return stats;
}

bool SwitchableRtspSource::switchUri(const std::string& uri) {
    std::lock_guard<std::mutex> lock(mutex_);
    uri_ = uri;
    closeCapture();
    if (running_) {
        return openCapture();
    }
    return true;
}

bool SwitchableRtspSource::openCapture() {
    capture_.release();
    cv::VideoCapture cap(uri_, cv::CAP_FFMPEG);
    if (!cap.isOpened()) {
        VA_LOG_ERROR() << "[RTSP] cv::VideoCapture open failed for URI " << uri_;
        return false;
    }
    capture_ = std::move(cap);
    return true;
}

void SwitchableRtspSource::closeCapture() {
    if (capture_.isOpened()) {
        capture_.release();
    }
}

} // namespace va::media

#include "pipeline/DecoderStage.h"

#include <iostream>

DecoderStage::DecoderStage() = default;
DecoderStage::~DecoderStage() { close(); }

bool DecoderStage::open(const Options& options) {
    close();
    options_ = options;

    cv::VideoCapture cap;
    int apiPreference = cv::CAP_FFMPEG;
    cap.open(options.url, apiPreference);
    if (!cap.isOpened()) {
        std::cerr << "DecoderStage: failed to open source " << options.url << std::endl;
        return false;
    }

    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    capture_ = std::move(cap);
    return true;
}

void DecoderStage::close() {
    if (capture_.isOpened()) {
        capture_.release();
    }
}

bool DecoderStage::readFrame(cv::Mat& frame) {
    if (!capture_.isOpened()) return false;
    if (!capture_.read(frame)) {
        std::cerr << "DecoderStage: read frame failed" << std::endl;
        return false;
    }
    return true;
}

bool DecoderStage::isOpened() const {
    return capture_.isOpened();
}

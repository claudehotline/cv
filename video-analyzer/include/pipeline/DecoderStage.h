#pragma once

#include <string>
#include <memory>
#include <opencv2/opencv.hpp>

class DecoderStage {
public:
    struct Options {
        std::string url;
        bool prefer_tcp {true};
        int reconnect_interval_ms {2000};
        int timeout_ms {5000};
    };

    DecoderStage();
    ~DecoderStage();

    bool open(const Options& options);
    void close();

    bool readFrame(cv::Mat& frame);
    bool isOpened() const;

private:
    Options options_;
    cv::VideoCapture capture_;
};

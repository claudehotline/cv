#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include <string>
#include <cstdint>

#include "analysis/Types.h"

struct LetterboxInfo {
    float scale {1.0f};
    int pad_w {0};
    int pad_h {0};
    int in_w {640};
    int in_h {640};
};

class IPreprocessor {
public:
    virtual ~IPreprocessor() = default;
    virtual cv::Mat apply(const cv::Mat& bgr, int in_w, int in_h, LetterboxInfo& info) = 0;
};

class LetterboxPreprocessor : public IPreprocessor {
public:
    cv::Mat apply(const cv::Mat& bgr, int in_w, int in_h, LetterboxInfo& info) override;
};

class IDetectionPostprocessor {
public:
    virtual ~IDetectionPostprocessor() = default;
    virtual std::vector<DetectionResult> run(const float* data,
                                             const std::vector<int64_t>& dims,
                                             const LetterboxInfo& info,
                                             const cv::Size& original_size,
                                             const std::vector<std::string>& class_names,
                                             float confThresh,
                                             float iouThresh) = 0;
};

class YoloDetectPost : public IDetectionPostprocessor {
public:
    std::vector<DetectionResult> run(const float* data,
                                     const std::vector<int64_t>& dims,
                                     const LetterboxInfo& info,
                                     const cv::Size& original_size,
                                     const std::vector<std::string>& class_names,
                                     float confThresh,
                                     float iouThresh) override;
};


#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Lightweight task-specific interfaces and data contracts for future expansion.

struct PoseKeypoint {
    cv::Point2f pt;
    float score {0.0f};
    int id {0};
};

struct PoseResult {
    std::vector<PoseKeypoint> keypoints;
    float score {0.0f};
};

class ISegmentationModel {
public:
    virtual ~ISegmentationModel() = default;
    // Returns mask in the same size as input frame or provides scale metadata via an out parameter.
    virtual cv::Mat inferMask(const cv::Mat& frame) = 0;
};

class IPoseModel {
public:
    virtual ~IPoseModel() = default;
    virtual std::vector<PoseResult> inferPose(const cv::Mat& frame) = 0;
};


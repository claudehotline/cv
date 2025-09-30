#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <string>

#include "analysis/Types.h"

// Task-specific interfaces (DIP/LSP friendly)

class IDetectionModel {
public:
    virtual ~IDetectionModel() = default;
    virtual bool initialize(const std::string& model_path) = 0;
    virtual std::vector<DetectionResult> detectObjects(const cv::Mat& frame) = 0;
};

class ISegmentationModel {
public:
    virtual ~ISegmentationModel() = default;
    virtual bool initialize(const std::string& model_path) = 0;
    virtual SegmentationResult segmentInstances(const cv::Mat& frame) = 0;
};

class IPoseModel {
public:
    virtual ~IPoseModel() = default;
    virtual bool initialize(const std::string& model_path) = 0;
    virtual std::vector<PoseResult> inferPose(const cv::Mat& frame) = 0;
};


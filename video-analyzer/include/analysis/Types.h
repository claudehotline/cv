#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Core analysis task types shared across modules

enum class AnalysisType {
    OBJECT_DETECTION,
    INSTANCE_SEGMENTATION,
    POSE_ESTIMATION
};

struct DetectionResult {
    cv::Rect bbox;
    float confidence {0.0f};
    int class_id {0};
    std::string class_name;
};

struct SegmentationResult {
    std::vector<DetectionResult> detections;
    cv::Mat mask; // optional instance/semantic mask aligned to input frame size
};

struct PoseKeypoint {
    cv::Point2f pt;
    float score {0.0f};
    int id {0};
};

struct PoseResult {
    std::vector<PoseKeypoint> keypoints;
    float score {0.0f};
};


#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

#include "analysis/Types.h" // reuse DetectionResult, SegmentationResult
#include "analysis/Backend.h"
#include "analysis/OnnxRuntimeBackend.h"

#ifdef USE_ONNXRUNTIME

// Standalone ONNX YOLO-style detector implementation (extracted from VideoAnalyzer).
class OnnxYoloModel {
public:
    OnnxYoloModel();
    ~OnnxYoloModel();

    bool initialize(const std::string& model_path);
    bool initialize(const std::string& model_path, const InferenceConfig& config);
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame);
    SegmentationResult segmentInstances(const cv::Mat& frame);

private:
    std::unique_ptr<IInferenceBackend> backend_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<std::string> class_names_;

    int in_w_ {640};
    int in_h_ {640};
    float lb_scale_ {1.0f};
    int lb_pad_w_ {0};
    int lb_pad_h_ {0};

    cv::Mat preprocessImage(const cv::Mat& frame);
};

#endif // USE_ONNXRUNTIME

#pragma once

#include "analysis/Types.h"
#include "analysis/Interfaces.h"

// Adapter: wraps YOLOv12Detector to AIModel interface
class YoloDetectAdapter : public IDetectionModel {
public:
    YoloDetectAdapter();
    ~YoloDetectAdapter() override;

    bool initialize(const std::string& model_path) override;
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override;

private:
    std::shared_ptr<IDetectionModel> impl_;
};

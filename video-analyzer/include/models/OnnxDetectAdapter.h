#pragma once

#include "analysis/Types.h"
#include "analysis/Interfaces.h"

// Adapter to wrap existing ONNXModel into a detachable implementation.
class OnnxDetectAdapter : public IDetectionModel {
public:
    OnnxDetectAdapter();
    ~OnnxDetectAdapter() override;

    bool initialize(const std::string& model_path) override;
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override;

private:
    std::shared_ptr<IDetectionModel> impl_;
};

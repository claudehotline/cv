#pragma once

#include <memory>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "analysis/Interfaces.h"
#include "analysis/Backend.h"
#include "analysis/PrePost.h"
#include "analysis/Tensor.h"

// Template-method style base detector for YOLO-style models.
class BaseYoloDetector : public IDetectionModel {
public:
    BaseYoloDetector(std::unique_ptr<IInferenceBackend> backend,
                     std::unique_ptr<IPreprocessor> pre,
                     std::unique_ptr<IDetectionPostprocessor> post);
    ~BaseYoloDetector() override;

    bool initialize(const std::string& model_path) override;
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override;

protected:
    virtual int inputWidth() const { return 640; }
    virtual int inputHeight() const { return 640; }
    virtual const std::vector<std::string>& classNames() const = 0;
    virtual float confThreshold() const { return 0.65f; }
    virtual float iouThreshold() const { return 0.45f; }
    virtual int selectOutputIndex(const std::vector<Tensor>& outs) const { return 0; }
    virtual std::vector<const char*> inputBindingNames() const { return {}; }

protected:
    std::unique_ptr<IInferenceBackend> backend_;
    std::unique_ptr<IPreprocessor> preproc_;
    std::unique_ptr<IDetectionPostprocessor> postproc_;
};


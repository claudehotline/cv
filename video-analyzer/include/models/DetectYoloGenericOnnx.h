#pragma once

#include <memory>
#include <vector>
#include <string>

#include "analysis/detectors/BaseYoloDetector.h"
#include "analysis/OnnxRuntimeBackend.h"
#include "analysis/PrePost.h"

class DetectYoloGenericOnnx : public BaseYoloDetector {
public:
    DetectYoloGenericOnnx(std::unique_ptr<IInferenceBackend> backend,
                          std::unique_ptr<IPreprocessor> pre,
                          std::unique_ptr<IDetectionPostprocessor> post);
    ~DetectYoloGenericOnnx() override;

protected:
    const std::vector<std::string>& classNames() const override { return coco_; }
    float confThreshold() const override { return 0.65f; }
    float iouThreshold() const override { return 0.45f; }

private:
    std::vector<std::string> coco_;
};

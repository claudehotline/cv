#pragma once

#include <memory>
#include <string>
#include <vector>

#include "analysis/detectors/BaseYoloDetector.h"

class ConfigurableYoloDetector : public BaseYoloDetector {
public:
    struct Options {
        int in_w {640};
        int in_h {640};
        float conf {0.65f};
        float iou {0.45f};
        std::vector<std::string> class_names;
    };

    ConfigurableYoloDetector(Options opts,
                             std::unique_ptr<IInferenceBackend> backend,
                             std::unique_ptr<IPreprocessor> pre,
                             std::unique_ptr<IDetectionPostprocessor> post);
    ~ConfigurableYoloDetector() override;

protected:
    int inputWidth() const override { return opts_.in_w; }
    int inputHeight() const override { return opts_.in_h; }
    const std::vector<std::string>& classNames() const override { return opts_.class_names; }
    float confThreshold() const override { return opts_.conf; }
    float iouThreshold() const override { return opts_.iou; }

private:
    Options opts_;
};


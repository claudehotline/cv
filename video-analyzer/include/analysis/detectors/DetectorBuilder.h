#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "analysis/Interfaces.h"
#include "analysis/OnnxRuntimeBackend.h"

class IInferenceBackend;
class IPreprocessor;
class IDetectionPostprocessor;

// Fluent builder for configurable YOLO detectors
class DetectorBuilder {
public:
    DetectorBuilder& framework(const std::string& fw);
    DetectorBuilder& family(const std::string& fam);
    DetectorBuilder& inputSize(int w, int h);
    DetectorBuilder& thresholds(float conf, float iou);
    DetectorBuilder& classNames(std::vector<std::string> names);
    DetectorBuilder& inferenceConfig(const InferenceConfig& config);  // GPU/CPU configuration
    // Factories for custom components (optional)
    DetectorBuilder& backendFactory(std::function<std::unique_ptr<IInferenceBackend>()> f);
    DetectorBuilder& preprocessorFactory(std::function<std::unique_ptr<IPreprocessor>()> f);
    DetectorBuilder& postprocessorFactory(std::function<std::unique_ptr<IDetectionPostprocessor>()> f);

    std::shared_ptr<IDetectionModel> buildDetect() const;

    static std::vector<std::string> coco80();

private:
    std::string framework_ = "onnx";
    std::string family_;
    int in_w_ = 640, in_h_ = 640;
    float conf_ = 0.25f, iou_ = 0.45f;
    std::vector<std::string> classes_;
    InferenceConfig inference_config_;  // GPU/CPU configuration
    std::function<std::unique_ptr<IInferenceBackend>()> backend_factory_;
    std::function<std::unique_ptr<IPreprocessor>()> preproc_factory_;
    std::function<std::unique_ptr<IDetectionPostprocessor>()> postproc_factory_;
};

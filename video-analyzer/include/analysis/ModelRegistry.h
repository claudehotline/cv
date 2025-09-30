#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "analysis/Interfaces.h"
#include "analysis/Types.h"
#include "analysis/OnnxRuntimeBackend.h"

// Minimal model registry to satisfy OCP/DIP without large refactor.
// Returns AIModel adapters based on a simple key/family inference.

struct ModelDesc {
    std::string id;           // e.g. "yolov12x"
    std::string path;         // onnx or engine path
    std::string family;       // e.g. "yolo", "yolov12", "detr" (optional)
    std::string framework;    // e.g. "onnx", "tensorrt" (optional)
    AnalysisType task { AnalysisType::OBJECT_DETECTION };
    InferenceConfig inference_config;  // GPU/CPU configuration
    float confidence_threshold {0.0f};
    float nms_threshold {0.0f};
    int input_width {0};
    int input_height {0};
};

class ModelRegistry {
public:
    using DetectCreator = std::function<std::shared_ptr<IDetectionModel>(const ModelDesc&)>;
    using SegCreator = std::function<std::shared_ptr<ISegmentationModel>(const ModelDesc&)>;
    using PoseCreator = std::function<std::shared_ptr<IPoseModel>(const ModelDesc&)>;

    static ModelRegistry& instance();

    void registerDetector(const std::string& key, DetectCreator fn);
    void registerSegmenter(const std::string& key, SegCreator fn);
    void registerPose(const std::string& key, PoseCreator fn);

    // Heuristic creation: try by family key, otherwise fallbacks are tried.
    std::shared_ptr<IDetectionModel> createDetector(const ModelDesc& desc) const;
    std::shared_ptr<ISegmentationModel> createSegmenter(const ModelDesc& desc) const;
    std::shared_ptr<IPoseModel> createPose(const ModelDesc& desc) const;

private:
    std::map<std::string, DetectCreator> detect_creators_;
    std::map<std::string, SegCreator> seg_creators_;
    std::map<std::string, PoseCreator> pose_creators_;
};

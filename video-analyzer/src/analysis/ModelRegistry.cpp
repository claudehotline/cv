#include "analysis/ModelRegistry.h"

#include <algorithm>
#include <cctype>

#ifdef USE_ONNXRUNTIME
#include "models/YoloDetectAdapter.h"
#endif

// Fallbacks

#ifdef USE_ONNXRUNTIME
// Generic ONNX model from existing codebase
class ONNXModel;
#endif

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}

ModelRegistry& ModelRegistry::instance() {
    static ModelRegistry inst;
    return inst;
}

void ModelRegistry::registerDetector(const std::string& key, DetectCreator fn) {
    detect_creators_[toLower(key)] = std::move(fn);
}

void ModelRegistry::registerSegmenter(const std::string& key, SegCreator fn) {
    seg_creators_[toLower(key)] = std::move(fn);
}

void ModelRegistry::registerPose(const std::string& key, PoseCreator fn) {
    pose_creators_[toLower(key)] = std::move(fn);
}

std::shared_ptr<IDetectionModel> ModelRegistry::createDetector(const ModelDesc& desc) const {
    const std::string fam = toLower(desc.family);
    const std::string id = toLower(desc.id);
    const std::string path = toLower(desc.path);

    auto tryKey = [&](const std::string& k) -> std::shared_ptr<IDetectionModel> {
        auto it = detect_creators_.find(k);
        if (it != detect_creators_.end()) return it->second(desc);
        return nullptr;
    };

    // 1) exact family key
    if (!fam.empty()) {
        if (auto m = tryKey(fam)) return m;
    }

    // 2) heuristic by id or path
    if (id.find("yolov12") != std::string::npos || path.find("yolov12") != std::string::npos) {
        if (auto m = tryKey("yolov12")) return m;
        if (auto m2 = tryKey("yolo")) return m2;
    }
    if (id.find("yolo") != std::string::npos || path.find("yolo") != std::string::npos) {
        if (auto m = tryKey("yolo")) return m;
    }

    // 3) as a last resort try any default creator registered under "default"
    if (auto m = tryKey("default")) return m;

    return nullptr;
}

std::shared_ptr<ISegmentationModel> ModelRegistry::createSegmenter(const ModelDesc& desc) const {
    const std::string fam = toLower(desc.family);
    const std::string id = toLower(desc.id);
    const std::string path = toLower(desc.path);

    auto tryKey = [&](const std::string& k) -> std::shared_ptr<ISegmentationModel> {
        auto it = seg_creators_.find(k);
        if (it != seg_creators_.end()) return it->second(desc);
        return nullptr;
    };

    if (!fam.empty()) if (auto m = tryKey(fam)) return m;
    if (id.find("yolo") != std::string::npos || path.find("yolo") != std::string::npos) if (auto m = tryKey("yolo-seg")) return m;
    if (auto m = tryKey("default")) return m;
    return nullptr;
}

std::shared_ptr<IPoseModel> ModelRegistry::createPose(const ModelDesc& desc) const {
    const std::string fam = toLower(desc.family);
    const std::string id = toLower(desc.id);
    const std::string path = toLower(desc.path);

    auto tryKey = [&](const std::string& k) -> std::shared_ptr<IPoseModel> {
        auto it = pose_creators_.find(k);
        if (it != pose_creators_.end()) return it->second(desc);
        return nullptr;
    };

    if (!fam.empty()) if (auto m = tryKey(fam)) return m;
    if (id.find("hrnet") != std::string::npos || path.find("hrnet") != std::string::npos) if (auto m = tryKey("hrnet")) return m;
    if (auto m = tryKey("default")) return m;
    return nullptr;
}

#include "models/OnnxDetectAdapter.h"

#include "analysis/ModelRegistry.h"
#include "analysis/detectors/DetectorBuilder.h"

OnnxDetectAdapter::OnnxDetectAdapter() = default;
OnnxDetectAdapter::~OnnxDetectAdapter() = default;

bool OnnxDetectAdapter::initialize(const std::string& model_path) {
    DetectorBuilder b;
    b.framework("onnx").family("default").inputSize(640, 640).thresholds(0.25f, 0.45f).classNames(DetectorBuilder::coco80());
    impl_ = b.buildDetect();
    return impl_ && impl_->initialize(model_path);
}

std::vector<DetectionResult> OnnxDetectAdapter::detectObjects(const cv::Mat& frame) {
    return impl_ ? impl_->detectObjects(frame) : std::vector<DetectionResult>{};
}

namespace {
struct Registrar {
    Registrar() {
        // Default creator for detectors when family is unknown
        ModelRegistry::instance().registerDetector("default", [](const ModelDesc& desc){
            DetectorBuilder b;
            b.framework(desc.framework).family("default").inputSize(640, 640).thresholds(0.25f, 0.45f).classNames(DetectorBuilder::coco80());
            auto m = b.buildDetect();
            if (m && m->initialize(desc.path)) return m;
            return std::shared_ptr<IDetectionModel>();
        });
    }
} registrar_instance;
}


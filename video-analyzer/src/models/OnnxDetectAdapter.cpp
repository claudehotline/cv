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
            const std::string framework = desc.framework.empty() ? "onnx" : desc.framework;
            const int input_w = desc.input_width > 0 ? desc.input_width : 640;
            const int input_h = desc.input_height > 0 ? desc.input_height : 640;
            const float conf = desc.confidence_threshold > 0.0f ? desc.confidence_threshold : 0.25f;
            const float iou = desc.nms_threshold > 0.0f ? desc.nms_threshold : 0.45f;

            b.framework(framework)
             .family("default")
             .inputSize(input_w, input_h)
             .thresholds(conf, iou)
             .classNames(DetectorBuilder::coco80());
            auto m = b.buildDetect();
            if (m && m->initialize(desc.path)) return m;
            return std::shared_ptr<IDetectionModel>();
        });
    }
} registrar_instance;
}

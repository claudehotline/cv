#include "models/YoloDetectAdapter.h"

#include "analysis/ModelRegistry.h"
#include "analysis/detectors/DetectorBuilder.h"

YoloDetectAdapter::YoloDetectAdapter() = default;
YoloDetectAdapter::~YoloDetectAdapter() = default;

bool YoloDetectAdapter::initialize(const std::string& model_path) {
    DetectorBuilder b;
    b.framework("onnx").family("yolov12").inputSize(640, 640).thresholds(0.25f, 0.45f).classNames(DetectorBuilder::coco80());
    impl_ = b.buildDetect();
    return impl_ && impl_->initialize(model_path);
}

std::vector<DetectionResult> YoloDetectAdapter::detectObjects(const cv::Mat& frame) {
    return impl_ ? impl_->detectObjects(frame) : std::vector<DetectionResult>{};
}

namespace {
struct Registrar {
    Registrar() {
        ModelRegistry::instance().registerDetector("yolov12", [](const ModelDesc& desc){
            DetectorBuilder b;
            b.framework(desc.framework).family("yolov12").inputSize(640, 640).thresholds(0.65f, 0.45f).classNames(DetectorBuilder::coco80()).inferenceConfig(desc.inference_config);
            auto m = b.buildDetect();
            if (m && m->initialize(desc.path)) return m;
            return std::shared_ptr<IDetectionModel>();
        });
        ModelRegistry::instance().registerDetector("yolo", [](const ModelDesc& desc){
            DetectorBuilder b;
            b.framework(desc.framework).family("yolo").inputSize(640, 640).thresholds(0.65f, 0.45f).classNames(DetectorBuilder::coco80()).inferenceConfig(desc.inference_config);
            auto m = b.buildDetect();
            if (m && m->initialize(desc.path)) return m;
            return std::shared_ptr<IDetectionModel>();
        });
    }
} registrar_instance;
}


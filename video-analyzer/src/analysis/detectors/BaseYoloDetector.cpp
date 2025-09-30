#include "analysis/detectors/BaseYoloDetector.h"

BaseYoloDetector::BaseYoloDetector(std::unique_ptr<IInferenceBackend> backend,
                                   std::unique_ptr<IPreprocessor> pre,
                                   std::unique_ptr<IDetectionPostprocessor> post)
    : backend_(std::move(backend)), preproc_(std::move(pre)), postproc_(std::move(post)) {}

BaseYoloDetector::~BaseYoloDetector() = default;

bool BaseYoloDetector::initialize(const std::string& model_path) {
    return backend_ && backend_->loadModel(model_path);
}

std::vector<DetectionResult> BaseYoloDetector::detectObjects(const cv::Mat& frame) {
    std::vector<DetectionResult> out;
    if (!backend_ || !preproc_ || !postproc_ || frame.empty()) return out;

    LetterboxInfo linfo;
    const int in_w = inputWidth();
    const int in_h = inputHeight();
    cv::Mat chw = preproc_->apply(frame, in_w, in_h, linfo);

    Tensor tin; tin.shape = {1, 3, in_h, in_w};
    tin.data.assign(chw.ptr<float>(), chw.ptr<float>() + (size_t)(3*in_h*in_w));

    std::vector<Tensor> outputs;
    if (!backend_->infer({tin}, outputs) || outputs.empty()) return out;
    int idx = std::max(0, std::min(selectOutputIndex(outputs), (int)outputs.size()-1));
    const Tensor& tout = outputs[(size_t)idx];

    out = postproc_->run(tout.data.data(), tout.shape, linfo, frame.size(), classNames(), confThreshold(), iouThreshold());
    return out;
}


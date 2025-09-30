#include "models/OnnxYoloModel.h"

#ifdef USE_ONNXRUNTIME

#include <algorithm>
#include <cmath>
#include <iostream>
#include "analysis/PrePost.h"
#include "analysis/OnnxRuntimeBackend.h"

namespace {
}

OnnxYoloModel::OnnxYoloModel() { }
OnnxYoloModel::~OnnxYoloModel() = default;

bool OnnxYoloModel::initialize(const std::string& model_path) {
    // Use default CPU configuration
    InferenceConfig default_config;
    default_config.device = InferenceDevice::CPU;
    default_config.num_threads = 4;
    return initialize(model_path, default_config);
}

bool OnnxYoloModel::initialize(const std::string& model_path, const InferenceConfig& config) {
    try {
        auto ort_backend = std::make_unique<OnnxRuntimeBackend>();

        // Apply inference configuration before loading model
        ort_backend->setInferenceConfig(config);

        if (!ort_backend->loadModel(model_path)) return false;
        backend_ = std::move(ort_backend);
        input_names_ = backend_->getInputNames();
        output_names_ = backend_->getOutputNames();
        class_names_ = {"person","bicycle","car","motorcycle","airplane","bus","train","truck","boat","traffic light"};
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ONNX init failed: " << e.what() << std::endl; return false;
    }
}

cv::Mat OnnxYoloModel::preprocessImage(const cv::Mat& frame) {
    static LetterboxPreprocessor pre;
    LetterboxInfo info;
    in_w_ = 640; in_h_ = 640;
    cv::Mat chw = pre.apply(frame, in_w_, in_h_, info);
    lb_scale_ = info.scale; lb_pad_w_ = info.pad_w; lb_pad_h_ = info.pad_h;
    return chw;
}

std::vector<DetectionResult> OnnxYoloModel::detectObjects(const cv::Mat& frame) {
    std::vector<DetectionResult> results;
    try {
        const int inW = in_w_, inH = in_h_;
        cv::Mat chw = preprocessImage(frame);
        std::vector<int64_t> ishape = {1,3,inH,inW};
        size_t n = static_cast<size_t>(1ull*3*inH*inW);
        Tensor tin; tin.shape = ishape; tin.data.assign(chw.ptr<float>(), chw.ptr<float>() + n);
        std::vector<Tensor> outs;
        if (!backend_ || !backend_->infer({tin}, outs) || outs.empty()) return results;

        // Assume first output contains detections in either [1, N, C] or [1, C, N]
        Tensor& out = outs[0];
        auto dims = out.shape;
        if (dims.size() < 3) return results;

        const float confThresh = 0.65f;
        const float nmsThresh = 0.45f;
        const float* data = out.data.data();
        YoloDetectPost post;
        LetterboxInfo info; info.scale = lb_scale_; info.pad_w = lb_pad_w_; info.pad_h = lb_pad_h_; info.in_w = inW; info.in_h = inH;
        results = post.run(data, dims, info, frame.size(), class_names_, confThresh, nmsThresh);
    } catch (const std::exception& e) {
        std::cerr << "ONNX inference failed: " << e.what() << std::endl;
    }
    return results;
}

SegmentationResult OnnxYoloModel::segmentInstances(const cv::Mat& frame) {
    SegmentationResult s; s.detections = detectObjects(frame); s.mask = cv::Mat::zeros(frame.size(), CV_8UC1); return s;
}

#endif // USE_ONNXRUNTIME

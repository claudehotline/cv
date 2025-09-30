#include "analysis/detectors/DetectorBuilder.h"

#include <algorithm>
#include <cctype>

#include "analysis/OnnxRuntimeBackend.h"
#include "analysis/PrePost.h"
#include "analysis/detectors/ConfigurableYoloDetector.h"

namespace {
static std::string toLower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }
}

DetectorBuilder& DetectorBuilder::framework(const std::string& fw) { framework_ = fw; return *this; }
DetectorBuilder& DetectorBuilder::family(const std::string& fam) { family_ = fam; return *this; }
DetectorBuilder& DetectorBuilder::inputSize(int w, int h) { in_w_ = w; in_h_ = h; return *this; }
DetectorBuilder& DetectorBuilder::thresholds(float conf, float iou) { conf_ = conf; iou_ = iou; return *this; }
DetectorBuilder& DetectorBuilder::classNames(std::vector<std::string> names) { classes_ = std::move(names); return *this; }
DetectorBuilder& DetectorBuilder::inferenceConfig(const InferenceConfig& config) { inference_config_ = config; return *this; }
DetectorBuilder& DetectorBuilder::backendFactory(std::function<std::unique_ptr<IInferenceBackend>()> f) { backend_factory_ = std::move(f); return *this; }
DetectorBuilder& DetectorBuilder::preprocessorFactory(std::function<std::unique_ptr<IPreprocessor>()> f) { preproc_factory_ = std::move(f); return *this; }
DetectorBuilder& DetectorBuilder::postprocessorFactory(std::function<std::unique_ptr<IDetectionPostprocessor>()> f) { postproc_factory_ = std::move(f); return *this; }

std::shared_ptr<IDetectionModel> DetectorBuilder::buildDetect() const {
    // Resolve components
    std::unique_ptr<IInferenceBackend> backend;
    if (backend_factory_) backend = backend_factory_();
    if (!backend) {
        const auto fw = toLower(framework_);
        // Extend here for "tensorrt"/"openvino" etc.
        auto ort_backend = std::make_unique<OnnxRuntimeBackend>();

        // Apply GPU/CPU configuration
        ort_backend->setInferenceConfig(inference_config_);

        backend = std::move(ort_backend);
    }

    std::unique_ptr<IPreprocessor> pre = preproc_factory_ ? preproc_factory_() : std::make_unique<LetterboxPreprocessor>();
    std::unique_ptr<IDetectionPostprocessor> post = postproc_factory_ ? postproc_factory_() : std::make_unique<YoloDetectPost>();

    ConfigurableYoloDetector::Options opts; opts.in_w = in_w_; opts.in_h = in_h_; opts.conf = conf_; opts.iou = iou_;
    opts.class_names = classes_.empty() ? coco80() : classes_;
    auto det = std::make_shared<ConfigurableYoloDetector>(std::move(opts), std::move(backend), std::move(pre), std::move(post));
    return det;
}

std::vector<std::string> DetectorBuilder::coco80() {
    return {
        "person","bicycle","car","motorcycle","airplane","bus","train","truck","boat",
        "traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat",
        "dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack",
        "umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball",
        "kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket",
        "bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple",
        "sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair",
        "couch","potted plant","bed","dining table","toilet","tv","laptop","mouse",
        "remote","keyboard","cell phone","microwave","oven","toaster","sink","refrigerator",
        "book","clock","vase","scissors","teddy bear","hair drier","toothbrush"
    };
}

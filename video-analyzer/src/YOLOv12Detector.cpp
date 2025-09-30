#ifdef USE_ONNXRUNTIME

#include "YOLOv12Detector.h"

#include <opencv2/dnn/dnn.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace {
    constexpr float kEps = 1e-6f;
}

YOLOv12Detector::YOLOv12Detector()
    : input_size_(640, 640),
      conf_threshold_(0.25f),
      iou_threshold_(0.45f) {
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "YOLOv12");
    session_options_ = std::make_unique<Ort::SessionOptions>();
    session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Try GPU first, fallback to CPU
    enableGPU(0);

    initializeClassNames();
}

YOLOv12Detector::~YOLOv12Detector() = default;

void YOLOv12Detector::enableGPU(int device_id) {
#ifdef USE_CUDA
    try {
        OrtCUDAProviderOptions cuda_options{};
        cuda_options.device_id = device_id;
        cuda_options.gpu_mem_limit = SIZE_MAX;
        cuda_options.arena_extend_strategy = 1;
        cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchDefault;
        cuda_options.do_copy_in_default_stream = 1;

        session_options_->AppendExecutionProvider_CUDA(cuda_options);
        session_options_->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

        std::cout << "✅ YOLOv12: CUDA GPU inference enabled (Device " << device_id << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "⚠️  YOLOv12: CUDA not available, falling back to CPU: " << e.what() << std::endl;
        enableCPU(4);
    }
#else
    std::cerr << "⚠️  YOLOv12: CUDA not compiled, using CPU" << std::endl;
    enableCPU(4);
#endif
}

void YOLOv12Detector::enableCPU(int num_threads) {
    session_options_->SetIntraOpNumThreads(num_threads);
    session_options_->SetInterOpNumThreads(num_threads);
    session_options_->SetExecutionMode(ExecutionMode::ORT_PARALLEL);
    session_options_->EnableMemPattern();
    session_options_->EnableCpuMemArena();

    std::cout << "✅ YOLOv12: CPU inference enabled (" << num_threads << " threads)" << std::endl;
}

bool YOLOv12Detector::initialize(const std::string& model_path) {
    try {
#ifdef _WIN32
        std::wstring wmodel_path(model_path.begin(), model_path.end());
        session_ = std::make_unique<Ort::Session>(*env_, wmodel_path.c_str(), *session_options_);
#else
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), *session_options_);
#endif

        Ort::AllocatorWithDefaultOptions allocator;

        input_names_.clear();
        const size_t input_count = session_->GetInputCount();
        input_names_.reserve(input_count);
        for (size_t i = 0; i < input_count; ++i) {
            Ort::AllocatedStringPtr name = session_->GetInputNameAllocated(i, allocator);
            input_names_.emplace_back(name.get());
        }

        output_names_.clear();
        const size_t output_count = session_->GetOutputCount();
        output_names_.reserve(output_count);
        for (size_t i = 0; i < output_count; ++i) {
            Ort::AllocatedStringPtr name = session_->GetOutputNameAllocated(i, allocator);
            output_names_.emplace_back(name.get());
        }

        return true;
    } catch (const std::exception& ex) {
        std::cerr << "YOLOv12 model initialization failed: " << ex.what() << std::endl;
        session_.reset();
        return false;
    }
}

std::vector<YOLODetection> YOLOv12Detector::detect(const cv::Mat& image) {
    std::vector<YOLODetection> detections;

    if (!session_) {
        std::cerr << "YOLOv12 session is not initialized." << std::endl;
        return detections;
    }
    if (image.empty()) {
        std::cerr << "YOLOv12 detector received an empty frame." << std::endl;
        return detections;
    }

    cv::Size original_size = image.size();
    cv::Mat input_blob = preprocess(image);

    std::vector<int64_t> input_shape = {1, 3, input_size_.height, input_size_.width};
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(memory_info,
                                                             input_blob.ptr<float>(),
                                                             static_cast<size_t>(input_blob.total()),
                                                             input_shape.data(),
                                                             input_shape.size());

    std::vector<const char*> input_name_ptrs;
    for (const auto& name : input_names_) input_name_ptrs.push_back(name.c_str());
    std::vector<const char*> output_name_ptrs;
    for (const auto& name : output_names_) output_name_ptrs.push_back(name.c_str());

    try {
        auto outputs = session_->Run(Ort::RunOptions{nullptr},
                                     input_name_ptrs.data(), &input_tensor, 1,
                                     output_name_ptrs.data(), output_name_ptrs.size());
        if (outputs.empty()) return detections;

        const Ort::Value& out = outputs.front();
        const auto info = out.GetTensorTypeAndShapeInfo();
        const float* data = out.GetTensorData<float>();
        std::vector<int64_t> dims = info.GetShape();

        detections = postprocess(data, dims, original_size);
    } catch (const std::exception& ex) {
        std::cerr << "YOLOv12 inference failed: " << ex.what() << std::endl;
    }

    return detections;
}

cv::Mat YOLOv12Detector::preprocess(const cv::Mat& image) {
    // Letterbox to input_size_ maintaining aspect ratio
    const int in_w = input_size_.width;
    const int in_h = input_size_.height;
    const int src_w = image.cols, src_h = image.rows;
    const float r = std::min(static_cast<float>(in_w) / src_w, static_cast<float>(in_h) / src_h);
    const int new_w = static_cast<int>(std::round(src_w * r));
    const int new_h = static_cast<int>(std::round(src_h * r));
    lb_pad_w_ = (in_w - new_w) / 2;
    lb_pad_h_ = (in_h - new_h) / 2;
    lb_scale_ = r;

    cv::Mat resized; cv::resize(image, resized, cv::Size(new_w, new_h));
    cv::Mat canvas(in_h, in_w, image.type(), cv::Scalar(114,114,114));
    resized.copyTo(canvas(cv::Rect(lb_pad_w_, lb_pad_h_, new_w, new_h)));

    cv::Mat rgb; cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    cv::Mat f32; rgb.convertTo(f32, CV_32F, 1.0f/255.0f);
    std::vector<cv::Mat> channels; cv::split(f32, channels);
    const int channel_size = in_w * in_h;
    cv::Mat chw(1, 3 * channel_size, CV_32F);
    float* dst = chw.ptr<float>();
    for (int c=0;c<3;++c) std::memcpy(dst + c*channel_size, channels[c].ptr<float>(), channel_size*sizeof(float));
    return chw;
}

std::vector<YOLODetection> YOLOv12Detector::postprocess(const float* data,
                                                        const std::vector<int64_t>& dims,
                                                        const cv::Size& original_size) {
    std::vector<YOLODetection> detections;
    if (dims.size() < 3) return detections;

    // dims either [1, N, C] or [1, C, N]
    int64_t N = 0, C = 0; bool det_major = false;
    if (dims[1] < dims[2]) { // [1, N, C]
        N = dims[1]; C = dims[2]; det_major = true;
    } else { // [1, C, N]
        C = dims[1]; N = dims[2]; det_major = false;
    }
    const int attributes = 5 + static_cast<int>(class_names_.size());
    if (C < attributes) return detections;

    auto getVal = [&](int i, int a) -> float {
        if (det_major) return data[i * C + a]; // [N,C]
        else return data[a * N + i];           // [C,N]
    };

    std::vector<cv::Rect> boxes; std::vector<float> scores; std::vector<int> class_ids;
    boxes.reserve(static_cast<size_t>(N)); scores.reserve(static_cast<size_t>(N)); class_ids.reserve(static_cast<size_t>(N));

    for (int i=0; i<static_cast<int>(N); ++i) {
        float obj = getVal(i, 4); if (obj < kEps) continue;
        int best_id = -1; float best = 0.0f;
        for (int c=0; c<static_cast<int>(class_names_.size()); ++c) {
            float s = getVal(i, 5 + c); if (s > best) { best = s; best_id = c; }
        }
        float conf = obj * best; if (best_id < 0 || conf < conf_threshold_) continue;

        float cx = getVal(i, 0), cy = getVal(i, 1), w = std::max(getVal(i, 2), 0.0f), h = std::max(getVal(i, 3), 0.0f);
        // normalize to input if needed
        if (cx <= 1.5f && cy <= 1.5f && w <= 1.5f && h <= 1.5f) {
            cx *= static_cast<float>(input_size_.width);
            cy *= static_cast<float>(input_size_.height);
            w  *= static_cast<float>(input_size_.width);
            h  *= static_cast<float>(input_size_.height);
        }
        // undo letterbox
        float x0_in = (cx - 0.5f*w) - static_cast<float>(lb_pad_w_);
        float y0_in = (cy - 0.5f*h) - static_cast<float>(lb_pad_h_);
        float x1_in = (cx + 0.5f*w) - static_cast<float>(lb_pad_w_);
        float y1_in = (cy + 0.5f*h) - static_cast<float>(lb_pad_h_);
        float x0 = x0_in / lb_scale_, y0 = y0_in / lb_scale_, x1 = x1_in / lb_scale_, y1 = y1_in / lb_scale_;
        float bw = std::max(0.0f, x1 - x0), bh = std::max(0.0f, y1 - y0);

        x0 = std::clamp(x0, 0.0f, static_cast<float>(original_size.width - 1));
        y0 = std::clamp(y0, 0.0f, static_cast<float>(original_size.height - 1));
        bw = std::clamp(bw, 0.0f, static_cast<float>(original_size.width) - x0);
        bh = std::clamp(bh, 0.0f, static_cast<float>(original_size.height) - y0);

        boxes.emplace_back(static_cast<int>(std::round(x0)), static_cast<int>(std::round(y0)),
                           static_cast<int>(std::round(bw)), static_cast<int>(std::round(bh)));
        scores.push_back(conf); class_ids.push_back(best_id);
    }

    if (boxes.empty()) return detections;
    std::vector<int> kept = nonMaximumSuppression(boxes, scores, iou_threshold_);
    for (int idx : kept) {
        if (idx < 0 || idx >= static_cast<int>(boxes.size())) continue;
        YOLODetection d; d.bbox = boxes[idx]; d.confidence = scores[idx]; d.class_id = class_ids[idx];
        d.class_name = (d.class_id >= 0 && d.class_id < static_cast<int>(class_names_.size())) ? class_names_[d.class_id] : std::string("unknown");
        detections.push_back(std::move(d));
    }
    return detections;
}

void YOLOv12Detector::initializeClassNames() {
    class_names_ = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
        "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
        "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
        "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
        "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
        "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
        "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
        "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator",
        "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
    };
}

std::vector<int> YOLOv12Detector::nonMaximumSuppression(const std::vector<cv::Rect>& boxes,
                                                        const std::vector<float>& scores,
                                                        float threshold) {
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, conf_threshold_, threshold, indices);
    return indices;
}

#endif // USE_ONNXRUNTIME


#pragma once

#ifdef USE_ONNXRUNTIME
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>

struct YOLODetection {
    cv::Rect bbox;
    float confidence;
    int class_id;
    std::string class_name;
};

class YOLOv12Detector {
public:
    YOLOv12Detector();
    ~YOLOv12Detector();

    bool initialize(const std::string& model_path);
    std::vector<YOLODetection> detect(const cv::Mat& image);

    // 设置检测参数
    void setConfidenceThreshold(float threshold) { conf_threshold_ = threshold; }
    void setIOUThreshold(float threshold) { iou_threshold_ = threshold; }
    void setInputSize(const cv::Size& size) { input_size_ = size; }

    // GPU configuration
    void enableGPU(int device_id = 0);
    void enableCPU(int num_threads = 4);

private:
    // ONNX Runtime相关
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;

    // 输入输出名称
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    // 模型参数
    cv::Size input_size_;
    float conf_threshold_;
    float iou_threshold_;

    // COCO类别名称
    std::vector<std::string> class_names_;

    // 私有方法
    cv::Mat preprocess(const cv::Mat& image);
    std::vector<YOLODetection> postprocess(
        const float* data,
        const std::vector<int64_t>& dims,
        const cv::Size& original_size);

    void initializeClassNames();
    std::vector<int> nonMaximumSuppression(
        const std::vector<cv::Rect>& boxes,
        const std::vector<float>& scores,
        float threshold);

    // letterbox parameters for correct box mapping
    float lb_scale_ {1.0f};
    int lb_pad_w_ {0};
    int lb_pad_h_ {0};
    bool use_letterbox_ {true};
};

#endif // USE_ONNXRUNTIME


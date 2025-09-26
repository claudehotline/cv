#include "VideoAnalyzer.h"
#include <json/json.h>
#include <fstream>
#include <iostream>

// SimpleModel 实现（作为备用模型）
bool SimpleModel::initialize(const std::string& model_path) {
    std::string cascade_path = model_path + "/haarcascade_frontalface_alt.xml";
    model_loaded_ = face_cascade_.load(cascade_path);

    if (!model_loaded_) {
        std::cerr << "警告: 无法加载Haar级联分类器，将使用模拟检测" << std::endl;
        model_loaded_ = true; // 允许使用模拟检测
    }

    return model_loaded_;
}

std::vector<DetectionResult> SimpleModel::detectObjects(const cv::Mat& frame) {
    std::vector<DetectionResult> results;

    if (!face_cascade_.empty()) {
        std::vector<cv::Rect> faces;
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        face_cascade_.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(30, 30));

        for (const auto& face : faces) {
            DetectionResult result;
            result.bbox = face;
            result.confidence = 0.85f; // 模拟置信度
            result.class_id = 0;
            result.class_name = "face";
            results.push_back(result);
        }
    } else {
        // 模拟检测结果
        DetectionResult mock_result;
        mock_result.bbox = cv::Rect(50, 50, 100, 100);
        mock_result.confidence = 0.75f;
        mock_result.class_id = 0;
        mock_result.class_name = "mock_object";
        results.push_back(mock_result);
    }

    return results;
}

SegmentationResult SimpleModel::segmentInstances(const cv::Mat& frame) {
    SegmentationResult result;

    // 模拟实例分割
    result.detections = detectObjects(frame);

    // 创建简单的分割掩码
    result.mask = cv::Mat::zeros(frame.size(), CV_8UC1);
    for (const auto& detection : result.detections) {
        cv::rectangle(result.mask, detection.bbox, cv::Scalar(255), -1);
    }

    return result;
}

#ifdef USE_ONNXRUNTIME
// ONNXModel 实现
ONNXModel::ONNXModel() {
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "VideoAnalyzer");
}

ONNXModel::~ONNXModel() = default;

bool ONNXModel::initialize(const std::string& model_path) {
    try {
        Ort::SessionOptions session_options;
        session_options.SetIntraOpNumThreads(1);
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), session_options);

        // 获取输入输出信息
        size_t num_input_nodes = session_->GetInputCount();
        size_t num_output_nodes = session_->GetOutputCount();

        Ort::AllocatorWithDefaultOptions allocator;

        for (size_t i = 0; i < num_input_nodes; i++) {
            char* input_name = session_->GetInputName(i, allocator);
            input_names_.push_back(std::string(input_name));
            allocator.Free(input_name);
        }

        for (size_t i = 0; i < num_output_nodes; i++) {
            char* output_name = session_->GetOutputName(i, allocator);
            output_names_.push_back(std::string(output_name));
            allocator.Free(output_name);
        }

        // 加载类别名称（如果有的话）
        class_names_ = {"person", "bicycle", "car", "motorcycle", "airplane",
                       "bus", "train", "truck", "boat", "traffic light"};

        return true;
    } catch (const std::exception& e) {
        std::cerr << "ONNX模型初始化失败: " << e.what() << std::endl;
        return false;
    }
}

std::vector<DetectionResult> ONNXModel::detectObjects(const cv::Mat& frame) {
    std::vector<DetectionResult> results;

    try {
        cv::Mat preprocessed = preprocessImage(frame);

        // 准备输入张量
        std::vector<int64_t> input_shape = {1, 3, 640, 640};
        size_t input_tensor_size = 1 * 3 * 640 * 640;

        std::vector<float> input_tensor_values(input_tensor_size);
        std::memcpy(input_tensor_values.data(), preprocessed.data, input_tensor_size * sizeof(float));

        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_values.data(), input_tensor_size,
            input_shape.data(), input_shape.size());

        // 运行推理
        auto output_tensors = session_->Run(Ort::RunOptions{nullptr},
                                          input_names_.data(), &input_tensor, 1,
                                          output_names_.data(), output_names_.size());

        // 后处理
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape = output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();

        // 这里需要根据具体模型输出格式进行后处理
        // 暂时返回模拟结果
        results = postprocessDetection({}, frame.size());

    } catch (const std::exception& e) {
        std::cerr << "ONNX推理失败: " << e.what() << std::endl;
    }

    return results;
}

SegmentationResult ONNXModel::segmentInstances(const cv::Mat& frame) {
    SegmentationResult result;
    result.detections = detectObjects(frame);

    // 创建分割掩码（这里需要根据具体模型实现）
    result.mask = cv::Mat::zeros(frame.size(), CV_8UC1);

    return result;
}

cv::Mat ONNXModel::preprocessImage(const cv::Mat& frame) {
    cv::Mat resized, normalized;
    cv::resize(frame, resized, cv::Size(640, 640));
    resized.convertTo(normalized, CV_32F, 1.0 / 255.0);

    // 转换为CHW格式
    cv::Mat channels[3];
    cv::split(normalized, channels);

    cv::Mat result(1, 3 * 640 * 640, CV_32F);
    for (int i = 0; i < 3; ++i) {
        std::memcpy(result.data + i * 640 * 640 * sizeof(float),
                   channels[i].data, 640 * 640 * sizeof(float));
    }

    return result;
}

std::vector<DetectionResult> ONNXModel::postprocessDetection(const std::vector<float>& outputs,
                                                           const cv::Size& original_size) {
    std::vector<DetectionResult> results;

    // 模拟后处理结果
    DetectionResult result;
    result.bbox = cv::Rect(100, 100, 200, 200);
    result.confidence = 0.9f;
    result.class_id = 0;
    result.class_name = "person";
    results.push_back(result);

    return results;
}
#endif

// VideoAnalyzer 实现
VideoAnalyzer::VideoAnalyzer() : running_(false), num_workers_(2), webrtc_enabled_(false), rtsp_processing_(false) {
}

VideoAnalyzer::~VideoAnalyzer() {
    stop();
    stopWebRTCStreaming();
    stopRTSPProcessing();
}

bool VideoAnalyzer::initialize(const std::string& config_file) {
    if (!loadConfig(config_file)) {
        return false;
    }

    // 初始化模型
#ifdef USE_ONNXRUNTIME
    detection_model_ = std::make_unique<ONNXModel>();
    segmentation_model_ = std::make_unique<ONNXModel>();

    if (!detection_model_->initialize("models/detection_model.onnx")) {
        std::cout << "ONNX模型加载失败，使用简单模型" << std::endl;
        detection_model_ = std::make_unique<SimpleModel>();
        detection_model_->initialize("models/");
    }

    if (!segmentation_model_->initialize("models/segmentation_model.onnx")) {
        segmentation_model_ = std::make_unique<SimpleModel>();
        segmentation_model_->initialize("models/");
    }
#else
    detection_model_ = std::make_unique<SimpleModel>();
    segmentation_model_ = std::make_unique<SimpleModel>();

    detection_model_->initialize("models/");
    segmentation_model_->initialize("models/");
#endif

    return true;
}

bool VideoAnalyzer::loadConfig(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "无法打开配置文件: " << config_file << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;

    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        std::cerr << "JSON解析错误: " << errs << std::endl;
        return false;
    }

    num_workers_ = root.get("num_workers", 2).asInt();

    // 检查是否启用WebRTC streaming
    webrtc_enabled_ = root.get("webrtc_enabled", false).asBool();
    if (webrtc_enabled_) {
        int webrtc_port = root.get("webrtc_port", 8080).asInt();
        startWebRTCStreaming(webrtc_port);
    }

    return true;
}

void VideoAnalyzer::setResultCallback(std::function<void(const AnalysisResult&)> callback) {
    result_callback_ = callback;
}

bool VideoAnalyzer::processFrame(const cv::Mat& frame, const std::string& source_id,
                                AnalysisType type, int request_id) {
    if (!running_) {
        return false;
    }

    AnalysisRequest request;
    request.frame = frame.clone();
    request.source_id = source_id;
    request.type = type;
    request.request_id = request_id;

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        request_queue_.push(request);
    }
    queue_condition_.notify_one();

    return true;
}

void VideoAnalyzer::start() {
    if (running_) {
        return;
    }

    running_ = true;

    // 启动工作线程
    for (int i = 0; i < num_workers_; ++i) {
        worker_threads_.emplace_back(&VideoAnalyzer::workerLoop, this);
    }
}

void VideoAnalyzer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    queue_condition_.notify_all();

    // 等待所有工作线程结束
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

bool VideoAnalyzer::isRunning() const {
    return running_;
}

void VideoAnalyzer::workerLoop() {
    while (running_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_condition_.wait(lock, [this] { return !request_queue_.empty() || !running_; });

        if (!running_) {
            break;
        }

        if (request_queue_.empty()) {
            continue;
        }

        AnalysisRequest request = request_queue_.front();
        request_queue_.pop();
        lock.unlock();

        processRequest(request);
    }
}

void VideoAnalyzer::processRequest(const AnalysisRequest& request) {
    AnalysisResult result;
    result.source_id = request.source_id;
    result.request_id = request.request_id;
    result.type = request.type;
    result.success = true;

    try {
        cv::Mat processed_frame = request.frame.clone();

        if (request.type == AnalysisType::OBJECT_DETECTION) {
            result.detections = detection_model_->detectObjects(request.frame);

            // 在图像上绘制检测结果
            for (const auto& detection : result.detections) {
                cv::rectangle(processed_frame, detection.bbox, cv::Scalar(0, 255, 0), 2);

                std::string label = detection.class_name + ": " +
                                  std::to_string(static_cast<int>(detection.confidence * 100)) + "%";

                cv::putText(processed_frame, label,
                           cv::Point(detection.bbox.x, detection.bbox.y - 10),
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
            }
        } else if (request.type == AnalysisType::INSTANCE_SEGMENTATION) {
            SegmentationResult seg_result = segmentation_model_->segmentInstances(request.frame);
            result.detections = seg_result.detections;
            result.segmentation_mask = seg_result.mask;

            // 将分割掩码叠加到原图上
            cv::Mat mask_colored;
            cv::applyColorMap(seg_result.mask, mask_colored, cv::COLORMAP_JET);
            cv::addWeighted(processed_frame, 0.7, mask_colored, 0.3, 0, processed_frame);
        }

        result.processed_frame = processed_frame;

        // 推送处理后的帧到WebRTC
        if (webrtc_enabled_) {
            pushFrameToWebRTC(processed_frame, request.source_id);
            std::cout << "推送帧到WebRTC - 源ID: " << request.source_id << std::endl;
        } else {
            std::cout << "WebRTC未启用，跳过帧推送" << std::endl;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.processed_frame = request.frame.clone();
    }

    if (result_callback_) {
        result_callback_(result);
    }
}

// WebRTC相关方法实现
bool VideoAnalyzer::startWebRTCStreaming(int port) {
    if (webrtc_streamer_) {
        return true; // 已经启动
    }

    webrtc_streamer_ = std::make_unique<WebRTCStreamer>();

    if (!webrtc_streamer_->Initialize(port)) {
        std::cerr << "WebRTC streamer初始化失败" << std::endl;
        webrtc_streamer_.reset();
        return false;
    }

    std::cout << "WebRTC streaming已启动，端口: " << port << std::endl;
    return true;
}

void VideoAnalyzer::stopWebRTCStreaming() {
    if (webrtc_streamer_) {
        webrtc_streamer_->Shutdown();
        webrtc_streamer_.reset();
        std::cout << "WebRTC streaming已停止" << std::endl;
    }
}

bool VideoAnalyzer::createWebRTCOffer(const std::string& client_id, std::string& sdp_offer) {
    if (!webrtc_streamer_) {
        return false;
    }
    return webrtc_streamer_->CreateOffer(client_id, sdp_offer);
}

bool VideoAnalyzer::handleWebRTCAnswer(const std::string& client_id, const std::string& sdp_answer) {
    if (!webrtc_streamer_) {
        return false;
    }
    return webrtc_streamer_->HandleAnswer(client_id, sdp_answer);
}

bool VideoAnalyzer::addWebRTCIceCandidate(const std::string& client_id, const Json::Value& candidate) {
    if (!webrtc_streamer_) {
        return false;
    }
    return webrtc_streamer_->AddIceCandidate(client_id, candidate);
}

void VideoAnalyzer::setWebRTCClientConnectedCallback(std::function<void(const std::string&)> callback) {
    if (webrtc_streamer_) {
        webrtc_streamer_->SetOnClientConnected(callback);
    }
}

void VideoAnalyzer::setWebRTCClientDisconnectedCallback(std::function<void(const std::string&)> callback) {
    if (webrtc_streamer_) {
        webrtc_streamer_->SetOnClientDisconnected(callback);
    }
}

void VideoAnalyzer::setWebRTCSignalingCallback(std::function<void(const std::string&, const Json::Value&)> callback) {
    if (webrtc_streamer_) {
        webrtc_streamer_->SetOnSignalingMessage(callback);
    }
}

void VideoAnalyzer::pushFrameToWebRTC(const cv::Mat& frame, const std::string& source_id) {
    if (webrtc_streamer_ && !frame.empty()) {
        webrtc_streamer_->PushFrame(source_id, frame);
    }
}

// RTSP流接收相关方法实现
bool VideoAnalyzer::addRTSPSource(const std::string& source_id, const std::string& rtsp_url, AnalysisType type) {
    std::lock_guard<std::mutex> lock(rtsp_sources_mutex_);

    // 检查是否已存在相同的source_id
    for (const auto& source : rtsp_sources_) {
        if (source->source_id == source_id) {
            std::cerr << "RTSP源已存在: " << source_id << std::endl;
            return false;
        }
    }

    auto rtsp_source = std::make_unique<RTSPSource>();
    rtsp_source->source_id = source_id;
    rtsp_source->rtsp_url = rtsp_url;
    rtsp_source->analysis_type = type;
    rtsp_source->active = false;
    rtsp_source->request_id_counter = 0;

    // 尝试打开RTSP流
    if (!rtsp_source->capture.open(rtsp_url)) {
        std::cerr << "无法打开RTSP流: " << rtsp_url << std::endl;
        return false;
    }

    // 设置缓冲区大小以减少延迟
    rtsp_source->capture.set(cv::CAP_PROP_BUFFERSIZE, 1);

    rtsp_sources_.push_back(std::move(rtsp_source));

    std::cout << "RTSP源已添加: " << source_id << " -> " << rtsp_url << std::endl;
    return true;
}

bool VideoAnalyzer::removeRTSPSource(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(rtsp_sources_mutex_);

    auto it = std::remove_if(rtsp_sources_.begin(), rtsp_sources_.end(),
        [&source_id](const std::unique_ptr<RTSPSource>& source) {
            if (source->source_id == source_id) {
                // 停止处理线程
                source->active = false;
                if (source->processing_thread.joinable()) {
                    source->processing_thread.join();
                }
                // 释放VideoCapture
                source->capture.release();
                std::cout << "RTSP源已移除: " << source_id << std::endl;
                return true;
            }
            return false;
        });

    if (it != rtsp_sources_.end()) {
        rtsp_sources_.erase(it, rtsp_sources_.end());
        return true;
    }

    std::cerr << "未找到RTSP源: " << source_id << std::endl;
    return false;
}

void VideoAnalyzer::startRTSPProcessing() {
    if (rtsp_processing_) {
        std::cout << "RTSP处理已在运行" << std::endl;
        return;
    }

    std::lock_guard<std::mutex> lock(rtsp_sources_mutex_);

    if (rtsp_sources_.empty()) {
        std::cerr << "没有可用的RTSP源" << std::endl;
        return;
    }

    rtsp_processing_ = true;

    // 为每个RTSP源启动处理线程
    for (auto& source : rtsp_sources_) {
        source->active = true;
        source->processing_thread = std::thread(&VideoAnalyzer::rtspProcessingLoop, this, source.get());
    }

    std::cout << "RTSP处理已启动，共 " << rtsp_sources_.size() << " 个源" << std::endl;
}

void VideoAnalyzer::stopRTSPProcessing() {
    if (!rtsp_processing_) {
        return;
    }

    rtsp_processing_ = false;

    std::lock_guard<std::mutex> lock(rtsp_sources_mutex_);

    // 停止所有处理线程
    for (auto& source : rtsp_sources_) {
        source->active = false;
        if (source->processing_thread.joinable()) {
            source->processing_thread.join();
        }
    }

    std::cout << "RTSP处理已停止" << std::endl;
}

bool VideoAnalyzer::isRTSPProcessing() const {
    return rtsp_processing_;
}

void VideoAnalyzer::rtspProcessingLoop(RTSPSource* source) {
    cv::Mat frame;
    int consecutive_failures = 0;
    const int max_failures = 5;

    std::cout << "开始处理RTSP流: " << source->source_id << " (" << source->rtsp_url << ")" << std::endl;

    while (source->active && rtsp_processing_) {
        if (!source->capture.read(frame)) {
            consecutive_failures++;
            std::cerr << "读取RTSP帧失败 (" << consecutive_failures << "/" << max_failures << "): "
                      << source->source_id << std::endl;

            if (consecutive_failures >= max_failures) {
                std::cerr << "连续读取失败，停止处理: " << source->source_id << std::endl;
                break;
            }

            // 等待一段时间后重试
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        consecutive_failures = 0; // 重置失败计数

        if (!frame.empty()) {
            // 处理帧
            bool success = processFrame(frame, source->source_id,
                                      source->analysis_type, source->request_id_counter++);

            if (!success) {
                std::cerr << "帧处理失败: " << source->source_id << std::endl;
            }
        }

        // 控制处理速度，避免CPU过载
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
    }

    std::cout << "RTSP处理线程结束: " << source->source_id << std::endl;
}
#include "VideoAnalyzer.h"
#include "WebRTCStreamer.h"
#include "analysis/ModelRegistry.h"

#include <json/json.h>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <thread>
#include <cmath>

namespace {
    std::function<void(const std::string&)> g_on_client_connected;
    std::function<void(const std::string&)> g_on_client_disconnected;
    std::function<void(const std::string&, const Json::Value&)> g_on_signaling;
}

VideoAnalyzer::VideoAnalyzer()
    : running_(false), num_workers_(2), webrtc_enabled_(false), analysis_enabled_(true), rtsp_processing_(false) {}

VideoAnalyzer::~VideoAnalyzer() { stop(); stopWebRTCStreaming(); stopRTSPProcessing(); }

bool VideoAnalyzer::initialize(const std::string& config_file) {
    if (!loadConfig(config_file)) return false;

    // Prepare default models
    std::string det_path = model_path_mapping_.count("default_detection") ? model_path_mapping_["default_detection"] : std::string();
    std::string seg_path = model_path_mapping_.count("default_segmentation") ? model_path_mapping_["default_segmentation"] : std::string();
    

    // Registry-based creation (OCP): prefer family-specific creators, fallback to default (registered by adapters)
    if (!det_path.empty()) {
        ModelDesc desc; desc.path = det_path; desc.id = det_path; desc.task = AnalysisType::OBJECT_DETECTION;
        if (det_path.find("yolov12") != std::string::npos) desc.family = "yolov12";
        else if (det_path.find("yolo") != std::string::npos) desc.family = "yolo";
        desc.inference_config = inference_config_;  // Apply GPU/CPU configuration
        detection_model_ = ModelRegistry::instance().createDetector(desc);
    }
    if (!detection_model_) {
    }

    if (!segmentation_model_ && !seg_path.empty()) {
        ModelDesc segDesc; segDesc.path = seg_path; segDesc.id = seg_path; segDesc.task = AnalysisType::INSTANCE_SEGMENTATION;
        if (seg_path.find("yolo") != std::string::npos) segDesc.family = "yolo-seg"; // convention key
        segDesc.inference_config = inference_config_;  // Apply GPU/CPU configuration
        segmentation_model_ = ModelRegistry::instance().createSegmenter(segDesc);
    }
    return true;
}

void VideoAnalyzer::setResultCallback(std::function<void(const AnalysisResult&)> callback) { result_callback_ = std::move(callback); }

bool VideoAnalyzer::processFrame(const cv::Mat& frame, const std::string& source_id, AnalysisType type, int request_id) {
    if (frame.empty()) return false;
    AnalysisRequest req; req.frame = frame.clone(); req.source_id = source_id; req.type = type; req.request_id = request_id;
    {
        std::lock_guard<std::mutex> lk(queue_mutex_);
        while (static_cast<int>(request_queue_.size()) >= max_queue_size_) {
            request_queue_.pop(); // drop oldest to bound memory
        }
        request_queue_.push(std::move(req));
    }
    queue_condition_.notify_one();
    return true;
}

void VideoAnalyzer::start() {
    if (running_) return; running_ = true;
    for (int i=0;i<num_workers_;++i) worker_threads_.emplace_back(&VideoAnalyzer::workerLoop, this);
}

void VideoAnalyzer::stop() {
    if (!running_) return; running_ = false; queue_condition_.notify_all();
    for (auto& t : worker_threads_) if (t.joinable()) t.join();
    worker_threads_.clear();
}

bool VideoAnalyzer::isRunning() const { return running_; }

bool VideoAnalyzer::startWebRTCStreaming(int port) {
    if (webrtc_streamer_) return true;
    webrtc_streamer_ = std::make_unique<WebRTCStreamer>();
    if (!webrtc_streamer_->Initialize(port)) { webrtc_streamer_.reset(); return false; }
    if (g_on_client_connected) webrtc_streamer_->SetOnClientConnected(g_on_client_connected);
    if (g_on_client_disconnected) webrtc_streamer_->SetOnClientDisconnected(g_on_client_disconnected);
    if (g_on_signaling) webrtc_streamer_->SetOnSignalingMessage(g_on_signaling);
    std::cout << "WebRTC streaming started on port: " << port << std::endl;
    return true;
}

void VideoAnalyzer::stopWebRTCStreaming() { if (webrtc_streamer_) { webrtc_streamer_->Shutdown(); webrtc_streamer_.reset(); std::cout << "WebRTC streaming stopped" << std::endl; } }

bool VideoAnalyzer::createWebRTCOffer(const std::string& client_id, std::string& sdp_offer) { return webrtc_streamer_ ? webrtc_streamer_->CreateOffer(client_id, sdp_offer) : false; }
bool VideoAnalyzer::handleWebRTCAnswer(const std::string& client_id, const std::string& sdp_answer) { return webrtc_streamer_ ? webrtc_streamer_->HandleAnswer(client_id, sdp_answer) : false; }
bool VideoAnalyzer::addWebRTCIceCandidate(const std::string& client_id, const Json::Value& candidate) { return webrtc_streamer_ ? webrtc_streamer_->AddIceCandidate(client_id, candidate) : false; }

void VideoAnalyzer::setWebRTCClientConnectedCallback(std::function<void(const std::string&)> cb) {
    g_on_client_connected = std::move(cb); if (webrtc_streamer_ && g_on_client_connected) webrtc_streamer_->SetOnClientConnected(g_on_client_connected);
}
void VideoAnalyzer::setWebRTCClientDisconnectedCallback(std::function<void(const std::string&)> cb) {
    g_on_client_disconnected = std::move(cb); if (webrtc_streamer_ && g_on_client_disconnected) webrtc_streamer_->SetOnClientDisconnected(g_on_client_disconnected);
}
void VideoAnalyzer::setWebRTCSignalingCallback(std::function<void(const std::string&, const Json::Value&)> cb) {
    g_on_signaling = std::move(cb); if (webrtc_streamer_ && g_on_signaling) webrtc_streamer_->SetOnSignalingMessage(g_on_signaling);
}

bool VideoAnalyzer::addRTSPSource(const std::string& source_id, const std::string& rtsp_url, AnalysisType type) {
    auto src = std::make_unique<RTSPSource>();
    src->source_id = source_id; src->rtsp_url = rtsp_url; src->analysis_type = type; src->request_id_counter = 0; src->active = false;
    if (!src->capture.open(rtsp_url)) { std::cerr << "Failed to open RTSP: " << rtsp_url << std::endl; return false; }
    src->capture.set(cv::CAP_PROP_BUFFERSIZE, 1);
    {
        std::lock_guard<std::mutex> lk(rtsp_sources_mutex_);
        rtsp_sources_.push_back(std::move(src));
    }
    std::cout << "RTSP source added: " << source_id << " -> " << rtsp_url << std::endl; return true;
}

bool VideoAnalyzer::removeRTSPSource(const std::string& source_id) {
    std::lock_guard<std::mutex> lk(rtsp_sources_mutex_);
    auto it = std::remove_if(rtsp_sources_.begin(), rtsp_sources_.end(), [&](const std::unique_ptr<RTSPSource>& s){ return s->source_id==source_id; });
    if (it == rtsp_sources_.end()) { std::cerr << "RTSP source not found: " << source_id << std::endl; return false; }
    for (auto iter = it; iter != rtsp_sources_.end(); ++iter) { (*iter)->active=false; if ((*iter)->processing_thread.joinable()) (*iter)->processing_thread.join(); (*iter)->capture.release(); }
    rtsp_sources_.erase(it, rtsp_sources_.end());
    std::cout << "RTSP source removed: " << source_id << std::endl; return true;
}

void VideoAnalyzer::startRTSPProcessing() {
    if (rtsp_processing_) { std::cout << "RTSP processing already running" << std::endl; return; }
    std::lock_guard<std::mutex> lk(rtsp_sources_mutex_);
    if (rtsp_sources_.empty()) { std::cerr << "No RTSP sources available" << std::endl; return; }
    rtsp_processing_ = true;
    for (auto& s : rtsp_sources_) { s->active = true; s->processing_thread = std::thread(&VideoAnalyzer::rtspProcessingLoop, this, s.get()); }
    std::cout << "RTSP processing started, sources: " << rtsp_sources_.size() << std::endl;
}

void VideoAnalyzer::stopRTSPProcessing() {
    if (!rtsp_processing_) return; rtsp_processing_ = false;
    std::lock_guard<std::mutex> lk(rtsp_sources_mutex_);
    for (auto& s : rtsp_sources_) { s->active=false; if (s->processing_thread.joinable()) s->processing_thread.join(); }
    std::cout << "RTSP processing stopped" << std::endl;
}

bool VideoAnalyzer::isRTSPProcessing() const { return rtsp_processing_; }

void VideoAnalyzer::pushFrameToWebRTC(const cv::Mat& frame, const std::string& source_id) {
    if (webrtc_streamer_) webrtc_streamer_->PushFrame(source_id, frame);
}

bool VideoAnalyzer::setWebRTCClientSource(const std::string& client_id, const std::string& source_id) {
    if (webrtc_streamer_) {
        return webrtc_streamer_->SetClientSource(client_id, source_id);
    }
    return false;
}

void VideoAnalyzer::setAnalysisEnabled(bool enabled) { analysis_enabled_ = enabled; }
bool VideoAnalyzer::isAnalysisEnabled() const { return analysis_enabled_.load(); }

bool VideoAnalyzer::setCurrentModel(const std::string& model_id, AnalysisType type) {
    std::string path;
    if (model_path_mapping_.count(model_id)) path = model_path_mapping_[model_id];
    if (path.empty()) return false;

    if (type == AnalysisType::OBJECT_DETECTION) {
        ModelDesc desc; desc.id = model_id; desc.path = path; desc.task = AnalysisType::OBJECT_DETECTION;
        if (path.find("yolov12") != std::string::npos) desc.family = "yolov12";
        else if (path.find("yolo") != std::string::npos) desc.family = "yolo";
        auto m = ModelRegistry::instance().createDetector(desc);
        if (!m) return false;
        detection_model_ = std::move(m);
        return true;
    } else if (type == AnalysisType::INSTANCE_SEGMENTATION) {
        ModelDesc desc; desc.id = model_id; desc.path = path; desc.task = AnalysisType::INSTANCE_SEGMENTATION;
        if (path.find("yolo") != std::string::npos) desc.family = "yolo-seg";
        auto m = ModelRegistry::instance().createSegmenter(desc);
        if (!m) return false;
        segmentation_model_ = std::move(m);
        return true;
    }
    return false;
}

void VideoAnalyzer::workerLoop() {
    while (running_) {
        AnalysisRequest req; {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            queue_condition_.wait(lk, [&]{ return !request_queue_.empty() || !running_; });
            if (!running_) break; if (request_queue_.empty()) continue; req = std::move(request_queue_.front()); request_queue_.pop();
        }
        processRequest(req);
    }
}

void VideoAnalyzer::processRequest(const AnalysisRequest& req) {
    AnalysisResult res; res.source_id = req.source_id; res.request_id = req.request_id; res.type = req.type; res.success = true;
    try {
        cv::Mat processed = req.frame.clone();
        const bool do_analysis = analysis_enabled_.load();
        if (do_analysis) {
            std::shared_lock<std::shared_mutex> lk(model_mutex_);
            if (req.type == AnalysisType::OBJECT_DETECTION && detection_model_) {
                res.detections = detection_model_->detectObjects(req.frame);
            } else if (req.type == AnalysisType::INSTANCE_SEGMENTATION && segmentation_model_) {
                SegmentationResult s = segmentation_model_->segmentInstances(req.frame);
                res.detections = std::move(s.detections);
                res.segmentation_mask = std::move(s.mask);
            }
            for (const auto& d : res.detections) {
                cv::rectangle(processed, d.bbox, cv::Scalar(0, 255, 0), 2);
                std::string label = d.class_name + ": " + std::to_string(static_cast<int>(d.confidence * 100)) + "%";
                cv::putText(processed, label, cv::Point(d.bbox.x, std::max(0, d.bbox.y - 8)), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
            }
            if (!res.segmentation_mask.empty()) {
                cv::Mat color; cv::applyColorMap(res.segmentation_mask, color, cv::COLORMAP_JET);
                cv::addWeighted(processed, 0.7, color, 0.3, 0, processed);
            }
        }
        res.processed_frame = processed.clone();
        // Always push processed frame (with or without detections)
        pushFrameToWebRTC(processed, req.source_id);
    } catch (const std::exception& e) { res.success = false; res.error_message = e.what(); }
    if (result_callback_) result_callback_(res);
}

void VideoAnalyzer::rtspProcessingLoop(RTSPSource* s) {
    cv::Mat frame; int request_id = 0; int consecutive_failures = 0; const int max_failures = 15;
    while (s->active && rtsp_processing_) {
        if (!s->capture.read(frame)) {
            if (++consecutive_failures >= max_failures) break; std::this_thread::sleep_for(std::chrono::milliseconds(500)); continue;
        }
        consecutive_failures = 0;
        if (!frame.empty()) { processFrame(frame, s->source_id, s->analysis_type, request_id++); }
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

bool VideoAnalyzer::loadConfig(const std::string& config_file) {
    try {
        std::ifstream f(config_file); if (!f.is_open()) { std::cerr << "Cannot open config: " << config_file << std::endl; return false; }
        Json::Value root; Json::CharReaderBuilder b; std::string errs; if (!Json::parseFromStream(b, f, &root, &errs)) { std::cerr << "JSON parse error: " << errs << std::endl; return false; }
        num_workers_ = root.get("num_workers", 2).asInt();
        webrtc_enabled_ = root.get("webrtc_enabled", false).asBool(); if (webrtc_enabled_) { int port = root.get("webrtc_port", 8080).asInt(); startWebRTCStreaming(port); }

        if (root.isMember("models") && root["models"].isObject()) {
            const auto& models = root["models"];
            if (models.isMember("detection") && models["detection"].isObject()) {
                const auto& det = models["detection"]; std::string p = det.get("path", "").asString(); if (!p.empty()) model_path_mapping_["default_detection"] = p;
            }
            if (models.isMember("segmentation") && models["segmentation"].isObject()) {
                const auto& seg = models["segmentation"]; std::string p = seg.get("path", "").asString(); if (!p.empty()) model_path_mapping_["default_segmentation"] = p;
            }
            
        }
        // performance settings
        if (root.isMember("performance") && root["performance"].isObject()) {
            const auto& perf = root["performance"];
            max_queue_size_ = std::max(1, perf.get("max_queue_size", 5).asInt());
            int target_fps = perf.get("target_fps", 30).asInt();
            if (webrtc_streamer_) webrtc_streamer_->SetFrameRate(target_fps);
        }

        // inference settings
        if (root.isMember("inference") && root["inference"].isObject()) {
            const auto& inf = root["inference"];

            std::string device_str = inf.get("device", "auto").asString();
            if (device_str == "cuda") {
                inference_config_.device = InferenceDevice::CUDA;
            } else if (device_str == "cpu") {
                inference_config_.device = InferenceDevice::CPU;
            } else {
                // auto: detect best available device
                auto devices = OnnxRuntimeBackend::getAvailableDevices();
                // Prefer GPU over CPU
                if (devices.size() > 1) {
                    inference_config_.device = devices[devices.size() - 1];  // Last device (usually GPU)
                } else {
                    inference_config_.device = InferenceDevice::CPU;
                }
            }

            // CUDA specific settings
            if (inf.isMember("cuda")) {
                const auto& cuda = inf["cuda"];
                inference_config_.cuda_device_id = cuda.get("device_id", 0).asInt();
                inference_config_.gpu_mem_limit = static_cast<size_t>(cuda.get("gpu_mem_limit", Json::Value::UInt64(2147483648)).asUInt64());
            }

            // CPU specific settings
            if (inf.isMember("cpu")) {
                const auto& cpu = inf["cpu"];
                inference_config_.num_threads = cpu.get("num_threads", 4).asInt();
                inference_config_.use_arena = cpu.get("use_arena", true).asBool();
            }

            inference_config_.enable_profiling = inf.get("enable_profiling", false).asBool();

            std::cout << "📝 Inference configuration loaded:" << std::endl;
            std::cout << "   Device: " << OnnxRuntimeBackend::deviceToString(inference_config_.device) << std::endl;
        }

        return true;
    } catch (const std::exception& e) { std::cerr << "loadConfig failed: " << e.what() << std::endl; return false; }
}

std::vector<std::string> VideoAnalyzer::getRTSPSourceIds() const {
    std::lock_guard<std::mutex> lock(rtsp_sources_mutex_);
    std::vector<std::string> source_ids;
    for (const auto& source : rtsp_sources_) {
        if (source) {
            source_ids.push_back(source->source_id);
        }
    }
    return source_ids;
}

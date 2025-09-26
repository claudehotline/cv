#pragma once

#include <opencv2/opencv.hpp>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>

#ifdef USE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

#include "WebRTCStreamer.h"

enum class AnalysisType {
    OBJECT_DETECTION,
    INSTANCE_SEGMENTATION
};

struct DetectionResult {
    cv::Rect bbox;
    float confidence;
    int class_id;
    std::string class_name;
};

struct SegmentationResult {
    std::vector<DetectionResult> detections;
    cv::Mat mask;
};

struct AnalysisRequest {
    cv::Mat frame;
    std::string source_id;
    AnalysisType type;
    int request_id;
};

struct AnalysisResult {
    std::string source_id;
    int request_id;
    AnalysisType type;
    cv::Mat processed_frame;
    std::vector<DetectionResult> detections;
    cv::Mat segmentation_mask;
    bool success;
    std::string error_message;
};

class AIModel {
public:
    virtual ~AIModel() = default;
    virtual bool initialize(const std::string& model_path) = 0;
    virtual std::vector<DetectionResult> detectObjects(const cv::Mat& frame) = 0;
    virtual SegmentationResult segmentInstances(const cv::Mat& frame) = 0;
};

#ifdef USE_ONNXRUNTIME
class ONNXModel : public AIModel {
public:
    ONNXModel();
    ~ONNXModel() override;

    bool initialize(const std::string& model_path) override;
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override;
    SegmentationResult segmentInstances(const cv::Mat& frame) override;

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<std::string> class_names_;

    cv::Mat preprocessImage(const cv::Mat& frame);
    std::vector<DetectionResult> postprocessDetection(const std::vector<float>& outputs,
                                                     const cv::Size& original_size);
};
#endif

class SimpleModel : public AIModel {
public:
    bool initialize(const std::string& model_path) override;
    std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override;
    SegmentationResult segmentInstances(const cv::Mat& frame) override;

private:
    cv::CascadeClassifier face_cascade_;
    bool model_loaded_;
};

class VideoAnalyzer {
public:
    VideoAnalyzer();
    ~VideoAnalyzer();

    bool initialize(const std::string& config_file);

    void setResultCallback(std::function<void(const AnalysisResult&)> callback);

    bool processFrame(const cv::Mat& frame, const std::string& source_id,
                     AnalysisType type, int request_id);

    void start();
    void stop();

    bool isRunning() const;

    // WebRTC相关方法
    bool startWebRTCStreaming(int port = 8080);
    void stopWebRTCStreaming();
    bool createWebRTCOffer(const std::string& client_id, std::string& sdp_offer);
    bool handleWebRTCAnswer(const std::string& client_id, const std::string& sdp_answer);
    bool addWebRTCIceCandidate(const std::string& client_id, const Json::Value& candidate);

    // 设置WebRTC事件回调
    void setWebRTCClientConnectedCallback(std::function<void(const std::string&)> callback);
    void setWebRTCClientDisconnectedCallback(std::function<void(const std::string&)> callback);
    void setWebRTCSignalingCallback(std::function<void(const std::string&, const Json::Value&)> callback);

    // RTSP流接收相关方法
    bool addRTSPSource(const std::string& source_id, const std::string& rtsp_url, AnalysisType type = AnalysisType::OBJECT_DETECTION);
    bool removeRTSPSource(const std::string& source_id);
    void startRTSPProcessing();
    void stopRTSPProcessing();
    bool isRTSPProcessing() const;

    // 公开给测试视频生成器使用
    void pushFrameToWebRTC(const cv::Mat& frame, const std::string& source_id);

private:
    std::unique_ptr<AIModel> detection_model_;
    std::unique_ptr<AIModel> segmentation_model_;

    std::function<void(const AnalysisResult&)> result_callback_;

    std::queue<AnalysisRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;

    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    int num_workers_;

    // WebRTC streaming
    std::unique_ptr<WebRTCStreamer> webrtc_streamer_;
    bool webrtc_enabled_;

    // RTSP流接收相关
    struct RTSPSource {
        std::string source_id;
        std::string rtsp_url;
        AnalysisType analysis_type;
        cv::VideoCapture capture;
        std::thread processing_thread;
        std::atomic<bool> active;
        int request_id_counter;
    };

    std::vector<std::unique_ptr<RTSPSource>> rtsp_sources_;
    std::mutex rtsp_sources_mutex_;
    std::atomic<bool> rtsp_processing_;

    void workerLoop();
    void processRequest(const AnalysisRequest& request);

    // RTSP处理相关私有方法
    void rtspProcessingLoop(RTSPSource* source);

    bool loadConfig(const std::string& config_file);
};
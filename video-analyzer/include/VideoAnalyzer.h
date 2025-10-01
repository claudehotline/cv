#pragma once

#include <opencv2/opencv.hpp>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <vector>

#include "WebRTCStreamer.h"
#include "analysis/Types.h"
#include "analysis/Interfaces.h"
#include "analysis/OnnxRuntimeBackend.h"
#include "orchestration/TrackManager.h"
#include "ConfigLoader.h"

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

struct ModelConfig {
    std::string id;
    std::string name;
    std::string type;
    std::string path;
    float confidence_threshold {0.0f};
    float nms_threshold {0.0f};
    int input_width {0};
    int input_height {0};
};

struct StreamContextInfo {
    std::string stream;
    std::string profile;
    std::string source_url;
    std::string model_id;
    AnalysisType task { AnalysisType::OBJECT_DETECTION };
    int ref_count {0};
};

// Compatibility base that conforms to the new interfaces
class AIModel : public IDetectionModel, public ISegmentationModel {
public:
    ~AIModel() override = default;
    virtual bool initialize(const std::string& model_path) override = 0;
    virtual std::vector<DetectionResult> detectObjects(const cv::Mat& frame) override = 0;
    virtual SegmentationResult segmentInstances(const cv::Mat& frame) override = 0;
};

// No concrete model implementations declared here; models live under models/ and adapters.

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

    // WebRTC related
    bool startWebRTCStreaming(int port = 8080);
    void stopWebRTCStreaming();
    bool createWebRTCOffer(const std::string& client_id, std::string& sdp_offer);
    bool handleWebRTCAnswer(const std::string& client_id, const std::string& sdp_answer);
    bool addWebRTCIceCandidate(const std::string& client_id, const Json::Value& candidate);
    bool setWebRTCClientSource(const std::string& client_id, const std::string& source_id);

    // WebRTC event callbacks
    void setWebRTCClientConnectedCallback(std::function<void(const std::string&)> callback);
    void setWebRTCClientDisconnectedCallback(std::function<void(const std::string&)> callback);
    void setWebRTCSignalingCallback(std::function<void(const std::string&, const Json::Value&)> callback);

    // RTSP source management
    bool addRTSPSource(const std::string& source_id, const std::string& rtsp_url,
                       AnalysisType type = AnalysisType::OBJECT_DETECTION);
    bool removeRTSPSource(const std::string& source_id);
    void startRTSPProcessing();
    void stopRTSPProcessing();
    bool isRTSPProcessing() const;

    // For test video generator (if used externally)
    void pushFrameToWebRTC(const cv::Mat& frame, const std::string& source_id);

    // Analysis control
    void setAnalysisEnabled(bool enabled);
    bool isAnalysisEnabled() const;

    // Hot switch models
    bool setCurrentModel(const std::string& model_id, AnalysisType type);

    // Subscription and switching (per stream/profile) - skeleton for TrackManager
    bool subscribeStream(const std::string& stream, const std::string& profile,
                         const std::string& url = std::string(),
                         const std::string& model_id = std::string(),
                         AnalysisType task = AnalysisType::OBJECT_DETECTION);
    void unsubscribeStream(const std::string& stream, const std::string& profile);
    bool switchSourceFor(const std::string& stream, const std::string& profile, const std::string& new_url);
    bool switchModelFor(const std::string& stream, const std::string& profile, const std::string& model_id);
    bool switchTaskFor(const std::string& stream, const std::string& profile, AnalysisType task);

    // Inference device control (engine set)
    void setInferenceDevice(InferenceDevice dev, int cuda_device_id = 0);

    std::optional<StreamContextInfo> getStreamContext(const std::string& stream, const std::string& profile) const;

    // Get list of RTSP sources
    std::vector<std::string> getRTSPSourceIds() const;

    const std::vector<ModelConfig>& getDetectionModels() const { return detection_models_; }
    const std::string& getCurrentDetectionModelId() const { return current_detection_model_id_; }

private:
    std::shared_ptr<IDetectionModel> detection_model_;
    std::shared_ptr<ISegmentationModel> segmentation_model_;

    std::map<std::string, std::string> model_path_mapping_;
    std::vector<ModelConfig> detection_models_;
    std::string current_detection_model_id_;
    mutable std::shared_mutex model_mutex_;

    std::vector<ProfileEntry> profile_configs_;

    std::function<void(const AnalysisResult&)> result_callback_;

    std::queue<AnalysisRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;

    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_;
    int num_workers_ {2};
    int max_queue_size_ {5};

    // WebRTC streaming
    std::unique_ptr<WebRTCStreamer> webrtc_streamer_;
    bool webrtc_enabled_ {false};

    // Analysis enable switch
    std::atomic<bool> analysis_enabled_;

    // RTSP source holder
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
    mutable std::mutex rtsp_sources_mutex_;
    std::atomic<bool> rtsp_processing_;

    // Inference configuration
    InferenceConfig inference_config_;

    // Track manager for per-(stream,profile) pipelines
    TrackManager track_manager_;

    std::optional<ProfileEntry> findProfile(const std::string& profile) const;
    AnalysisType parseTask(const std::string& task) const;

    void workerLoop();
    void processRequest(const AnalysisRequest& request);

    // RTSP
    void rtspProcessingLoop(RTSPSource* source);

    bool loadConfig(const std::string& config_file);
};

#pragma once

#include <opencv2/opencv.hpp>
#include <rtc/rtc.hpp>
#include <json/json.h>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <string>
#include <map>
#include <chrono>

// FFmpeg forward declarations removed - using simplified encoding

// 基于libdatachannel的WebRTC视频源
class WebRTCVideoSource {
public:
    WebRTCVideoSource();
    ~WebRTCVideoSource();

    // 推送视频帧到特定源
    void PushFrame(const std::string& source_id, const cv::Mat& frame);
    void SetFrameRate(int fps) { target_fps_ = fps; }

    // 获取特定源的编码后数据
    std::vector<uint8_t> GetEncodedFrame(const std::string& source_id);
    bool HasEncodedFrame(const std::string& source_id) const;

private:
    int target_fps_;

    // 为每个视频源维护独立的帧队列
    struct SourceData {
        std::queue<std::vector<uint8_t>> encoded_frames;
        std::chrono::steady_clock::time_point last_frame_time;
        int frame_width = 0;
        int frame_height = 0;
        bool encoder_initialized = false;
    };

    mutable std::mutex sources_mutex_;
    std::map<std::string, SourceData> video_sources_;

    // 简化版编码器相关
    std::vector<uint8_t> EncodeFrame(const cv::Mat& frame);
};

class WebRTCStreamer {
public:
    WebRTCStreamer();
    ~WebRTCStreamer();

    bool Initialize(int port = 8080);
    void Shutdown();

    // WebRTC连接管理 - 基于libdatachannel
    bool CreateOffer(const std::string& client_id, std::string& sdp_offer);
    bool HandleAnswer(const std::string& client_id, const std::string& sdp_answer);
    bool AddIceCandidate(const std::string& client_id, const Json::Value& candidate);

    // 客户端源选择
    bool SetClientSource(const std::string& client_id, const std::string& source_id);
    std::string GetClientSource(const std::string& client_id) const;

    // 视频流推送
    void PushFrame(const std::string& source_id, const cv::Mat& frame);
    void SetFrameRate(int fps);

    // 事件回调
    void SetOnClientConnected(std::function<void(const std::string&)> callback);
    void SetOnClientDisconnected(std::function<void(const std::string&)> callback);
    void SetOnSignalingMessage(std::function<void(const std::string&, const Json::Value&)> callback);

    // 获取统计信息
    struct StreamStats {
        int connected_clients = 0;
        int frames_sent = 0;
        double avg_fps = 0.0;
        int64_t bytes_sent = 0;
    };
    StreamStats GetStats() const;

private:
    bool initialized_;
    int port_;

    // libdatachannel配置
    rtc::Configuration rtc_config_;

    // 连接管理 - 使用libdatachannel
    struct ClientConnection {
        std::string client_id;
        std::string requested_source = "camera_01";  // 客户端请求的视频源，默认camera_01
        std::shared_ptr<rtc::PeerConnection> peer_connection;
        std::shared_ptr<rtc::Track> video_track;
        std::shared_ptr<rtc::DataChannel> data_channel;
        bool connected;
        std::chrono::steady_clock::time_point connect_time;
    };

    std::map<std::string, std::unique_ptr<ClientConnection>> clients_;
    mutable std::mutex clients_mutex_;

    // 视频源
    std::unique_ptr<WebRTCVideoSource> video_source_;

    // 统计信息
    mutable std::mutex stats_mutex_;
    StreamStats stats_;

    // 回调函数
    std::function<void(const std::string&)> on_client_connected_;
    std::function<void(const std::string&)> on_client_disconnected_;
    std::function<void(const std::string&, const Json::Value&)> on_signaling_message_;

    // 内部方法
    void InitializeLibDataChannel();
    std::shared_ptr<rtc::PeerConnection> CreatePeerConnection(const std::string& client_id);
    void HandleConnectionStateChange(const std::string& client_id, rtc::PeerConnection::State state);
    void UpdateStats();
    void SendVideoFrames();

    // 视频帧发送线程
    std::thread video_sender_thread_;
    std::atomic<bool> should_stop_sender_;
};
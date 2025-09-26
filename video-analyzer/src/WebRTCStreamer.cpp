#include "WebRTCStreamer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

// FFmpeg headers temporarily disabled - using simplified encoding
// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavutil/opt.h>
// #include <libswscale/swscale.h>
// }

// WebRTCVideoSource implementation - 简化版本编码器
WebRTCVideoSource::WebRTCVideoSource()
    : target_fps_(30), encoder_initialized_(false), frame_width_(0), frame_height_(0) {
    last_frame_time_ = std::chrono::steady_clock::now();
    frame_counter_ = 0;
}

WebRTCVideoSource::~WebRTCVideoSource() {
    // Cleanup simplified
}

void WebRTCVideoSource::PushFrame(const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }

    // 帧率控制
    auto now = std::chrono::steady_clock::now();
    auto frame_interval = std::chrono::milliseconds(1000 / target_fps_);

    if (now - last_frame_time_ < frame_interval) {
        return; // Skip frame to maintain target FPS
    }

    last_frame_time_ = now;

    // 初始化编码器（如果需要）
    if (!encoder_initialized_ || frame.cols != frame_width_ || frame.rows != frame_height_) {
        InitializeEncoder(frame.cols, frame.rows);
    }

    // 编码帧
    std::vector<uint8_t> encoded_frame = EncodeFrame(frame);

    if (!encoded_frame.empty()) {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        encoded_frames_.push(std::move(encoded_frame));

        // 限制队列大小
        while (encoded_frames_.size() > 10) {
            encoded_frames_.pop();
        }
    }
}

std::vector<uint8_t> WebRTCVideoSource::GetEncodedFrame() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (encoded_frames_.empty()) {
        return {};
    }

    std::vector<uint8_t> frame = std::move(encoded_frames_.front());
    encoded_frames_.pop();
    return frame;
}

bool WebRTCVideoSource::HasEncodedFrame() const {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    return !encoded_frames_.empty();
}

void WebRTCVideoSource::InitializeEncoder(int width, int height) {
    frame_width_ = width;
    frame_height_ = height;
    encoder_initialized_ = true;
    frame_counter_ = 0;

    std::cout << "Simplified video encoder initialized for " << width << "x" << height << std::endl;
}

std::vector<uint8_t> WebRTCVideoSource::EncodeFrame(const cv::Mat& frame) {
    if (!encoder_initialized_) {
        return {};
    }

    // 调整帧大小以优化传输（如果原始帧太大）
    cv::Mat resized_frame = frame;
    if (frame.cols > 640 || frame.rows > 480) {
        // 保持宽高比，最大640x480
        double scale = std::min(640.0 / frame.cols, 480.0 / frame.rows);
        int new_width = static_cast<int>(frame.cols * scale);
        int new_height = static_cast<int>(frame.rows * scale);
        cv::resize(frame, resized_frame, cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);
    }

    // 使用JPEG编码，通过数据通道发送
    std::vector<uint8_t> encoded_data;
    std::vector<int> compression_params = {
        cv::IMWRITE_JPEG_QUALITY, 75,  // 提高质量到75
        cv::IMWRITE_JPEG_OPTIMIZE, 1   // 启用优化
    };

    if (cv::imencode(".jpg", resized_frame, encoded_data, compression_params)) {
        frame_counter_++;
        return encoded_data;
    }

    return {};
}

void WebRTCVideoSource::CleanupEncoder() {
    encoder_initialized_ = false;
    frame_counter_ = 0;
}

// WebRTCStreamer implementation - 基于libdatachannel
WebRTCStreamer::WebRTCStreamer()
    : initialized_(false), port_(0), should_stop_sender_(false) {
    video_source_ = std::make_unique<WebRTCVideoSource>();
    stats_ = {};
}

WebRTCStreamer::~WebRTCStreamer() {
    Shutdown();
}

bool WebRTCStreamer::Initialize(int port) {
    if (initialized_) {
        return true;
    }

    try {
        port_ = port;
        InitializeLibDataChannel();

        // 启动视频帧发送线程
        should_stop_sender_ = false;
        video_sender_thread_ = std::thread(&WebRTCStreamer::SendVideoFrames, this);

        initialized_ = true;
        std::cout << "WebRTC Streamer initialized with libdatachannel on port " << port << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "WebRTC Streamer initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void WebRTCStreamer::InitializeLibDataChannel() {
    // 配置libdatachannel - 纯本地连接，不使用STUN/TURN
    // 不添加任何ICE服务器，libdatachannel会自动生成host候选
    rtc_config_.enableIceTcp = false;  // 只用UDP
    rtc_config_.disableAutoNegotiation = false;
    rtc_config_.portRangeBegin = 10000;  // 指定端口范围
    rtc_config_.portRangeEnd = 10100;

    // 绑定到localhost避免多网卡问题
    rtc_config_.bindAddress = "127.0.0.1";

    std::cout << "libdatachannel configuration initialized (localhost only, no STUN)" << std::endl;
}

void WebRTCStreamer::Shutdown() {
    if (!initialized_) {
        return;
    }

    // 停止视频发送线程
    should_stop_sender_ = true;
    if (video_sender_thread_.joinable()) {
        video_sender_thread_.join();
    }

    // 清理所有连接
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.clear();

    initialized_ = false;
    std::cout << "WebRTC Streamer shut down" << std::endl;
}

bool WebRTCStreamer::CreateOffer(const std::string& client_id, std::string& sdp_offer) {
    if (!initialized_) {
        return false;
    }

    try {
        // 局部变量存储回调信息
        Json::Value offer_message;
        bool should_send_callback = false;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);

            // 创建新的PeerConnection
            auto peer_connection = CreatePeerConnection(client_id);
            if (!peer_connection) {
                return false;
            }

            // 创建客户端连接
            auto client = std::make_unique<ClientConnection>();
            client->client_id = client_id;
            client->peer_connection = peer_connection;
            client->connected = false;
            client->connect_time = std::chrono::steady_clock::now();

            // 创建数据通道来发送视频帧（JPEG格式）
            // 使用数据通道比视频轨道更简单，不需要复杂的H.264编码
            auto data_channel = peer_connection->createDataChannel("video");
            client->data_channel = data_channel;

            // 设置数据通道回调
            data_channel->onOpen([client_id, this]() {
                std::cout << "数据通道已打开: " << client_id << std::endl;
            });

            data_channel->onClosed([client_id]() {
                std::cout << "数据通道已关闭: " << client_id << std::endl;
            });

            clients_[client_id] = std::move(client);

            // 使用libdatachannel的createOffer方法
            auto offer_desc = peer_connection->createOffer();

            // 设置本地描述
            peer_connection->setLocalDescription(rtc::Description::Type::Offer);

            // 获取生成的SDP offer
            sdp_offer = std::string(offer_desc);

            std::cout << "Created offer for client: " << client_id << std::endl;

            // 准备回调消息（在锁内）
            if (on_signaling_message_) {
                offer_message["type"] = "offer";
                offer_message["data"]["type"] = "offer";
                offer_message["data"]["sdp"] = sdp_offer;
                offer_message["timestamp"] = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                should_send_callback = true;
            }
        } // 锁在这里释放

        // 在锁外发送回调，避免死锁
        if (should_send_callback && on_signaling_message_) {
            on_signaling_message_(client_id, offer_message);
            std::cout << "已发送WebRTC offer到客户端: " << client_id << std::endl;
        }

        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to create offer for " << client_id << ": " << e.what() << std::endl;
        return false;
    }
}

bool WebRTCStreamer::HandleAnswer(const std::string& client_id, const std::string& sdp_answer) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto client_it = clients_.find(client_id);
    if (client_it == clients_.end()) {
        std::cerr << "Client not found: " << client_id << std::endl;
        return false;
    }

    try {
        // 设置远程描述
        rtc::Description answer(sdp_answer, rtc::Description::Type::Answer);
        client_it->second->peer_connection->setRemoteDescription(answer);

        std::cout << "Set answer for client: " << client_id << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to set answer for " << client_id << ": " << e.what() << std::endl;
        return false;
    }
}

bool WebRTCStreamer::AddIceCandidate(const std::string& client_id, const Json::Value& candidate) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto client_it = clients_.find(client_id);
    if (client_it == clients_.end()) {
        std::cerr << "客户端未找到: " << client_id << std::endl;
        return false;
    }

    try {
        std::string candidate_str = candidate["candidate"].asString();
        std::string mid = candidate["sdpMid"].asString();

        std::cout << "收到远程ICE候选: " << candidate_str << std::endl;

        rtc::Candidate rtc_candidate(candidate_str, mid);
        client_it->second->peer_connection->addRemoteCandidate(rtc_candidate);

        std::cout << "成功添加ICE候选 for client: " << client_id << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to add ICE candidate for " << client_id << ": " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<rtc::PeerConnection> WebRTCStreamer::CreatePeerConnection(const std::string& client_id) {
    try {
        auto peer_connection = std::make_shared<rtc::PeerConnection>(rtc_config_);

        // 设置状态变化回调
        peer_connection->onStateChange([this, client_id](rtc::PeerConnection::State state) {
            HandleConnectionStateChange(client_id, state);
        });

        // 设置ICE候选回调
        peer_connection->onLocalCandidate([this, client_id](rtc::Candidate candidate) {
            std::string cand_str = std::string(candidate);

            // 打印详细的ICE候选信息用于调试
            std::cout << "生成本地ICE候选: " << cand_str << std::endl;

            if (on_signaling_message_) {
                Json::Value ice_candidate;
                ice_candidate["candidate"] = cand_str;
                ice_candidate["sdpMid"] = candidate.mid();

                Json::Value message;
                message["type"] = "ice_candidate";
                message["client_id"] = client_id;
                message["data"] = ice_candidate;
                on_signaling_message_(client_id, message);
            }
        });

        std::cout << "Created peer connection for client: " << client_id << std::endl;
        return peer_connection;

    } catch (const std::exception& e) {
        std::cerr << "Failed to create peer connection for " << client_id << ": " << e.what() << std::endl;
        return nullptr;
    }
}

void WebRTCStreamer::HandleConnectionStateChange(const std::string& client_id, rtc::PeerConnection::State state) {
    bool connected = (state == rtc::PeerConnection::State::Connected);

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto client_it = clients_.find(client_id);
        if (client_it != clients_.end()) {
            client_it->second->connected = connected;
        }
    }

    // 添加状态名称的输出
    const char* state_name = "Unknown";
    switch(state) {
        case rtc::PeerConnection::State::New: state_name = "New"; break;
        case rtc::PeerConnection::State::Connecting: state_name = "Connecting"; break;
        case rtc::PeerConnection::State::Connected: state_name = "Connected"; break;
        case rtc::PeerConnection::State::Disconnected: state_name = "Disconnected"; break;
        case rtc::PeerConnection::State::Failed: state_name = "Failed"; break;
        case rtc::PeerConnection::State::Closed: state_name = "Closed"; break;
    }

    std::cout << "Connection state changed for " << client_id << ": " << state_name << " (" << (int)state << ")" << std::endl;

    if (connected && on_client_connected_) {
        on_client_connected_(client_id);
    } else if (!connected && on_client_disconnected_) {
        on_client_disconnected_(client_id);
    }
}

void WebRTCStreamer::PushFrame(const std::string& source_id, const cv::Mat& frame) {
    if (video_source_) {
        std::cout << "WebRTCStreamer::PushFrame - 推送帧，源ID: " << source_id << ", 帧大小: " << frame.cols << "x" << frame.rows << std::endl;
        video_source_->PushFrame(frame);

        // 更新统计信息
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_sent++;
        UpdateStats();
    }
}

void WebRTCStreamer::SetFrameRate(int fps) {
    if (video_source_) {
        video_source_->SetFrameRate(fps);
    }
}

void WebRTCStreamer::SendVideoFrames() {
    int frames_sent_count = 0;
    const size_t MAX_CHUNK_SIZE = 16384; // 16KB chunks for reliable transmission

    while (!should_stop_sender_) {
        if (video_source_ && video_source_->HasEncodedFrame()) {
            auto encoded_frame = video_source_->GetEncodedFrame();

            if (frames_sent_count % 30 == 0) {  // 每30帧记录一次
                std::cout << "发送视频帧 #" << frames_sent_count << ", 大小: " << encoded_frame.size() << " bytes" << std::endl;
            }
            frames_sent_count++;

            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (auto& [client_id, client] : clients_) {
                if (client->connected && client->data_channel && client->data_channel->isOpen()) {
                    try {
                        // 发送JPEG图像通过数据通道
                        // 如果数据较大，分块发送以提高可靠性
                        uint32_t total_size = static_cast<uint32_t>(encoded_frame.size());

                        if (total_size <= MAX_CHUNK_SIZE - 4) {
                            // 小数据，一次发送：格式：前4字节为大小，后面是JPEG数据
                            rtc::binary message;
                            message.reserve(4 + encoded_frame.size());

                            // 添加大小头（大端序）
                            message.push_back(static_cast<std::byte>((total_size >> 24) & 0xFF));
                            message.push_back(static_cast<std::byte>((total_size >> 16) & 0xFF));
                            message.push_back(static_cast<std::byte>((total_size >> 8) & 0xFF));
                            message.push_back(static_cast<std::byte>(total_size & 0xFF));

                            // 添加JPEG数据
                            for (uint8_t byte : encoded_frame) {
                                message.push_back(static_cast<std::byte>(byte));
                            }

                            // 通过数据通道发送
                            client->data_channel->send(message);
                        } else {
                            // 大数据，分块发送
                            // 首先发送头部信息（帧总大小）
                            rtc::binary header;
                            header.reserve(4);
                            header.push_back(static_cast<std::byte>((total_size >> 24) & 0xFF));
                            header.push_back(static_cast<std::byte>((total_size >> 16) & 0xFF));
                            header.push_back(static_cast<std::byte>((total_size >> 8) & 0xFF));
                            header.push_back(static_cast<std::byte>(total_size & 0xFF));
                            client->data_channel->send(header);

                            // 然后分块发送数据
                            size_t offset = 0;
                            while (offset < encoded_frame.size()) {
                                size_t chunk_size = std::min(MAX_CHUNK_SIZE, encoded_frame.size() - offset);

                                rtc::binary chunk;
                                chunk.reserve(chunk_size);
                                for (size_t i = 0; i < chunk_size; ++i) {
                                    chunk.push_back(static_cast<std::byte>(encoded_frame[offset + i]));
                                }

                                client->data_channel->send(chunk);
                                offset += chunk_size;

                                // 小延迟避免数据通道拥塞
                                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                            }
                        }

                        // Update statistics
                        stats_.bytes_sent += encoded_frame.size();
                    } catch (const std::exception& e) {
                        std::cerr << "Failed to send frame to " << client_id << ": " << e.what() << std::endl;
                    }
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS to reduce load
    }
}

void WebRTCStreamer::UpdateStats() {
    stats_.connected_clients = 0;
    for (const auto& [client_id, client] : clients_) {
        if (client->connected) {
            stats_.connected_clients++;
        }
    }
}

WebRTCStreamer::StreamStats WebRTCStreamer::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void WebRTCStreamer::SetOnClientConnected(std::function<void(const std::string&)> callback) {
    on_client_connected_ = callback;
}

void WebRTCStreamer::SetOnClientDisconnected(std::function<void(const std::string&)> callback) {
    on_client_disconnected_ = callback;
}

void WebRTCStreamer::SetOnSignalingMessage(std::function<void(const std::string&, const Json::Value&)> callback) {
    on_signaling_message_ = callback;
}
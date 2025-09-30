#include "WebRTCStreamer.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace {
    constexpr bool kVerboseLogging = false;
}

// FFmpeg headers temporarily disabled - using simplified encoding
// extern "C" {
// #include <libavcodec/avcodec.h>
// #include <libavformat/avformat.h>
// #include <libavutil/opt.h>
// #include <libswscale/swscale.h>
// }

// WebRTCVideoSource implementation - 简化版本不依赖ffmpeg
WebRTCVideoSource::WebRTCVideoSource()
    : target_fps_(30) {
}

WebRTCVideoSource::~WebRTCVideoSource() {
    // Cleanup simplified
}

void WebRTCVideoSource::PushFrame(const std::string& source_id, const cv::Mat& frame) {
    if (frame.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto& source = video_sources_[source_id];

    // 帧率控制
    auto now = std::chrono::steady_clock::now();
    auto frame_interval = std::chrono::milliseconds(1000 / target_fps_);

    if (now - source.last_frame_time < frame_interval) {
        return; // Skip frame to maintain target FPS
    }

    source.last_frame_time = now;

    // 初始化编码器（如果需要）
    if (!source.encoder_initialized || frame.cols != source.frame_width || frame.rows != source.frame_height) {
        source.frame_width = frame.cols;
        source.frame_height = frame.rows;
        source.encoder_initialized = true;
        std::cout << "Encoder initialized for source " << source_id << ": " << frame.cols << "x" << frame.rows << std::endl;
    }

    // 编码帧
    std::vector<uint8_t> encoded_frame = EncodeFrame(frame);

    if (!encoded_frame.empty()) {
        source.encoded_frames.push(std::move(encoded_frame));

        // 限制队列大小
        while (source.encoded_frames.size() > 10) {
            source.encoded_frames.pop();
        }
    }
}

std::vector<uint8_t> WebRTCVideoSource::GetEncodedFrame(const std::string& source_id) {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = video_sources_.find(source_id);
    if (it == video_sources_.end() || it->second.encoded_frames.empty()) {
        return {};
    }

    std::vector<uint8_t> frame = std::move(it->second.encoded_frames.front());
    it->second.encoded_frames.pop();
    return frame;
}

bool WebRTCVideoSource::HasEncodedFrame(const std::string& source_id) const {
    std::lock_guard<std::mutex> lock(sources_mutex_);
    auto it = video_sources_.find(source_id);
    return it != video_sources_.end() && !it->second.encoded_frames.empty();
}


std::vector<uint8_t> WebRTCVideoSource::EncodeFrame(const cv::Mat& frame) {

    // ����֡��С���Ż����䣨���ԭʼ̫֡��
    cv::Mat resized_frame = frame;
    if (frame.cols > 640 || frame.rows > 480) {
        // ���ֿ�߱ȣ����640x480
        double scale = std::min(640.0 / frame.cols, 480.0 / frame.rows);
        int new_width = static_cast<int>(frame.cols * scale);
        int new_height = static_cast<int>(frame.rows * scale);
        cv::resize(frame, resized_frame, cv::Size(new_width, new_height), 0, 0, cv::INTER_LINEAR);
    }

    // ʹ��JPEG���룬ͨ������ͨ������
    std::vector<uint8_t> encoded_data;
    std::vector<int> compression_params = {
        cv::IMWRITE_JPEG_QUALITY, 75,  // ���������75
        cv::IMWRITE_JPEG_OPTIMIZE, 1   // �����Ż�
    };

    if (cv::imencode(".jpg", resized_frame, encoded_data, compression_params)) {
        return encoded_data;
    }

    return {};
}


// WebRTCStreamer implementation - ����libdatachannel
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

        // �����Ƶ֡�����߳�
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
    // ����libdatachannel - ���������ӣ���ʹ��STUN/TURN
    // ������κ�ICE��������libdatachannel���Զ�����host��ѡ
    rtc_config_.enableIceTcp = false;  // ֻ��UDP
    rtc_config_.disableAutoNegotiation = false;
    rtc_config_.portRangeBegin = 10000;  // ָ���˿ڷ�Χ
    rtc_config_.portRangeEnd = 10100;

    // �󶨵�localhost�������������
    rtc_config_.bindAddress = "127.0.0.1";

    std::cout << "libdatachannel configuration initialized (localhost only, no STUN)" << std::endl;
}

void WebRTCStreamer::Shutdown() {
    if (!initialized_) {
        return;
    }

    // ֹͣ��Ƶ�����߳�
    should_stop_sender_ = true;
    if (video_sender_thread_.joinable()) {
        video_sender_thread_.join();
    }

    // ������������
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
        // �ֲ������洢�ص���Ϣ
        Json::Value offer_message;
        bool should_send_callback = false;

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);

            // �����µ�PeerConnection
            auto peer_connection = CreatePeerConnection(client_id);
            if (!peer_connection) {
                return false;
            }

            // �����ͻ�������
            auto client = std::make_unique<ClientConnection>();
            client->client_id = client_id;
            client->peer_connection = peer_connection;
            client->connected = false;
            client->connect_time = std::chrono::steady_clock::now();

            // ��������ͨ����������Ƶ֡��JPEG��ʽ��
            // ʹ������ͨ������Ƶ������򵥣�����Ҫ���ӵ�H.264����
            auto data_channel = peer_connection->createDataChannel("video");
            client->data_channel = data_channel;

            // ��������ͨ���ص�
            data_channel->onOpen([client_id, this]() {
                std::cout << "����ͨ���Ѵ�: " << client_id << std::endl;
            });

            data_channel->onClosed([client_id]() {
                std::cout << "����ͨ���ѹر�: " << client_id << std::endl;
            });

            clients_[client_id] = std::move(client);

            // ʹ��libdatachannel��createOffer����
            auto offer_desc = peer_connection->createOffer();

            // ���ñ�������
            peer_connection->setLocalDescription(rtc::Description::Type::Offer);

            // ��ȡ���ɵ�SDP offer
            sdp_offer = std::string(offer_desc);

            std::cout << "Created offer for client: " << client_id << std::endl;

            // ׼���ص���Ϣ�������ڣ�
            if (on_signaling_message_) {
                offer_message["type"] = "offer";
                offer_message["data"]["type"] = "offer";
                offer_message["data"]["sdp"] = sdp_offer;
                offer_message["timestamp"] = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                should_send_callback = true;
            }
        } // ���������ͷ�

        // �����ⷢ�ͻص�����������
        if (should_send_callback && on_signaling_message_) {
            on_signaling_message_(client_id, offer_message);
            std::cout << "�ѷ���WebRTC offer���ͻ���: " << client_id << std::endl;
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
        // ����Զ������
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
        std::cerr << "�ͻ���δ�ҵ�: " << client_id << std::endl;
        return false;
    }

    try {
        std::string candidate_str = candidate["candidate"].asString();
        std::string mid = candidate["sdpMid"].asString();

        std::cout << "�յ�Զ��ICE��ѡ: " << candidate_str << std::endl;

        rtc::Candidate rtc_candidate(candidate_str, mid);
        client_it->second->peer_connection->addRemoteCandidate(rtc_candidate);

        std::cout << "�ɹ����ICE��ѡ for client: " << client_id << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to add ICE candidate for " << client_id << ": " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<rtc::PeerConnection> WebRTCStreamer::CreatePeerConnection(const std::string& client_id) {
    try {
        auto peer_connection = std::make_shared<rtc::PeerConnection>(rtc_config_);

        // ����״̬�仯�ص�
        peer_connection->onStateChange([this, client_id](rtc::PeerConnection::State state) {
            HandleConnectionStateChange(client_id, state);
        });

        // ����ICE��ѡ�ص�
        peer_connection->onLocalCandidate([this, client_id](rtc::Candidate candidate) {
            std::string cand_str = std::string(candidate);

            // ��ӡ��ϸ��ICE��ѡ��Ϣ���ڵ���
            std::cout << "���ɱ���ICE��ѡ: " << cand_str << std::endl;

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

    // ���״̬���Ƶ����
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
        if (kVerboseLogging) {
            std::cout << "WebRTCStreamer::PushFrame - 推送帧，源ID: " << source_id << ", 帧大小: " << frame.cols << "x" << frame.rows << std::endl;
        }
        video_source_->PushFrame(source_id, frame);

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
        // 先收集需要处理的客户端信息，避免长时间持有锁
        std::vector<std::pair<std::string, std::string>> clients_to_process;
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            for (auto& [client_id, client] : clients_) {
                if (client->connected && client->data_channel && client->data_channel->isOpen()) {
                    clients_to_process.push_back({client_id, client->requested_source});
                }
            }
        }

        // 为每个连接的客户端发送对应源的视频帧
        for (const auto& [client_id, requested_source] : clients_to_process) {
            if (video_source_ && video_source_->HasEncodedFrame(requested_source)) {
                auto encoded_frame = video_source_->GetEncodedFrame(requested_source);

                if (!encoded_frame.empty()) {
                    // 重新获取客户端引用发送数据（需要再次加锁）
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    auto client_it = clients_.find(client_id);
                    if (client_it == clients_.end()) continue;

                    auto& client = client_it->second;
                    if (!client->connected || !client->data_channel || !client->data_channel->isOpen()) continue;

                    if (frames_sent_count % 30 == 0) {  // 每30帧记录一次
                        std::cout << "发送视频帧到客户端 " << client_id
                                  << " (源: " << requested_source << "), 大小: "
                                  << encoded_frame.size() << " bytes" << std::endl;
                    }
                    frames_sent_count++;

                    try {
                        // 发送JPEG图像通过数据通道
                        // 如果数据较大，分块发送以提高可靠性
                        uint32_t total_size = static_cast<uint32_t>(encoded_frame.size());

                        if (total_size <= MAX_CHUNK_SIZE - 4) {
                            // 小数据，一次发送（格式：前4字节为大小，后面是JPEG数据）
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
                            // 首先发送头部消息（帧总大小）
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

bool WebRTCStreamer::SetClientSource(const std::string& client_id, const std::string& source_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto client_it = clients_.find(client_id);
    if (client_it == clients_.end()) {
        std::cerr << "客户端未找到: " << client_id << std::endl;
        return false;
    }

    client_it->second->requested_source = source_id;
    std::cout << "客户端 " << client_id << " 切换到视频源: " << source_id << std::endl;

    return true;
}

std::string WebRTCStreamer::GetClientSource(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto client_it = clients_.find(client_id);
    if (client_it == clients_.end()) {
        return "";
    }

    return client_it->second->requested_source;
}


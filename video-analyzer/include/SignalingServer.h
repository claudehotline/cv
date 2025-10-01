#pragma once

#ifndef IXWEBSOCKET_HEADER_ONLY
#define IXWEBSOCKET_HEADER_ONLY
#endif
#include <ixwebsocket/IXWebSocketServer.h>
#include <json/json.h>
#include <memory>
#include <thread>
#include <mutex>
#include <map>
#include <functional>
#include <atomic>

class SignalingServer {
public:
    SignalingServer();
    ~SignalingServer();

    bool start(uint16_t port);
    void stop();

    // 设置与视频分析模块的通信回调
    void setMessageCallback(std::function<void(const std::string&, const Json::Value&)> callback);

    // 向特定客户端发送消息
    bool sendToClient(const std::string& client_id, const Json::Value& message);

    // 广播消息到所有客户端
    void broadcastMessage(const Json::Value& message);

    // 处理来自视频分析模块的消息
    void handleMessage(const std::string& client_id, const Json::Value& message);

    bool isRunning() const { return running_; }
    size_t getClientCount() const;

private:
    std::unique_ptr<ix::WebSocketServer> server_;
    std::atomic<bool> running_;
    uint16_t port_;

    struct ClientInfo {
        std::weak_ptr<ix::WebSocket> connection;
        std::string client_id;
        std::string client_type;
        int64_t connect_time;
        bool authenticated;
    };

    mutable std::mutex clients_mutex_;
    std::map<std::string, ClientInfo> clients_;
    std::map<std::weak_ptr<ix::WebSocket>, std::string, std::owner_less<std::weak_ptr<ix::WebSocket>>> connection_to_client_;

    // 与视频分析模块通信的回调
    std::function<void(const std::string&, const Json::Value&)> message_callback_;

    // WebSocket事件处理
    void onConnection(std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState);
    void onMessage(std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg);

    // 消息处理
    void handleClientAuthentication(std::weak_ptr<ix::WebSocket> webSocket, const Json::Value& message);
    void handleWebRTCSignaling(const std::string& client_id, const Json::Value& message);
    void handleControlMessage(const std::string& client_id, const Json::Value& message);

    // 工具方法
    void sendMessage(std::weak_ptr<ix::WebSocket> webSocket, const Json::Value& message);
    std::string generateClientId();
};

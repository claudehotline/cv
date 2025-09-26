#include "SignalingServer.h"
#include <iostream>
#include <sstream>
#include <random>
#include <chrono>

SignalingServer::SignalingServer() : running_(false), port_(0) {
    // Server will be initialized in start()
}

SignalingServer::~SignalingServer() {
    stop();
}

bool SignalingServer::start(uint16_t port) {
    if (running_) {
        return false;
    }

    try {
        port_ = port;
        server_ = std::make_unique<ix::WebSocketServer>(port);

        // Set up callbacks
        server_->setOnConnectionCallback(
            [this](std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState) {
                onConnection(webSocket, connectionState);

                // Set message callback for this specific connection
                if (auto shared_ws = webSocket.lock()) {
                    shared_ws->setOnMessageCallback(
                        [this, webSocket, connectionState](const ix::WebSocketMessagePtr& msg) {
                            if (auto shared_ws = webSocket.lock()) {
                                onMessage(connectionState, *shared_ws, msg);
                            }
                        }
                    );

                }
            }
        );

        auto result = server_->listen();
        if (!result.first) {
            std::cerr << "Failed to listen on port " << port << ": " << result.second << std::endl;
            return false;
        }

        server_->start();
        running_ = true;

        std::cout << "WebRTC Signaling Server started on port: " << port << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Failed to start signaling server: " << e.what() << std::endl;
        return false;
    }
}

void SignalingServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    try {
        if (server_) {
            server_->stop();
            server_.reset();
        }

        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.clear();
        connection_to_client_.clear();

        std::cout << "WebRTC Signaling Server stopped" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error stopping signaling server: " << e.what() << std::endl;
    }
}

void SignalingServer::onConnection(std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState> connectionState) {
    std::cout << "New signaling client connected from " << connectionState->getRemoteIp() << std::endl;

    // Send welcome message
    Json::Value welcome;
    welcome["type"] = "welcome";
    welcome["message"] = "Please send authentication info";
    welcome["timestamp"] = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    sendMessage(webSocket, welcome);
}

void SignalingServer::onMessage(std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg) {
    if (msg->type == ix::WebSocketMessageType::Message) {
        try {
            Json::Value json_msg;
            Json::CharReaderBuilder builder;
            std::string errs;

            std::istringstream iss(msg->str);
            if (!Json::parseFromStream(builder, iss, &json_msg, &errs)) {
                std::cerr << "JSON parsing error: " << errs << std::endl;
                return;
            }

            std::string msg_type = json_msg.get("type", "").asString();

            if (msg_type == "auth") {
                // Get weak_ptr from WebSocket reference
                auto clients = server_->getClients();
                for (auto client : clients) {
                    if (client.get() == &webSocket) {
                        handleClientAuthentication(std::weak_ptr<ix::WebSocket>(client), json_msg);
                        break;
                    }
                }
            } else {
                // Find client ID for this connection
                std::lock_guard<std::mutex> lock(clients_mutex_);
                for (auto& [weak_ws, client_id] : connection_to_client_) {
                    if (auto shared_ws = weak_ws.lock()) {
                        if (shared_ws.get() == &webSocket) {
                            if (msg_type == "webrtc" || msg_type == "request_offer" || msg_type == "answer" || msg_type == "ice_candidate") {
                                handleWebRTCSignaling(client_id, json_msg);
                            } else if (msg_type == "control" || msg_type == "start_analysis" || msg_type == "stop_analysis") {
                                handleControlMessage(client_id, json_msg);
                            } else {
                                std::cout << "未知消息类型: " << msg_type << " from " << client_id << std::endl;
                            }
                            break;
                        }
                    }
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Message processing error: " << e.what() << std::endl;
        }
    } else if (msg->type == ix::WebSocketMessageType::Close) {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        for (auto it = connection_to_client_.begin(); it != connection_to_client_.end();) {
            if (auto shared_ws = it->first.lock()) {
                if (shared_ws.get() == &webSocket) {
                    std::string client_id = it->second;
                    std::cout << "Signaling client disconnected: " << client_id << std::endl;

                    clients_.erase(client_id);
                    it = connection_to_client_.erase(it);
                    break;
                } else {
                    ++it;
                }
            } else {
                it = connection_to_client_.erase(it);
            }
        }
    }
}

void SignalingServer::handleClientAuthentication(std::weak_ptr<ix::WebSocket> webSocket, const Json::Value& message) {
    // 支持两种格式：直接属性或嵌套在data中
    std::string client_type = message.get("client_type", "").asString();
    if (client_type.empty() && message.isMember("data")) {
        client_type = message["data"].get("client_type", "").asString();
    }

    if (client_type.empty()) {
        Json::Value error;
        error["type"] = "auth_error";
        error["message"] = "Missing client_type";
        sendMessage(webSocket, error);
        return;
    }

    std::string client_id = generateClientId();

    std::lock_guard<std::mutex> lock(clients_mutex_);

    ClientInfo info;
    info.connection = webSocket;
    info.client_id = client_id;
    info.client_type = client_type;
    info.connect_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.authenticated = true;

    clients_[client_id] = info;
    connection_to_client_[webSocket] = client_id;

    Json::Value response;
    response["type"] = "auth_success";
    response["client_id"] = client_id;
    response["message"] = "Authentication successful";
    sendMessage(webSocket, response);

    std::cout << "Client authenticated: " << client_id << " (" << client_type << ")" << std::endl;
}

void SignalingServer::handleWebRTCSignaling(const std::string& client_id, const Json::Value& message) {
    std::string msg_type = message.get("type", "").asString();
    std::cout << "处理WebRTC信令消息: " << msg_type << " from " << client_id << std::endl;

    if (video_analyzer_callback_) {
        video_analyzer_callback_(client_id, message);
    } else {
        std::cerr << "警告: video_analyzer_callback_ 未设置" << std::endl;
    }
}

void SignalingServer::handleControlMessage(const std::string& client_id, const Json::Value& message) {
    std::string action = message.get("action", "").asString();
    std::cout << "Control message from " << client_id << ": " << action << std::endl;

    if (video_analyzer_callback_) {
        video_analyzer_callback_(client_id, message);
    }
}

void SignalingServer::sendMessage(std::weak_ptr<ix::WebSocket> webSocket, const Json::Value& message) {
    if (auto shared_ws = webSocket.lock()) {
        try {
            Json::StreamWriterBuilder builder;
            std::string json_str = Json::writeString(builder, message);
            shared_ws->send(json_str);
        } catch (const std::exception& e) {
            std::cerr << "Error sending message: " << e.what() << std::endl;
        }
    }
}

bool SignalingServer::sendToClient(const std::string& client_id, const Json::Value& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    auto client_it = clients_.find(client_id);
    if (client_it == clients_.end()) {
        return false;
    }

    sendMessage(client_it->second.connection, message);
    return true;
}

void SignalingServer::broadcastMessage(const Json::Value& message) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    for (const auto& [client_id, client_info] : clients_) {
        sendMessage(client_info.connection, message);
    }
}

void SignalingServer::handleVideoAnalyzerMessage(const std::string& client_id, const Json::Value& message) {
    sendToClient(client_id, message);
}

std::string SignalingServer::generateClientId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(100000, 999999);

    return "client_" + std::to_string(dis(gen));
}

size_t SignalingServer::getClientCount() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    return clients_.size();
}

void SignalingServer::setVideoAnalyzerCallback(std::function<void(const std::string&, const Json::Value&)> callback) {
    video_analyzer_callback_ = callback;
}
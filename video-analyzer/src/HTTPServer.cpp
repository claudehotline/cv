#include "HTTPServer.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#undef DELETE  // 避免与Windows.h中的DELETE宏冲突
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

HTTPServer::HTTPServer(int port) : port_(port), running_(false) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

HTTPServer::~HTTPServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

void HTTPServer::GET(const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_.push_back({"GET", path, handler});
}

void HTTPServer::POST(const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_.push_back({"POST", path, handler});
}

void HTTPServer::PUT(const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_.push_back({"PUT", path, handler});
}

void HTTPServer::DELETE(const std::string& path, RouteHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_.push_back({"DELETE", path, handler});
}

bool HTTPServer::start() {
    if (running_) {
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&HTTPServer::serverLoop, this);

    std::cout << "HTTP服务器已启动，端口: " << port_ << std::endl;
    return true;
}

void HTTPServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    std::cout << "HTTP服务器已停止" << std::endl;
}

void HTTPServer::serverLoop() {
    int server_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // 创建socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "创建socket失败" << std::endl;
        return;
    }

    // 设置地址重用
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // 绑定地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "绑定端口失败: " << port_ << std::endl;
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return;
    }

    // 开始监听
    if (listen(server_socket, 10) < 0) {
        std::cerr << "监听失败" << std::endl;
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return;
    }

    while (running_) {
        // 接受连接
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "接受连接失败" << std::endl;
            }
            continue;
        }

        // 处理请求（简化版本，实际应该使用线程池）
        std::thread client_thread(&HTTPServer::handleClient, this, client_socket);
        client_thread.detach();
    }

#ifdef _WIN32
    closesocket(server_socket);
#else
    close(server_socket);
#endif
}

void HTTPServer::handleClient(int client_socket) {
    char buffer[4096];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received <= 0) {
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return;
    }

    buffer[bytes_received] = '\0';
    std::string raw_request(buffer);

    try {
        // 解析请求
        Request request = parseRequest(raw_request);
        Response response;

        // 处理OPTIONS预检请求
        if (request.method == "OPTIONS") {
            response = Response(200);
        } else {
            // 查找匹配的路由
            bool found = false;
            std::lock_guard<std::mutex> lock(routes_mutex_);

            for (const auto& route : routes_) {
                if (matchRoute(request.method, request.path, route, request.params)) {
                    response = route.handler(request);
                    found = true;
                    break;
                }
            }

            if (!found) {
                response = errorResponse("路由未找到", 404);
            }
        }

        // 发送响应
        std::string response_str = buildResponse(response);
        send(client_socket, response_str.c_str(), response_str.length(), 0);

    } catch (const std::exception& e) {
        std::cerr << "处理请求时出错: " << e.what() << std::endl;
        Response error_response = errorResponse("服务器内部错误", 500);
        std::string response_str = buildResponse(error_response);
        send(client_socket, response_str.c_str(), response_str.length(), 0);
    }

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

HTTPServer::Request HTTPServer::parseRequest(const std::string& raw_request) {
    Request request;
    std::istringstream iss(raw_request);
    std::string line;

    // 解析请求行
    if (std::getline(iss, line)) {
        std::istringstream request_line(line);
        request_line >> request.method;

        std::string uri;
        request_line >> uri;

        // 分离路径和查询参数
        size_t query_pos = uri.find('?');
        if (query_pos != std::string::npos) {
            request.path = uri.substr(0, query_pos);
            request.query = uri.substr(query_pos + 1);
        } else {
            request.path = uri;
        }
    }

    // 解析头部
    std::string body_content;
    bool in_body = false;
    while (std::getline(iss, line)) {
        if (line == "\r" || line.empty()) {
            in_body = true;
            continue;
        }

        if (in_body) {
            body_content += line + "\n";
        } else {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                // 去除空白字符
                key.erase(0, key.find_first_not_of(" \t\r\n"));
                key.erase(key.find_last_not_of(" \t\r\n") + 1);
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                request.headers[key] = value;
            }
        }
    }

    request.body = body_content;
    return request;
}

bool HTTPServer::matchRoute(const std::string& method, const std::string& path,
                           const Route& route, std::map<std::string, std::string>& params) {
    if (method != route.method) {
        return false;
    }

    // 简单的路径匹配（支持 :id 参数）
    if (route.pattern == path) {
        return true;
    }

    // 处理路径参数 (例如 /api/sources/:id)
    std::regex pattern_regex(route.pattern);
    std::string regex_pattern = route.pattern;

    // 将 :id 替换为 ([^/]+)
    regex_pattern = std::regex_replace(regex_pattern, std::regex(":([a-zA-Z_][a-zA-Z0-9_]*)"), "([^/]+)");

    std::regex path_regex(regex_pattern);
    std::smatch matches;

    if (std::regex_match(path, matches, path_regex)) {
        // 提取参数
        std::regex param_regex(":([a-zA-Z_][a-zA-Z0-9_]*)");
        std::sregex_iterator iter(route.pattern.begin(), route.pattern.end(), param_regex);
        std::sregex_iterator end;

        int param_index = 1;
        for (; iter != end; ++iter, ++param_index) {
            if (param_index < static_cast<int>(matches.size())) {
                params[iter->str(1)] = matches[param_index].str();
            }
        }
        return true;
    }

    return false;
}

std::string HTTPServer::buildResponse(const Response& response) {
    std::ostringstream oss;

    // 状态行
    oss << "HTTP/1.1 " << response.status_code;
    switch (response.status_code) {
        case 200: oss << " OK"; break;
        case 201: oss << " Created"; break;
        case 400: oss << " Bad Request"; break;
        case 404: oss << " Not Found"; break;
        case 500: oss << " Internal Server Error"; break;
        default: oss << " Unknown"; break;
    }
    oss << "\r\n";

    // 头部
    oss << "Content-Length: " << response.body.length() << "\r\n";
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    oss << "\r\n";

    // 响应体
    oss << response.body;

    return oss.str();
}

Json::Value HTTPServer::parseJsonBody(const std::string& body) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;

    std::istringstream iss(body);
    if (!Json::parseFromStream(builder, iss, &root, &errs)) {
        throw std::runtime_error("JSON解析失败: " + errs);
    }

    return root;
}

HTTPServer::Response HTTPServer::jsonResponse(const Json::Value& data, int status) {
    Response response(status);
    Json::StreamWriterBuilder builder;
    response.body = Json::writeString(builder, data);
    return response;
}

HTTPServer::Response HTTPServer::errorResponse(const std::string& message, int status) {
    Json::Value error;
    error["success"] = false;
    error["message"] = message;
    error["timestamp"] = static_cast<int64_t>(time(nullptr));

    return jsonResponse(error, status);
}
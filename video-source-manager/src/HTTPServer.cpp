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

    std::cout << "HTTP server started on port: " << port_ << std::endl;
    return true;
}

void HTTPServer::stop() {
    if (running_) {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        std::cout << "HTTP server stopped" << std::endl;
    }
}

void HTTPServer::serverLoop() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        std::cerr << "Failed to bind socket to port " << port_ << std::endl;
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return;
    }

    if (listen(server_socket, 10) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
#ifdef _WIN32
        closesocket(server_socket);
#else
        close(server_socket);
#endif
        return;
    }

    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

#ifdef _WIN32
        int client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_socket == INVALID_SOCKET) {
#else
        int client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
        if (client_socket < 0) {
#endif
            if (running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }

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
    Request request = parseRequest(std::string(buffer));

    Response response(404);
    response.body = "{\"error\": \"Not Found\"}";

    std::lock_guard<std::mutex> lock(routes_mutex_);
    std::map<std::string, std::string> params;

    for (const auto& route : routes_) {
        if (matchRoute(request.method, request.path, route, params)) {
            request.params = params;
            try {
                response = route.handler(request);
            } catch (const std::exception& e) {
                response = errorResponse("Internal server error: " + std::string(e.what()), 500);
            }
            break;
        }
    }

    std::string response_str = buildResponse(response);
    send(client_socket, response_str.c_str(), static_cast<int>(response_str.length()), 0);

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

    if (std::getline(iss, line)) {
        std::istringstream request_line(line);
        std::string path_query;
        request_line >> request.method >> path_query;

        size_t query_pos = path_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_query.substr(0, query_pos);
            request.query = path_query.substr(query_pos + 1);
        } else {
            request.path = path_query;
        }
    }

    while (std::getline(iss, line) && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 2);
            if (!value.empty() && value.back() == '\r') {
                value.pop_back();
            }
            request.headers[key] = value;
        }
    }

    std::string body_line;
    while (std::getline(iss, body_line)) {
        request.body += body_line;
    }

    return request;
}

std::string HTTPServer::buildResponse(const Response& response) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.status_code << " ";

    switch (response.status_code) {
        case 200: oss << "OK"; break;
        case 201: oss << "Created"; break;
        case 400: oss << "Bad Request"; break;
        case 404: oss << "Not Found"; break;
        case 500: oss << "Internal Server Error"; break;
        default: oss << "Unknown"; break;
    }

    oss << "\r\n";

    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }

    oss << "Content-Length: " << response.body.length() << "\r\n";
    oss << "\r\n";
    oss << response.body;

    return oss.str();
}

bool HTTPServer::matchRoute(const std::string& method, const std::string& path,
                           const Route& route, std::map<std::string, std::string>& params) {
    if (method != route.method) {
        return false;
    }

    if (route.pattern == path) {
        return true;
    }

    std::regex pattern_regex(":[a-zA-Z_][a-zA-Z0-9_]*");
    std::string regex_pattern = std::regex_replace(route.pattern, pattern_regex, "([^/]+)");
    regex_pattern = "^" + regex_pattern + "$";

    std::regex path_regex(regex_pattern);
    std::smatch matches;

    if (std::regex_match(path, matches, path_regex)) {
        std::sregex_iterator param_iter(route.pattern.begin(), route.pattern.end(), pattern_regex);
        std::sregex_iterator param_end;

        int match_index = 1;
        for (auto iter = param_iter; iter != param_end; ++iter) {
            if (match_index < static_cast<int>(matches.size())) {
                std::string param_name = iter->str().substr(1);
                params[param_name] = matches[match_index].str();
                match_index++;
            }
        }
        return true;
    }

    return false;
}

Json::Value HTTPServer::parseJsonBody(const std::string& body) {
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(body, root)) {
        throw std::runtime_error("Invalid JSON format");
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
    error["error"] = message;
    error["success"] = false;
    return jsonResponse(error, status);
}
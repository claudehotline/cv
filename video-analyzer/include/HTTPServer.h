#pragma once

#ifdef _WIN32
#ifdef DELETE
#undef DELETE
#endif
#endif

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <json/json.h>

// 简单的HTTP服务器实现
class HTTPServer {
public:
    struct Request {
        std::string method;           // GET, POST, PUT, DELETE
        std::string path;             // /api/sources
        std::string query;            // ?param=value
        std::map<std::string, std::string> headers;
        std::string body;             // POST/PUT数据
        std::map<std::string, std::string> params; // URL参数
    };

    struct Response {
        int status_code = 200;        // HTTP状态码
        std::map<std::string, std::string> headers;
        std::string body;             // 响应体

        Response(int code = 200) : status_code(code) {
            headers["Content-Type"] = "application/json";
            headers["Access-Control-Allow-Origin"] = "*";
            headers["Access-Control-Allow-Methods"] = "GET,POST,PUT,DELETE,OPTIONS";
            headers["Access-Control-Allow-Headers"] = "Content-Type,Authorization";
        }
    };

    using RouteHandler = std::function<Response(const Request&)>;

public:
    HTTPServer(int port = 8080);
    ~HTTPServer();

    // 路由注册
    void GET(const std::string& path, RouteHandler handler);
    void POST(const std::string& path, RouteHandler handler);
    void PUT(const std::string& path, RouteHandler handler);
    void DELETE(const std::string& path, RouteHandler handler);

    // 服务器控制
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // 工具方法
    static Json::Value parseJsonBody(const std::string& body);
    static Response jsonResponse(const Json::Value& data, int status = 200);
    static Response errorResponse(const std::string& message, int status = 400);

private:
    struct Route {
        std::string method;
        std::string pattern;
        RouteHandler handler;
    };

    int port_;
    std::atomic<bool> running_;
    std::thread server_thread_;

    std::vector<Route> routes_;
    std::mutex routes_mutex_;

    void serverLoop();
    bool matchRoute(const std::string& method, const std::string& path,
                   const Route& route, std::map<std::string, std::string>& params);
    Request parseRequest(const std::string& raw_request);
    std::string buildResponse(const Response& response);
    void handleClient(int client_socket);
};
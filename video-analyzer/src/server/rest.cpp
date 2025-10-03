#include "server/rest.hpp"

#include "app/application.hpp"
#include "analyzer/analyzer.hpp"
#include "core/engine_manager.hpp"
#include "core/logger.hpp"

#include <json/json.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#undef DELETE
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace va::server {

namespace {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> params;
};

struct HttpResponse {
    int status_code {200};
    std::map<std::string, std::string> headers {
        {"Content-Type", "application/json"},
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET,POST,PUT,DELETE,OPTIONS,PATCH"},
        {"Access-Control-Allow-Headers", "Content-Type,Authorization"}
    };
    std::string body;
};

Json::Value parseJson(const std::string& body) {
    if (body.empty()) {
        return Json::Value(Json::objectValue);
    }
    Json::CharReaderBuilder builder;
    std::string errs;
    std::istringstream iss(body);
    Json::Value root;
    if (!Json::parseFromStream(builder, iss, &root, &errs)) {
        throw std::runtime_error("JSON parse error: " + errs);
    }
    return root;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

class SimpleHttpServer {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    explicit SimpleHttpServer(const RestServerOptions& options)
        : options_(options) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~SimpleHttpServer() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void addRoute(const std::string& method, const std::string& pattern, Handler handler) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        routes_.push_back(Route{method, pattern, buildRegex(pattern), extractParams(pattern), std::move(handler)});
    }

    bool start() {
        if (running_.exchange(true)) {
            return false;
        }
        server_thread_ = std::thread(&SimpleHttpServer::serverLoop, this);
        return true;
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        if (server_socket_ != -1) {
#ifdef _WIN32
            shutdown(server_socket_, SD_BOTH);
            closesocket(server_socket_);
#else
            shutdown(server_socket_, SHUT_RDWR);
            close(server_socket_);
#endif
            server_socket_ = -1;
        }

        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

private:
    struct Route {
        std::string method;
        std::string pattern;
        std::regex regex;
        std::vector<std::string> params;
        Handler handler;
    };

    RestServerOptions options_;
    std::atomic<bool> running_ {false};
    std::thread server_thread_;
    std::mutex routes_mutex_;
    std::vector<Route> routes_;
    int server_socket_ {-1};

    static std::regex buildRegex(const std::string& pattern) {
        std::string regex_pattern = std::regex_replace(pattern, std::regex(R"(:([a-zA-Z_][a-zA-Z0-9_]*))"), "([^/]+)");
        return std::regex("^" + regex_pattern + "$", std::regex::ECMAScript);
    }

    static std::vector<std::string> extractParams(const std::string& pattern) {
        std::vector<std::string> params;
        std::regex param_regex(R"(:([a-zA-Z_][a-zA-Z0-9_]*))");
        std::sregex_iterator iter(pattern.begin(), pattern.end(), param_regex);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
            params.push_back(iter->str(1));
        }
        return params;
    }

    void serverLoop() {
        server_socket_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (server_socket_ < 0) {
            VA_LOG_ERROR() << "REST server: failed to create socket";
            running_ = false;
            return;
        }

        int opt = 1;
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(options_.port));

        if (options_.host == "0.0.0.0" || options_.host == "*") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, options_.host.c_str(), &addr.sin_addr) <= 0) {
                VA_LOG_WARN() << "REST server: invalid host " << options_.host << ", defaulting to INADDR_ANY";
                addr.sin_addr.s_addr = INADDR_ANY;
            }
        }

        if (bind(server_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            VA_LOG_ERROR() << "REST server: bind failed on port " << options_.port;
            running_ = false;
#ifdef _WIN32
            closesocket(server_socket_);
#else
            close(server_socket_);
#endif
            server_socket_ = -1;
            return;
        }

        if (listen(server_socket_, 16) < 0) {
            VA_LOG_ERROR() << "REST server: listen failed";
            running_ = false;
#ifdef _WIN32
            closesocket(server_socket_);
#else
            close(server_socket_);
#endif
            server_socket_ = -1;
            return;
        }

        VA_LOG_INFO() << "REST server listening on " << options_.host << ":" << options_.port;

        while (running_) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            int client_socket = accept(server_socket_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);
            if (client_socket < 0) {
                if (running_) {
                    VA_LOG_WARN() << "REST server: accept failed";
                }
                continue;
            }

            std::thread(&SimpleHttpServer::handleClient, this, client_socket).detach();
        }
    }

    void handleClient(int client_socket) {
        constexpr size_t buffer_size = 8192;
        std::string request_data;
        request_data.reserve(buffer_size);
        char buffer[buffer_size];
        int received = 0;

        do {
#ifdef _WIN32
            received = recv(client_socket, buffer, static_cast<int>(buffer_size), 0);
#else
            received = static_cast<int>(recv(client_socket, buffer, buffer_size, 0));
#endif
            if (received > 0) {
                request_data.append(buffer, static_cast<size_t>(received));
                if (request_data.find("\r\n\r\n") != std::string::npos) {
                    break;
                }
            }
        } while (received > 0);

        if (request_data.empty()) {
#ifdef _WIN32
            closesocket(client_socket);
#else
            close(client_socket);
#endif
            return;
        }

        HttpRequest request;
        try {
            request = parseRequest(request_data);
        } catch (const std::exception& ex) {
            HttpResponse response;
            response.status_code = 400;
            Json::Value error(Json::objectValue);
            error["success"] = false;
            error["message"] = ex.what();
            response.body = Json::writeString(Json::StreamWriterBuilder{}, error);
            const auto raw = buildResponse(response);
            sendAll(client_socket, raw);
#ifdef _WIN32
            closesocket(client_socket);
#else
            close(client_socket);
#endif
            return;
        }

        if (request.method == "OPTIONS") {
            HttpResponse response;
            response.status_code = 204;
            const auto raw = buildResponse(response);
            sendAll(client_socket, raw);
#ifdef _WIN32
            closesocket(client_socket);
#else
            close(client_socket);
#endif
            return;
        }

        HttpResponse response;
        bool matched = false;
        {
            std::lock_guard<std::mutex> lock(routes_mutex_);
            for (auto& route : routes_) {
                std::map<std::string, std::string> params;
                if (matchRoute(request.method, request.path, route, params)) {
                    request.params = std::move(params);
                    try {
                        response = route.handler(request);
                    } catch (const std::exception& ex) {
                        Json::Value error;
                        error["success"] = false;
                        error["message"] = ex.what();
                        response.status_code = 500;
                        response.body = Json::writeString(Json::StreamWriterBuilder{}, error);
                    }
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            Json::Value error;
            error["success"] = false;
            error["message"] = "Route not found";
            response.status_code = 404;
            response.body = Json::writeString(Json::StreamWriterBuilder{}, error);
        }

        const auto raw = buildResponse(response);
        sendAll(client_socket, raw);

#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
    }

    void sendAll(int client_socket, const std::string& data) const {
#ifdef _WIN32
        send(client_socket, data.data(), static_cast<int>(data.size()), 0);
#else
        send(client_socket, data.data(), data.size(), 0);
#endif
    }

    HttpRequest parseRequest(const std::string& raw_request) const {
        std::istringstream iss(raw_request);
        std::string line;
        HttpRequest request;

        if (!std::getline(iss, line)) {
            throw std::runtime_error("Invalid HTTP request");
        }

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream request_line(line);
        request_line >> request.method;
        std::string uri;
        request_line >> uri;

        auto query_pos = uri.find('?');
        if (query_pos != std::string::npos) {
            request.path = uri.substr(0, query_pos);
            request.query = uri.substr(query_pos + 1);
        } else {
            request.path = uri;
        }

        while (std::getline(iss, line) && line != "\r") {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                key.erase(key.begin(), std::find_if(key.begin(), key.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                key.erase(std::find_if(key.rbegin(), key.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), key.end());
                value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
                value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());
                request.headers.emplace(std::move(key), std::move(value));
            }
        }

        std::ostringstream body;
        body << iss.rdbuf();
        request.body = body.str();
        return request;
    }

    std::string buildResponse(const HttpResponse& response) const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << response.status_code << " ";
        switch (response.status_code) {
            case 200: oss << "OK"; break;
            case 201: oss << "Created"; break;
            case 204: oss << "No Content"; break;
            case 400: oss << "Bad Request"; break;
            case 404: oss << "Not Found"; break;
            case 500: oss << "Internal Server Error"; break;
            default: oss << "Unknown"; break;
        }
        oss << "\r\n";
        oss << "Content-Length: " << response.body.size() << "\r\n";
        for (const auto& header : response.headers) {
            oss << header.first << ": " << header.second << "\r\n";
        }
        oss << "\r\n";
        oss << response.body;
        return oss.str();
    }

    bool matchRoute(const std::string& method,
                    const std::string& path,
                    Route& route,
                    std::map<std::string, std::string>& params) const {
        if (method != route.method) {
            return false;
        }
        std::smatch matches;
        if (std::regex_match(path, matches, route.regex)) {
            for (size_t i = 0; i < route.params.size(); ++i) {
                params[route.params[i]] = matches[i + 1];
            }
            return true;
        }
        return false;
    }
};

HttpResponse jsonResponse(const Json::Value& value, int status = 200) {
    HttpResponse response;
    response.status_code = status;
    Json::StreamWriterBuilder builder;
    response.body = Json::writeString(builder, value);
    return response;
}

HttpResponse errorResponse(const std::string& message, int status = 400) {
    Json::Value error;
    error["success"] = false;
    error["message"] = message;
    return jsonResponse(error, status);
}

Json::Value successPayload() {
    Json::Value root(Json::objectValue);
    root["success"] = true;
    return root;
}

va::analyzer::AnalyzerParams buildParamsFromJson(const Json::Value& json) {
    va::analyzer::AnalyzerParams params;
    if (json.isMember("conf")) {
        params.confidence_threshold = static_cast<float>(json["conf"].asDouble());
    }
    if (json.isMember("iou")) {
        params.iou_threshold = static_cast<float>(json["iou"].asDouble());
    }
    return params;
}

va::core::EngineDescriptor buildEngineDescriptor(const Json::Value& json) {
    va::core::EngineDescriptor descriptor;
    descriptor.name = json.isMember("type") ? json["type"].asString() : "";
    descriptor.provider = json.isMember("provider") ? json["provider"].asString() : descriptor.name;
    descriptor.device_index = json.isMember("device") ? json["device"].asInt() : 0;

    if (json.isMember("options") && json["options"].isObject()) {
        for (const auto& name : json["options"].getMemberNames()) {
            descriptor.options[name] = json["options"][name].asString();
        }
    }
    return descriptor;
}

} // namespace

struct RestServer::Impl {
    RestServerOptions options;
    va::app::Application& app;
    SimpleHttpServer server;

    Impl(RestServerOptions opts, va::app::Application& application)
        : options(std::move(opts)), app(application), server(options) {
        registerRoutes();
    }

    void registerRoutes() {
        server.addRoute("POST", "/subscribe", [this](const HttpRequest& req) { return handleSubscribe(req); });
        server.addRoute("POST", "/unsubscribe", [this](const HttpRequest& req) { return handleUnsubscribe(req); });
        server.addRoute("POST", "/source/switch", [this](const HttpRequest& req) { return handleSourceSwitch(req); });
        server.addRoute("POST", "/model/switch", [this](const HttpRequest& req) { return handleModelSwitch(req); });
        server.addRoute("POST", "/task/switch", [this](const HttpRequest& req) { return handleTaskSwitch(req); });
        server.addRoute("PATCH", "/model/params", [this](const HttpRequest& req) { return handleParamsUpdate(req); });
        server.addRoute("POST", "/engine/set", [this](const HttpRequest& req) { return handleSetEngine(req); });
    }

    bool start() {
        return server.start();
    }

    void stop() {
        server.stop();
    }

    HttpResponse handleSubscribe(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile", "source_uri"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            const std::string stream_id = body["stream_id"].asString();
            const std::string profile = body["profile"].asString();
            const std::string uri = body["source_uri"].asString();
            std::optional<std::string> model_override;
            if (body.isMember("model_id") && body["model_id"].isString()) {
                model_override = body["model_id"].asString();
            }

            auto result = app.subscribeStream(stream_id, profile, uri, model_override);
            if (!result) {
                return errorResponse(app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            payload["subscription_id"] = *result;
            return jsonResponse(payload, 201);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleUnsubscribe(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            const bool success = app.unsubscribeStream(body["stream_id"].asString(), body["profile"].asString());
            if (!success) {
                return errorResponse(app.lastError().empty() ? "unsubscribe failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleSourceSwitch(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile", "source_uri"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            if (!app.switchSource(body["stream_id"].asString(), body["profile"].asString(), body["source_uri"].asString())) {
                return errorResponse(app.lastError().empty() ? "switch source failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleModelSwitch(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile", "model_id"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            if (!app.switchModel(body["stream_id"].asString(), body["profile"].asString(), body["model_id"].asString())) {
                return errorResponse(app.lastError().empty() ? "switch model failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleTaskSwitch(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile", "task"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            if (!app.switchTask(body["stream_id"].asString(), body["profile"].asString(), body["task"].asString())) {
                return errorResponse(app.lastError().empty() ? "switch task failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleParamsUpdate(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            const auto required = {"stream_id", "profile"};
            for (const auto* key : required) {
                if (!body.isMember(key) || !body[key].isString()) {
                    return errorResponse(std::string("Missing required field: ") + key, 400);
                }
            }

            auto params = buildParamsFromJson(body);
            if (!app.updateParams(body["stream_id"].asString(), body["profile"].asString(), params)) {
                return errorResponse(app.lastError().empty() ? "update params failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            payload["conf"] = params.confidence_threshold;
            payload["iou"] = params.iou_threshold;
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }

    HttpResponse handleSetEngine(const HttpRequest& req) {
        try {
            const Json::Value body = parseJson(req.body);
            if (!body.isMember("type") || !body["type"].isString()) {
                return errorResponse("Missing required field: type", 400);
            }

            auto descriptor = buildEngineDescriptor(body);
            if (!app.setEngine(descriptor)) {
                return errorResponse(app.lastError().empty() ? "set engine failed" : app.lastError(), 400);
            }

            Json::Value payload = successPayload();
            payload["type"] = descriptor.name;
            payload["provider"] = descriptor.provider;
            payload["device"] = descriptor.device_index;
            return jsonResponse(payload, 200);
        } catch (const std::exception& ex) {
            return errorResponse(ex.what(), 400);
        }
    }
};

RestServer::RestServer(RestServerOptions options, va::app::Application& app)
    : options_(std::move(options)), app_(app), impl_(std::make_unique<Impl>(options_, app_)) {}

RestServer::~RestServer() {
    stop();
}

bool RestServer::start() {
    return impl_ ? impl_->start() : false;
}

void RestServer::stop() {
    if (impl_) {
        impl_->stop();
    }
}

} // namespace va::server

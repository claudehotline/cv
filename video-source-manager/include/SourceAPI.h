#pragma once

#include "HTTPServer.h"
#include "VideoSourceManager.h"
#include <json/json.h>
#include <memory>
#include <mutex>
#include <map>

class SourceAPI {
public:
    SourceAPI(VideoSourceManager* manager);
    ~SourceAPI();

    // 启动/停止API服务
    bool start(int port = 8081);
    void stop();
    bool isRunning() const;

private:
    VideoSourceManager* source_manager_;
    std::unique_ptr<HTTPServer> http_server_;

    // 设置路由
    void setupRoutes();

    // API处理函数
    HTTPServer::Response getSources(const HTTPServer::Request& req);
    HTTPServer::Response addSource(const HTTPServer::Request& req);
    HTTPServer::Response updateSource(const HTTPServer::Request& req);
    HTTPServer::Response deleteSource(const HTTPServer::Request& req);
    HTTPServer::Response getSourceInfo(const HTTPServer::Request& req);

    // RTSP控制
    HTTPServer::Response startRTSP(const HTTPServer::Request& req);
    HTTPServer::Response stopRTSP(const HTTPServer::Request& req);
    HTTPServer::Response getRTSPStatus(const HTTPServer::Request& req);

    // 状态监控
    HTTPServer::Response getSourceStats(const HTTPServer::Request& req);
    HTTPServer::Response getSystemInfo(const HTTPServer::Request& req);

    // 工具方法
    Json::Value sourceConfigToJson(const VideoSourceConfig& config);
    VideoSourceConfig parseSourceFromJson(const Json::Value& json);
    Json::Value createSuccessResponse(const Json::Value& data = Json::Value());
    Json::Value createErrorResponse(const std::string& message);
};
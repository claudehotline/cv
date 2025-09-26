#pragma once

#include "HTTPServer.h"
#include "VideoAnalyzer.h"
#include <json/json.h>
#include <memory>
#include <mutex>
#include <map>

class AnalysisAPI {
public:
    AnalysisAPI(VideoAnalyzer* analyzer);
    ~AnalysisAPI();

    // 启动/停止API服务
    bool start(int port = 8082);
    void stop();
    bool isRunning() const;

private:
    VideoAnalyzer* video_analyzer_;
    std::unique_ptr<HTTPServer> http_server_;

    // 设置路由
    void setupRoutes();

    // API处理函数
    // 模型管理
    HTTPServer::Response getModels(const HTTPServer::Request& req);
    HTTPServer::Response loadModel(const HTTPServer::Request& req);
    HTTPServer::Response unloadModel(const HTTPServer::Request& req);
    HTTPServer::Response getModelInfo(const HTTPServer::Request& req);

    // 分析控制
    HTTPServer::Response startAnalysis(const HTTPServer::Request& req);
    HTTPServer::Response stopAnalysis(const HTTPServer::Request& req);
    HTTPServer::Response getAnalysisStatus(const HTTPServer::Request& req);
    HTTPServer::Response getAnalysisResults(const HTTPServer::Request& req);
    HTTPServer::Response getAnalysisTasks(const HTTPServer::Request& req);

    // 视频源管理（分析模块的视角）
    HTTPServer::Response getAnalysisSources(const HTTPServer::Request& req);
    HTTPServer::Response addAnalysisSource(const HTTPServer::Request& req);
    HTTPServer::Response removeAnalysisSource(const HTTPServer::Request& req);

    // 系统状态监控
    HTTPServer::Response getSystemInfo(const HTTPServer::Request& req);
    HTTPServer::Response getPerformanceStats(const HTTPServer::Request& req);

    // 工具方法
    Json::Value modelInfoToJson(const std::string& model_id);
    Json::Value analysisResultToJson(const std::map<std::string, cv::Rect>& result);
    Json::Value analysisTaskToJson(const std::string& task_id);
    Json::Value createSuccessResponse(const Json::Value& data = Json::Value());
    Json::Value createErrorResponse(const std::string& message);
};
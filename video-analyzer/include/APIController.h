#pragma once

#include "HTTPServer.h"
#include "VideoAnalyzer.h"
#include <json/json.h>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

// 视频源信息结构
struct VideoSourceInfo {
    std::string id;
    std::string name;
    std::string type;        // camera, file, rtsp
    std::string url;         // 源地址
    std::string status;      // active, inactive, error
    int fps;
    std::string resolution;
    bool rtsp_enabled;
    std::string rtsp_url;
};

// 分析模型信息结构
struct ModelInfo {
    std::string id;
    std::string name;
    std::string type;        // detection, segmentation
    std::string path;
    std::string status;      // loaded, unloaded
    std::string format;      // onnx, pytorch
    int64_t size_bytes;
};

// 分析任务信息结构
struct AnalysisTask {
    std::string id;
    std::string source_id;
    std::string model_id;
    AnalysisType analysis_type;
    std::string status;      // running, stopped, error
    int64_t start_time;
    int frames_processed;
    double avg_fps;
};

// 系统状态信息
struct SystemStatus {
    bool running;
    int64_t uptime_seconds;
    int active_sources;
    int running_tasks;
    double cpu_usage;
    double memory_usage;
    double gpu_usage;
    std::string version;
};

class APIController {
public:
    APIController(VideoAnalyzer* analyzer);
    ~APIController();

    // 设置HTTP服务器
    void setupRoutes(HTTPServer* server);

    // 启动/停止API服务
    bool start(int port = 8080);
    void stop();

    bool isRunning() const;

private:
    VideoAnalyzer* video_analyzer_;
    std::unique_ptr<HTTPServer> http_server_;

    // 数据存储
    mutable std::mutex sources_mutex_;
    std::map<std::string, VideoSourceInfo> video_sources_;

    mutable std::mutex models_mutex_;
    std::map<std::string, ModelInfo> available_models_;

    mutable std::mutex tasks_mutex_;
    std::map<std::string, AnalysisTask> analysis_tasks_;

    // 系统状态
    mutable std::mutex status_mutex_;
    SystemStatus system_status_;

    // API路由处理函数
    // 视频源管理
    HTTPServer::Response getSources(const HTTPServer::Request& req);
    HTTPServer::Response addSource(const HTTPServer::Request& req);
    HTTPServer::Response updateSource(const HTTPServer::Request& req);
    HTTPServer::Response deleteSource(const HTTPServer::Request& req);
    HTTPServer::Response getSourceInfo(const HTTPServer::Request& req);

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

    // 系统配置和状态
    HTTPServer::Response getSystemConfig(const HTTPServer::Request& req);
    HTTPServer::Response updateSystemConfig(const HTTPServer::Request& req);
    HTTPServer::Response getSystemStatus(const HTTPServer::Request& req);

    // 工具方法
    std::string generateId();
    Json::Value sourceInfoToJson(const VideoSourceInfo& source);
    Json::Value modelInfoToJson(const ModelInfo& model);
    Json::Value analysisTaskToJson(const AnalysisTask& task);
    Json::Value systemStatusToJson(const SystemStatus& status);

    VideoSourceInfo parseVideoSourceFromJson(const Json::Value& json);
    AnalysisType parseAnalysisType(const std::string& type_str);
    std::string analysisTypeToString(AnalysisType type);

    void updateSystemStatus();
    void initializeDefaultModels();
    void loadVideoSourcesFromConfig();
};
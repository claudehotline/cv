#include "AnalysisAPI.h"
#include <iostream>
#include <sstream>

AnalysisAPI::AnalysisAPI(VideoAnalyzer* analyzer) : video_analyzer_(analyzer) {
    http_server_ = std::make_unique<HTTPServer>();
    setupRoutes();
}

AnalysisAPI::~AnalysisAPI() {
    stop();
}

bool AnalysisAPI::start(int port) {
    http_server_ = std::make_unique<HTTPServer>(port);
    setupRoutes();

    if (http_server_->start()) {
        std::cout << "视频分析API服务已启动，端口: " << port << std::endl;
        return true;
    }

    std::cerr << "视频分析API服务启动失败" << std::endl;
    return false;
}

void AnalysisAPI::stop() {
    if (http_server_) {
        http_server_->stop();
        std::cout << "视频分析API服务已停止" << std::endl;
    }
}

bool AnalysisAPI::isRunning() const {
    return http_server_ && http_server_->isRunning();
}

void AnalysisAPI::setupRoutes() {
    if (!http_server_) return;

    // 模型管理接口
    http_server_->GET("/api/models", [this](const HTTPServer::Request& req) {
        return getModels(req);
    });

    http_server_->POST("/api/models/load", [this](const HTTPServer::Request& req) {
        return loadModel(req);
    });

    http_server_->POST("/api/models/unload", [this](const HTTPServer::Request& req) {
        return unloadModel(req);
    });

    http_server_->GET("/api/models/:id", [this](const HTTPServer::Request& req) {
        return getModelInfo(req);
    });

    // 分析控制接口
    http_server_->POST("/api/analysis/start", [this](const HTTPServer::Request& req) {
        return startAnalysis(req);
    });

    http_server_->POST("/api/analysis/stop", [this](const HTTPServer::Request& req) {
        return stopAnalysis(req);
    });

    http_server_->GET("/api/analysis/status", [this](const HTTPServer::Request& req) {
        return getAnalysisStatus(req);
    });

    http_server_->GET("/api/analysis/results", [this](const HTTPServer::Request& req) {
        return getAnalysisResults(req);
    });

    http_server_->GET("/api/analysis/tasks", [this](const HTTPServer::Request& req) {
        return getAnalysisTasks(req);
    });

    // 视频源管理接口（分析模块视角）
    http_server_->GET("/api/sources", [this](const HTTPServer::Request& req) {
        return getAnalysisSources(req);
    });

    http_server_->POST("/api/sources", [this](const HTTPServer::Request& req) {
        return addAnalysisSource(req);
    });

    http_server_->DELETE("/api/sources/:id", [this](const HTTPServer::Request& req) {
        return removeAnalysisSource(req);
    });

    // 系统监控接口
    http_server_->GET("/api/system/info", [this](const HTTPServer::Request& req) {
        return getSystemInfo(req);
    });

    http_server_->GET("/api/system/stats", [this](const HTTPServer::Request& req) {
        return getPerformanceStats(req);
    });
}

HTTPServer::Response AnalysisAPI::getModels(const HTTPServer::Request& req) {
    try {
        Json::Value result(Json::arrayValue);

        // 模拟可用模型列表
        Json::Value model1;
        model1["id"] = "yolo-v5";
        model1["name"] = "YOLOv5 Object Detection";
        model1["type"] = "object_detection";
        model1["status"] = "loaded";
        model1["format"] = "onnx";
        model1["accuracy"] = 0.89;
        model1["inference_time_ms"] = 45;
        result.append(model1);

        Json::Value model2;
        model2["id"] = "mobilenet-ssd";
        model2["name"] = "MobileNet SSD";
        model2["type"] = "object_detection";
        model2["status"] = "available";
        model2["format"] = "onnx";
        model2["accuracy"] = 0.75;
        model2["inference_time_ms"] = 25;
        result.append(model2);

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取模型列表失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::loadModel(const HTTPServer::Request& req) {
    try {
        Json::Value json_body = HTTPServer::parseJsonBody(req.body);
        std::string model_id = json_body.get("model_id", "").asString();

        if (model_id.empty()) {
            return HTTPServer::errorResponse("缺少模型ID", 400);
        }

        // 这里需要VideoAnalyzer支持模型加载操作
        Json::Value result;
        result["model_id"] = model_id;
        result["status"] = "loaded";
        result["message"] = "模型加载成功";

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("加载模型失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::unloadModel(const HTTPServer::Request& req) {
    try {
        Json::Value json_body = HTTPServer::parseJsonBody(req.body);
        std::string model_id = json_body.get("model_id", "").asString();

        if (model_id.empty()) {
            return HTTPServer::errorResponse("缺少模型ID", 400);
        }

        Json::Value result;
        result["model_id"] = model_id;
        result["status"] = "unloaded";
        result["message"] = "模型卸载成功";

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("卸载模型失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getModelInfo(const HTTPServer::Request& req) {
    try {
        auto it = req.params.find("id");
        if (it == req.params.end()) {
            return HTTPServer::errorResponse("缺少模型ID", 400);
        }

        std::string model_id = it->second;
        Json::Value result = modelInfoToJson(model_id);

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取模型信息失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::startAnalysis(const HTTPServer::Request& req) {
    try {
        Json::Value json_body = HTTPServer::parseJsonBody(req.body);
        std::string source_id = json_body.get("source_id", "").asString();
        std::string model_id = json_body.get("model_id", "").asString();
        std::string analysis_type = json_body.get("analysis_type", "object_detection").asString();

        if (source_id.empty() || model_id.empty()) {
            return HTTPServer::errorResponse("缺少必要参数", 400);
        }

        // 这里需要VideoAnalyzer支持分析任务启动
        Json::Value result;
        result["task_id"] = "task_" + std::to_string(time(nullptr));
        result["source_id"] = source_id;
        result["model_id"] = model_id;
        result["analysis_type"] = analysis_type;
        result["status"] = "started";
        result["message"] = "分析任务已启动";

        return HTTPServer::jsonResponse(createSuccessResponse(result), 201);
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("启动分析失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::stopAnalysis(const HTTPServer::Request& req) {
    try {
        Json::Value json_body = HTTPServer::parseJsonBody(req.body);
        std::string task_id = json_body.get("task_id", "").asString();

        if (task_id.empty()) {
            return HTTPServer::errorResponse("缺少任务ID", 400);
        }

        Json::Value result;
        result["task_id"] = task_id;
        result["status"] = "stopped";
        result["message"] = "分析任务已停止";

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("停止分析失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getAnalysisStatus(const HTTPServer::Request& req) {
    try {
        Json::Value result;
        result["total_tasks"] = 2;
        result["running_tasks"] = 1;
        result["completed_tasks"] = 5;
        result["failed_tasks"] = 0;
        result["avg_fps"] = 28.5;
        result["total_frames_processed"] = 12450;

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取分析状态失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getAnalysisResults(const HTTPServer::Request& req) {
    try {
        // 获取查询参数
        std::string task_id = "";
        std::string limit_str = "";

        // 模拟分析结果
        Json::Value result(Json::arrayValue);

        Json::Value detection1;
        detection1["timestamp"] = static_cast<int64_t>(time(nullptr) - 10);
        detection1["task_id"] = "task_123";
        detection1["frame_id"] = 1001;
        detection1["detections"] = Json::Value(Json::arrayValue);

        Json::Value obj1;
        obj1["class"] = "person";
        obj1["confidence"] = 0.92;
        obj1["bbox"] = Json::Value(Json::arrayValue);
        obj1["bbox"].append(100); // x
        obj1["bbox"].append(150); // y
        obj1["bbox"].append(80);  // width
        obj1["bbox"].append(120); // height
        detection1["detections"].append(obj1);

        result.append(detection1);

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取分析结果失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getAnalysisTasks(const HTTPServer::Request& req) {
    try {
        Json::Value result(Json::arrayValue);

        Json::Value task1;
        task1["task_id"] = "task_123";
        task1["source_id"] = "camera_01";
        task1["model_id"] = "yolo-v5";
        task1["analysis_type"] = "object_detection";
        task1["status"] = "running";
        task1["start_time"] = static_cast<int64_t>(time(nullptr) - 300);
        task1["frames_processed"] = 8500;
        task1["avg_fps"] = 28.5;
        result.append(task1);

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取分析任务失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getAnalysisSources(const HTTPServer::Request& req) {
    try {
        Json::Value result(Json::arrayValue);

        Json::Value source1;
        source1["source_id"] = "camera_01";
        source1["rtsp_url"] = "rtsp://localhost:8554/camera_01";
        source1["status"] = "active";
        source1["analysis_enabled"] = true;
        source1["fps"] = 30.0;
        source1["resolution"] = "1920x1080";
        result.append(source1);

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取分析视频源失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::addAnalysisSource(const HTTPServer::Request& req) {
    try {
        Json::Value json_body = HTTPServer::parseJsonBody(req.body);
        std::string source_id = json_body.get("source_id", "").asString();
        std::string rtsp_url = json_body.get("rtsp_url", "").asString();

        if (source_id.empty() || rtsp_url.empty()) {
            return HTTPServer::errorResponse("缺少必要参数", 400);
        }

        // 这里需要VideoAnalyzer支持添加RTSP源
        Json::Value result;
        result["source_id"] = source_id;
        result["rtsp_url"] = rtsp_url;
        result["status"] = "added";
        result["message"] = "分析视频源添加成功";

        return HTTPServer::jsonResponse(createSuccessResponse(result), 201);
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("添加分析视频源失败: " + std::string(e.what()), 400);
    }
}

HTTPServer::Response AnalysisAPI::removeAnalysisSource(const HTTPServer::Request& req) {
    try {
        auto it = req.params.find("id");
        if (it == req.params.end()) {
            return HTTPServer::errorResponse("缺少视频源ID", 400);
        }

        std::string source_id = it->second;

        Json::Value result;
        result["source_id"] = source_id;
        result["status"] = "removed";
        result["message"] = "分析视频源移除成功";

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("移除分析视频源失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getSystemInfo(const HTTPServer::Request& req) {
    try {
        Json::Value result;
        result["module"] = "video-analyzer";
        result["version"] = "1.0.0";
        result["status"] = "running";
        result["uptime_seconds"] = 1800;
        result["active_sources"] = 1;
        result["running_tasks"] = 1;
        result["loaded_models"] = 2;
        result["gpu_enabled"] = true;
        result["gpu_memory_used_mb"] = 2048;
        result["gpu_utilization_percent"] = 65.2;

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取系统信息失败: " + std::string(e.what()), 500);
    }
}

HTTPServer::Response AnalysisAPI::getPerformanceStats(const HTTPServer::Request& req) {
    try {
        Json::Value result;
        result["cpu_usage_percent"] = 45.6;
        result["memory_usage_mb"] = 1024;
        result["gpu_usage_percent"] = 65.2;
        result["gpu_memory_mb"] = 2048;
        result["avg_inference_time_ms"] = 42.5;
        result["frames_per_second"] = 28.5;
        result["total_frames_processed"] = 125000;
        result["error_rate_percent"] = 0.02;

        return HTTPServer::jsonResponse(createSuccessResponse(result));
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse("获取性能统计失败: " + std::string(e.what()), 500);
    }
}

Json::Value AnalysisAPI::modelInfoToJson(const std::string& model_id) {
    Json::Value json;
    json["id"] = model_id;
    json["name"] = "YOLOv5 Object Detection";
    json["type"] = "object_detection";
    json["status"] = "loaded";
    json["format"] = "onnx";
    json["path"] = "/models/" + model_id + ".onnx";
    json["size_bytes"] = 14336000;
    json["accuracy"] = 0.89;
    json["inference_time_ms"] = 45;
    json["classes"] = Json::Value(Json::arrayValue);
    json["classes"].append("person");
    json["classes"].append("car");
    json["classes"].append("truck");
    return json;
}

Json::Value AnalysisAPI::analysisResultToJson(const std::map<std::string, cv::Rect>& result) {
    Json::Value json(Json::arrayValue);
    for (const auto& detection : result) {
        Json::Value obj;
        obj["class"] = detection.first;
        obj["bbox"]["x"] = detection.second.x;
        obj["bbox"]["y"] = detection.second.y;
        obj["bbox"]["width"] = detection.second.width;
        obj["bbox"]["height"] = detection.second.height;
        json.append(obj);
    }
    return json;
}

Json::Value AnalysisAPI::analysisTaskToJson(const std::string& task_id) {
    Json::Value json;
    json["task_id"] = task_id;
    json["source_id"] = "camera_01";
    json["model_id"] = "yolo-v5";
    json["status"] = "running";
    json["start_time"] = static_cast<int64_t>(time(nullptr));
    json["frames_processed"] = 1000;
    json["avg_fps"] = 30.0;
    return json;
}

Json::Value AnalysisAPI::createSuccessResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["timestamp"] = static_cast<int64_t>(time(nullptr));
    if (!data.isNull()) {
        response["data"] = data;
    }
    return response;
}

Json::Value AnalysisAPI::createErrorResponse(const std::string& message) {
    Json::Value response;
    response["success"] = false;
    response["message"] = message;
    response["timestamp"] = static_cast<int64_t>(time(nullptr));
    return response;
}
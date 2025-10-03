#include "AnalysisAPI.h"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

Json::Value modelToJson(const DetectionModelEntry& model, uint32_t active, bool selected) {
    Json::Value entry;
    entry["id"] = model.id;
    entry["task"] = model.task;
    entry["family"] = model.family;
    entry["variant"] = model.variant;
    entry["type"] = model.type.empty() ? "onnx" : model.type;
    entry["path"] = model.path;
    entry["input_width"] = model.input_width;
    entry["input_height"] = model.input_height;
    entry["confidence_threshold"] = model.conf;
    entry["nms_threshold"] = model.iou;
    entry["active_pipelines"] = active;
    entry["active"] = selected;
    return entry;
}

Json::Value profileToJson(const ProfileEntry& profile, const AppConfigPayload& app_config) {
    Json::Value entry;
    entry["name"] = profile.name;
    entry["task"] = profile.task;
    entry["model_id"] = profile.model_id;
    entry["model_family"] = profile.model_family;
    entry["model_variant"] = profile.model_variant;
    entry["model_path"] = profile.model_path;
    entry["input_width"] = profile.input_width;
    entry["input_height"] = profile.input_height;
    Json::Value encoder;
    encoder["width"] = profile.enc_width;
    encoder["height"] = profile.enc_height;
    encoder["fps"] = profile.enc_fps;
    encoder["bitrate_kbps"] = profile.enc_bitrate_kbps;
    encoder["gop"] = profile.enc_gop;
    encoder["bframes"] = profile.enc_bframes;
    encoder["zero_latency"] = profile.enc_zero_latency;
    if (!profile.enc_preset.empty()) encoder["preset"] = profile.enc_preset;
    if (!profile.enc_tune.empty()) encoder["tune"] = profile.enc_tune;
    if (!profile.enc_profile.empty()) encoder["profile"] = profile.enc_profile;
    if (!profile.enc_codec.empty()) encoder["codec"] = profile.enc_codec;
    entry["encoder"] = encoder;

    Json::Value publish;
    auto expand = [&](const std::string& templ) {
        std::string result = templ;
        auto replace_all = [](std::string& target, const std::string& from, const std::string& to) {
            size_t pos = 0;
            while ((pos = target.find(from, pos)) != std::string::npos) {
                target.replace(pos, from.length(), to);
                pos += to.length();
            }
        };
        replace_all(result, "${whip_base}", app_config.sfu_whip_base);
        replace_all(result, "${whep_base}", app_config.sfu_whep_base);
        replace_all(result, "${stream}", "{stream}");
        return result;
    };
    if (!profile.publish_whip_template.empty()) {
        publish["whip"] = expand(profile.publish_whip_template);
    }
    if (!profile.publish_whep_template.empty()) {
        publish["whep"] = expand(profile.publish_whep_template);
    }
    entry["publish"] = publish;

    return entry;
}

} // namespace

AnalysisAPI::AnalysisAPI(va::app::Application* app)
    : app_(app) {
    http_server_ = std::make_unique<HTTPServer>();
    setupRoutes();
}

AnalysisAPI::~AnalysisAPI() {
    stop();
}

bool AnalysisAPI::start(int port) {
    if (!app_) {
        return false;
    }

    http_server_ = std::make_unique<HTTPServer>(port);
    setupRoutes();
    if (http_server_->start()) {
        return true;
    }
    return false;
}

void AnalysisAPI::stop() {
    if (http_server_) {
        http_server_->stop();
    }
}

bool AnalysisAPI::isRunning() const {
    return http_server_ && http_server_->isRunning();
}

void AnalysisAPI::setupRoutes() {
    if (!http_server_) {
        return;
    }

    http_server_->GET("/api/models", [this](const HTTPServer::Request& req) {
        return handleGetModels(req);
    });

    http_server_->GET("/api/profiles", [this](const HTTPServer::Request& req) {
        return handleGetProfiles(req);
    });

    http_server_->POST("/api/subscribe", [this](const HTTPServer::Request& req) {
        return handleSubscribe(req);
    });

    http_server_->POST("/api/unsubscribe", [this](const HTTPServer::Request& req) {
        return handleUnsubscribe(req);
    });

    http_server_->GET("/api/pipelines", [this](const HTTPServer::Request& req) {
        return handlePipelines(req);
    });

    http_server_->POST("/api/models/load", [this](const HTTPServer::Request& req) {
        return handleLoadModel(req);
    });

    // Legacy endpoints return 501 to indicate pending implementation
    http_server_->GET("/api/system/info", [this](const HTTPServer::Request& req) {
        return handleSystemInfo(req);
    });
    http_server_->GET("/api/system/stats", [this](const HTTPServer::Request& req) {
        return handleSystemStats(req);
    });
}

HTTPServer::Response AnalysisAPI::handleGetModels(const HTTPServer::Request&) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    std::unordered_map<std::string, uint32_t> usage;
    for (const auto& info : app_->pipelines()) {
        usage[info.model_id]++;
    }

    Json::Value arr(Json::arrayValue);
    for (const auto& model : app_->detectionModels()) {
        const auto it = usage.find(model.id);
        const uint32_t active = it != usage.end() ? it->second : 0;
        const bool selected = app_->isModelActive(model.id);
        arr.append(modelToJson(model, active, selected));
    }
    return HTTPServer::jsonResponse(createSuccessResponse(arr));
}

HTTPServer::Response AnalysisAPI::handleGetProfiles(const HTTPServer::Request&) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value arr(Json::arrayValue);
    const auto& cfg = app_->appConfig();
    for (const auto& profile : app_->profiles()) {
        arr.append(profileToJson(profile, cfg));
    }
    return HTTPServer::jsonResponse(createSuccessResponse(arr));
}

HTTPServer::Response AnalysisAPI::handleLoadModel(const HTTPServer::Request& req) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value body;
    try {
        body = HTTPServer::parseJsonBody(req.body);
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse(std::string("invalid json: ") + e.what(), 400);
    }

    const std::string model_id = body.get("model_id", "").asString();
    if (model_id.empty()) {
        return HTTPServer::errorResponse("model_id required", 400);
    }

    const auto& models = app_->detectionModels();
    auto it = std::find_if(models.begin(), models.end(), [&](const DetectionModelEntry& entry) {
        return entry.id == model_id;
    });
    if (it == models.end()) {
        return HTTPServer::errorResponse("model not found", 404);
    }

    if (!app_->loadModel(model_id)) {
        const std::string reason = app_->lastError();
        int status = 500;
        if (!reason.empty()) {
            if (reason.find("not found") != std::string::npos) {
                status = 404;
            } else {
                status = 400;
            }
        }
        return HTTPServer::errorResponse(reason.empty() ? "failed to activate model" : reason, status);
    }

    uint32_t active = 0;
    for (const auto& info : app_->pipelines()) {
        if (info.model_id == model_id) {
            active++;
        }
    }

    Json::Value data = modelToJson(*it, active, app_->isModelActive(model_id));
    return HTTPServer::jsonResponse(createSuccessResponse(data));
}

HTTPServer::Response AnalysisAPI::handleSubscribe(const HTTPServer::Request& req) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value body;
    try {
        body = HTTPServer::parseJsonBody(req.body);
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse(std::string("invalid json: ") + e.what(), 400);
    }

    const std::string stream = body.get("stream", "").asString();
    const std::string profile = body.get("profile", "").asString();
    const std::string uri = body.get("url", body.get("uri", "").asString()).asString();
    const std::string requested_model = body.get("model_id", "").asString();

    if (stream.empty() || profile.empty() || uri.empty()) {
        return HTTPServer::errorResponse("stream/profile/url required", 400);
    }

    std::optional<std::string> model_override;
    if (!requested_model.empty()) {
        model_override = requested_model;
    }

    auto key = app_->subscribeStream(stream, profile, uri, model_override);
    if (!key) {
        const std::string reason = app_->lastError();
        int status = 500;
        if (!reason.empty()) {
            if (reason.find("not found") != std::string::npos) {
                status = 404;
            } else if (reason.find("failed") != std::string::npos) {
                status = 500;
            } else {
                status = 400;
            }
        }
        return HTTPServer::errorResponse(reason.empty() ? "subscribe failed" : reason, status);
    }

    Json::Value data;
    data["stream"] = stream;
    data["profile"] = profile;
    data["pipeline_key"] = *key;
    data["status"] = "subscribed";

    std::string effective_model;
    for (const auto& info : app_->pipelines()) {
        if (info.key == *key) {
            effective_model = info.model_id;
            break;
        }
    }
    if (!effective_model.empty()) {
        data["model_id"] = effective_model;
    } else if (!requested_model.empty()) {
        data["model_id"] = requested_model;
    }

    return HTTPServer::jsonResponse(createSuccessResponse(data), 201);
}

HTTPServer::Response AnalysisAPI::handleUnsubscribe(const HTTPServer::Request& req) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value body;
    try {
        body = HTTPServer::parseJsonBody(req.body);
    } catch (const std::exception& e) {
        return HTTPServer::errorResponse(std::string("invalid json: ") + e.what(), 400);
    }

    const std::string stream = body.get("stream", "").asString();
    const std::string profile = body.get("profile", "").asString();

    if (stream.empty() || profile.empty()) {
        return HTTPServer::errorResponse("stream/profile required", 400);
    }

    if (!app_->unsubscribeStream(stream, profile)) {
        return HTTPServer::errorResponse("unsubscribe failed", 500);
    }

    Json::Value data;
    data["stream"] = stream;
    data["profile"] = profile;
    data["status"] = "unsubscribed";
    return HTTPServer::jsonResponse(createSuccessResponse(data));
}

HTTPServer::Response AnalysisAPI::handlePipelines(const HTTPServer::Request&) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value arr(Json::arrayValue);
    for (const auto& info : app_->pipelines()) {
        Json::Value item;
        item["key"] = info.key;
        item["stream"] = info.stream_id;
        item["profile"] = info.profile_id;
        item["source_uri"] = info.source_uri;
        item["model_id"] = info.model_id;
        item["task"] = info.task;
        item["running"] = info.running;
        item["last_active_ms"] = info.last_active_ms;
        item["track_id"] = info.track_id;
        item["metrics"]["fps"] = info.metrics.fps;
        item["metrics"]["avg_latency_ms"] = info.metrics.avg_latency_ms;
        item["metrics"]["last_processed_ms"] = info.metrics.last_processed_ms;
        item["metrics"]["processed_frames"] = static_cast<Json::UInt64>(info.metrics.processed_frames);
        item["metrics"]["dropped_frames"] = static_cast<Json::UInt64>(info.metrics.dropped_frames);
        item["transport"]["connected"] = info.transport_stats.connected;
        item["transport"]["packets"] = static_cast<Json::UInt64>(info.transport_stats.packets);
        item["transport"]["bytes"] = static_cast<Json::UInt64>(info.transport_stats.bytes);
        Json::Value encoder;
        encoder["width"] = info.encoder_cfg.width;
        encoder["height"] = info.encoder_cfg.height;
        encoder["fps"] = info.encoder_cfg.fps;
        encoder["bitrate_kbps"] = info.encoder_cfg.bitrate_kbps;
        encoder["gop"] = info.encoder_cfg.gop;
        encoder["bframes"] = info.encoder_cfg.bframes;
        encoder["zero_latency"] = info.encoder_cfg.zero_latency;
        if (!info.encoder_cfg.preset.empty()) encoder["preset"] = info.encoder_cfg.preset;
        if (!info.encoder_cfg.tune.empty()) encoder["tune"] = info.encoder_cfg.tune;
        if (!info.encoder_cfg.profile.empty()) encoder["profile"] = info.encoder_cfg.profile;
        if (!info.encoder_cfg.codec.empty()) encoder["codec"] = info.encoder_cfg.codec;
        item["encoder"] = encoder;
        arr.append(item);
    }

    return HTTPServer::jsonResponse(createSuccessResponse(arr));
}

HTTPServer::Response AnalysisAPI::handleSystemInfo(const HTTPServer::Request&) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    Json::Value data;
    const auto& cfg = app_->appConfig();
    data["engine"]["type"] = cfg.engine.type;
    data["engine"]["device"] = cfg.engine.device;
    data["sfu"]["whip_base"] = cfg.sfu_whip_base;
    data["sfu"]["whep_base"] = cfg.sfu_whep_base;
    data["ffmpeg"]["enabled"] = app_->ffmpegEnabled();
    data["models"]["total"] = static_cast<Json::UInt>(app_->detectionModels().size());
    data["profiles"]["total"] = static_cast<Json::UInt>(app_->profiles().size());

    auto pipeline_infos = app_->pipelines();
    size_t running = 0;
    for (const auto& info : pipeline_infos) {
        if (info.running) {
            running++;
        }
    }
    data["pipelines"]["total"] = static_cast<Json::UInt>(pipeline_infos.size());
    data["pipelines"]["running"] = static_cast<Json::UInt>(running);

    return HTTPServer::jsonResponse(createSuccessResponse(data));
}

HTTPServer::Response AnalysisAPI::handleSystemStats(const HTTPServer::Request&) {
    if (!app_) {
        return HTTPServer::errorResponse("application not initialized", 500);
    }

    const auto stats = app_->systemStats();
    Json::Value data;
    data["pipelines"]["total"] = static_cast<Json::UInt>(stats.total_pipelines);
    data["pipelines"]["running"] = static_cast<Json::UInt>(stats.running_pipelines);
    data["metrics"]["aggregate_fps"] = stats.aggregate_fps;
    data["metrics"]["processed_frames"] = static_cast<Json::UInt64>(stats.processed_frames);
    data["metrics"]["dropped_frames"] = static_cast<Json::UInt64>(stats.dropped_frames);
    data["transport"]["packets"] = static_cast<Json::UInt64>(stats.transport_packets);
    data["transport"]["bytes"] = static_cast<Json::UInt64>(stats.transport_bytes);

    return HTTPServer::jsonResponse(createSuccessResponse(data));
}

Json::Value AnalysisAPI::createSuccessResponse(const Json::Value& data) {
    Json::Value response;
    response["success"] = true;
    response["timestamp"] = static_cast<int64_t>(std::time(nullptr));
    if (!data.isNull()) {
        response["data"] = data;
    }
    return response;
}

Json::Value AnalysisAPI::createErrorResponse(const std::string& message) {
    Json::Value response;
    response["success"] = false;
    response["error"] = message;
    response["timestamp"] = static_cast<int64_t>(std::time(nullptr));
    return response;
}

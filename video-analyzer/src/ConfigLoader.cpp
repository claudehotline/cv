#include "ConfigLoader.h"

#include <json/json.h>

#include <algorithm>
#include <fstream>

namespace {

std::string makePath(const std::string& dir, const std::string& name) {
    if (dir.empty()) return name;
    char last = dir.back();
    if (last == '/' || last == '\\') return dir + name;
    return dir + "/" + name;
}

Json::Value loadJsonFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        return Json::Value();
    }
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, f, &root, &errs)) {
        return Json::Value();
    }
    return root;
}

DetectionModelEntry parseDetectionEntry(const Json::Value& v) {
    DetectionModelEntry entry;
    entry.id = v.get("id", v.get("model", "").asString()).asString();
    entry.family = v.get("family", "yolo").asString();
    entry.type = v.get("type", "onnx").asString();
    entry.path = v.get("path", "").asString();

    if (v.isMember("input_size") && v["input_size"].isArray() && v["input_size"].size() >= 2) {
        entry.input_width = v["input_size"][0].asInt();
        entry.input_height = v["input_size"][1].asInt();
    }

    if (v.isMember("defaults") && v["defaults"].isObject()) {
        const auto& d = v["defaults"];
        entry.conf = d.get("conf", d.get("confidence_threshold", 0.0)).asFloat();
        entry.iou = d.get("iou", d.get("nms_threshold", 0.0)).asFloat();
    }

    return entry;
}

ProfileEntry parseProfileEntry(const std::string& name, const Json::Value& v) {
    ProfileEntry entry;
    entry.name = name;
    entry.task = v.get("task", "det").asString();

    if (v.isMember("model") && v["model"].isObject()) {
        const auto& m = v["model"];
        entry.model_id = m.get("id", m.get("model", "").asString()).asString();
        entry.input_width = m.get("input_w", 0).asInt();
        entry.input_height = m.get("input_h", 0).asInt();
    }

    if (v.isMember("encoder") && v["encoder"].isObject()) {
        const auto& e = v["encoder"];
        entry.enc_width = e.get("w", 0).asInt();
        entry.enc_height = e.get("h", 0).asInt();
        entry.enc_fps = e.get("fps", 0).asInt();
        entry.enc_bitrate_kbps = e.get("bitrate_kbps", 0).asInt();
        entry.enc_gop = e.get("gop", 0).asInt();
        entry.enc_bframes = e.get("bframes", 0).asInt();
    }

    return entry;
}

AppConfigPayload parseAppConfig(const Json::Value& v) {
    AppConfigPayload payload;
    if (v.isMember("engine") && v["engine"].isObject()) {
        const auto& eng = v["engine"];
        payload.engine.type = eng.get("type", "ort-cpu").asString();
        payload.engine.device = eng.get("device", 0).asInt();
    }
    return payload;
}

}

std::vector<DetectionModelEntry> ConfigLoader::loadDetectionModels(const std::string& config_dir) {
    std::vector<DetectionModelEntry> models;
    Json::Value root = loadJsonFile(makePath(config_dir, "models.yaml"));
    if (root.isMember("detection") && root["detection"].isArray()) {
        for (const auto& v : root["detection"]) {
            DetectionModelEntry entry = parseDetectionEntry(v);
            if (!entry.id.empty() && !entry.path.empty()) {
                models.emplace_back(std::move(entry));
            }
        }
    }
    return models;
}

std::vector<ProfileEntry> ConfigLoader::loadProfiles(const std::string& config_dir) {
    std::vector<ProfileEntry> profiles;
    Json::Value root = loadJsonFile(makePath(config_dir, "profiles.yaml"));
    if (root.isMember("profiles") && root["profiles"].isObject()) {
        const auto& obj = root["profiles"];
        for (const auto& name : obj.getMemberNames()) {
            profiles.emplace_back(parseProfileEntry(name, obj[name]));
        }
    }
    return profiles;
}

AppConfigPayload ConfigLoader::loadAppConfig(const std::string& config_dir) {
    Json::Value root = loadJsonFile(makePath(config_dir, "app.yaml"));
    return parseAppConfig(root);
}

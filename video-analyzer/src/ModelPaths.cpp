#include "ModelPaths.h"
#include <json/json.h>
#include <fstream>

bool LoadModelPaths(const std::string& config_file, std::string& detection_path, std::string& segmentation_path) {
    detection_path = "model/detection_model.onnx";
    segmentation_path = "model/segmentation_model.onnx";

    std::ifstream file(config_file);
    if (!file.is_open()) {
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, file, &root, &errs)) {
        return false;
    }

    if (root.isMember("models") && root["models"].isObject()) {
        const auto& models = root["models"];
        if (models.isMember("detection") && models["detection"].isObject()) {
            const auto& det = models["detection"];
            std::string p = det.get("path", "").asString();
            if (!p.empty()) detection_path = p;
        }
        if (models.isMember("segmentation") && models["segmentation"].isObject()) {
            const auto& seg = models["segmentation"];
            std::string p = seg.get("path", "").asString();
            if (!p.empty()) segmentation_path = p;
        }
    }
    return true;
}


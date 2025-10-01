#pragma once

#include <string>
#include <vector>
#include <optional>

struct ModelConfig;
struct ProfileConfig;
struct InferenceConfig;

struct DetectionModelEntry {
    std::string id;
    std::string family;
    std::string type;
    std::string path;
    int input_width {0};
    int input_height {0};
    float conf {0.0f};
    float iou {0.0f};
};

struct ProfileEntry {
    std::string name;
    std::string task; // "det" or "seg"
    std::string model_id;
    int input_width {0};
    int input_height {0};
    int enc_width {0};
    int enc_height {0};
    int enc_fps {0};
    int enc_bitrate_kbps {0};
    int enc_gop {0};
    int enc_bframes {0};
};

struct AppEngineSpec {
    std::string type; // ort-cpu / ort-cuda / ort-trt
    int device {0};
};

struct AppConfigPayload {
    AppEngineSpec engine;
};

class ConfigLoader {
public:
    static std::vector<DetectionModelEntry> loadDetectionModels(const std::string& config_dir);
    static std::vector<ProfileEntry> loadProfiles(const std::string& config_dir);
    static AppConfigPayload loadAppConfig(const std::string& config_dir);
};

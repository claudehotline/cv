#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct ModelConfig;
struct ProfileConfig;
struct InferenceConfig;

struct DetectionModelEntry {
    std::string id;
    std::string task;   // det / seg / pose ...
    std::string family;
    std::string variant;
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
    std::string model_family;
    std::string model_variant;
    std::string model_path;
    int input_width {0};
    int input_height {0};
    int enc_width {0};
    int enc_height {0};
    int enc_fps {0};
    int enc_bitrate_kbps {0};
    int enc_gop {0};
    int enc_bframes {0};
    bool enc_zero_latency {true};
    std::string enc_preset;
    std::string enc_tune;
    std::string enc_profile;
    std::string enc_codec;
    std::string publish_whip_template;
    std::string publish_whep_template;
};

struct AnalyzerParamsEntry {
    float conf {0.0f};
    float iou {0.0f};
    std::vector<std::string> class_whitelist;
    std::optional<std::string> classes_literal;
};

struct EngineOptions {
    bool use_io_binding {false};
    bool prefer_pinned_memory {true};
    bool allow_cpu_fallback {true};
    bool enable_profiling {false};
    bool tensorrt_fp16 {false};
    bool tensorrt_int8 {false};
    int tensorrt_workspace_mb {0};
    size_t io_binding_input_bytes {0};
    size_t io_binding_output_bytes {0};
};

struct AppEngineSpec {
    std::string type; // ort-cpu / ort-cuda / ort-trt
    int device {0};
    EngineOptions options;
};

struct ObservabilityConfig {
    std::string log_level {"info"};
    bool console {true};
    std::string file_path;
    int file_max_size_kb {0};
    int file_max_files {0};
    bool pipeline_metrics_enabled {false};
    int pipeline_metrics_interval_ms {5000};
};

struct AppConfigPayload {
    AppEngineSpec engine;
    std::string sfu_whip_base;
    std::string sfu_whep_base;
    ObservabilityConfig observability;
};

class ConfigLoader {
public:
    static std::vector<DetectionModelEntry> loadDetectionModels(const std::string& config_dir);
    static std::vector<ProfileEntry> loadProfiles(const std::string& config_dir);
    static AppConfigPayload loadAppConfig(const std::string& config_dir);
    static std::map<std::string, AnalyzerParamsEntry> loadAnalyzerParams(const std::string& config_dir);
};

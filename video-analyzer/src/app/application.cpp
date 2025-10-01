#include "app/application.hpp"

#include "ConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace va::app {

Application::Application() = default;
Application::~Application() {
    shutdown();
}

bool Application::initialize(const std::string& config_dir) {
    if (initialized_) {
        return true;
    }

    config_dir_ = config_dir;

    factories_ = va::buildFactories(engine_manager_);
    pipeline_builder_ = std::make_unique<va::core::PipelineBuilder>(factories_, engine_manager_);
    track_manager_ = std::make_unique<va::core::TrackManager>(*pipeline_builder_);

    detection_models_ = ConfigLoader::loadDetectionModels(config_dir_);
    detection_model_index_.clear();
    for (const auto& model : detection_models_) {
        if (!model.id.empty()) {
            detection_model_index_.emplace(model.id, model);
        }
    }

    profiles_ = ConfigLoader::loadProfiles(config_dir_);
    profile_index_.clear();
    for (const auto& profile : profiles_) {
        if (!profile.name.empty()) {
            profile_index_.emplace(profile.name, profile);
        }
    }
    analyzer_params_ = ConfigLoader::loadAnalyzerParams(config_dir_);
    app_config_ = ConfigLoader::loadAppConfig(config_dir_);

    va::core::EngineDescriptor descriptor;
    descriptor.name = app_config_.engine.type;
    descriptor.provider = app_config_.engine.type;
    descriptor.device_index = app_config_.engine.device;
    engine_manager_.setEngine(std::move(descriptor));

    va::server::RestServerOptions rest_options;
    rest_options.host = "0.0.0.0";
    rest_options.port = 8082;
    rest_server_ = std::make_unique<va::server::RestServer>(rest_options, *track_manager_);

    initialized_ = true;
    return true;
}

bool Application::start() {
    if (!initialized_) {
        return false;
    }

    if (rest_server_) {
        return rest_server_->start();
    }
    return false;
}

void Application::shutdown() {
    if (!initialized_) {
        return;
    }

    if (rest_server_) {
        rest_server_->stop();
        rest_server_.reset();
    }

    track_manager_.reset();
    pipeline_builder_.reset();

    initialized_ = false;
}

bool Application::isInitialized() const {
    return initialized_;
}

va::core::TrackManager* Application::trackManager() {
    return track_manager_.get();
}

std::vector<va::core::TrackManager::PipelineInfo> Application::pipelines() const {
    if (!track_manager_) {
        return {};
    }
    return track_manager_->listPipelines();
}

Application::SystemStats Application::systemStats() const {
    SystemStats stats;
    for (const auto& info : pipelines()) {
        stats.total_pipelines++;
        if (info.running) {
            stats.running_pipelines++;
        }
        stats.aggregate_fps += info.metrics.fps;
        stats.processed_frames += info.metrics.processed_frames;
        stats.dropped_frames += info.metrics.dropped_frames;
        stats.transport_packets += info.transport_stats.packets;
        stats.transport_bytes += info.transport_stats.bytes;
    }
    return stats;
}

bool Application::ffmpegEnabled() const {
#ifdef USE_FFMPEG
    return true;
#else
    return false;
#endif
}

std::optional<std::string> Application::subscribeStream(const std::string& stream_id,
                                                        const std::string& profile_name,
                                                        const std::string& source_uri) {
    if (!initialized_ || !track_manager_) {
        return std::nullopt;
    }

    auto profile_it = profile_index_.find(profile_name);
    if (profile_it == profile_index_.end()) {
        return std::nullopt;
    }

    auto model_opt = resolveModel(profile_it->second);
    if (!model_opt) {
        return std::nullopt;
    }

    auto params_opt = resolveParams(profile_it->second.task);
    if (!params_opt) {
        params_opt = AnalyzerParamsEntry{};
    }

    va::core::SourceConfig source_cfg = buildSourceConfig(stream_id, source_uri);
    va::core::FilterConfig filter_cfg = buildFilterConfig(stream_id, profile_it->second, *model_opt, *params_opt);
    va::core::EncoderConfig encoder_cfg = buildEncoderConfig(profile_it->second);
    va::core::TransportConfig transport_cfg = buildTransportConfig(stream_id, profile_it->second);

    auto key = track_manager_->subscribe(source_cfg, filter_cfg, encoder_cfg, transport_cfg);
    if (key.empty()) {
        return std::nullopt;
    }
    return key;
}

bool Application::unsubscribeStream(const std::string& stream_id, const std::string& profile_name) {
    if (!initialized_ || !track_manager_) {
        return false;
    }
    track_manager_->unsubscribe(stream_id, profile_name);
    return true;
}

std::optional<DetectionModelEntry> Application::resolveModel(const ProfileEntry& profile) const {
    if (!profile.model_id.empty()) {
        auto it = detection_model_index_.find(profile.model_id);
        if (it != detection_model_index_.end()) {
            return it->second;
        }
    }

    if (!profile.model_family.empty()) {
        std::string candidate = profile.task + ":" + profile.model_family;
        if (!profile.model_variant.empty()) {
            candidate += ":" + profile.model_variant;
        }
        auto it = detection_model_index_.find(candidate);
        if (it != detection_model_index_.end()) {
            return it->second;
        }
    }

    if (!profile.model_path.empty()) {
        DetectionModelEntry entry;
        entry.id = !profile.model_id.empty() ? profile.model_id : profile.model_path;
        entry.task = profile.task;
        entry.family = profile.model_family;
        entry.variant = profile.model_variant;
        entry.path = profile.model_path;
        entry.input_width = profile.input_width;
        entry.input_height = profile.input_height;
        return entry;
    }

    return std::nullopt;
}

std::optional<AnalyzerParamsEntry> Application::resolveParams(const std::string& task) const {
    std::string key = task;
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = analyzer_params_.find(key);
    if (it != analyzer_params_.end()) {
        return it->second;
    }
    return std::nullopt;
}

va::core::SourceConfig Application::buildSourceConfig(const std::string& stream_id,
                                                     const std::string& uri) const {
    va::core::SourceConfig cfg;
    cfg.stream_id = stream_id;
    cfg.uri = uri;
    return cfg;
}

va::core::FilterConfig Application::buildFilterConfig(const std::string& stream_id,
                                                     const ProfileEntry& profile,
                                                     const DetectionModelEntry& model,
                                                     const AnalyzerParamsEntry& params) const {
    va::core::FilterConfig cfg;
    cfg.profile_id = profile.name;
    cfg.task = profile.task;
    cfg.model_id = model.id;
    cfg.model_path = !model.path.empty() ? model.path : profile.model_path;

    cfg.input_width = profile.input_width > 0 ? profile.input_width : model.input_width;
    cfg.input_height = profile.input_height > 0 ? profile.input_height : model.input_height;

    cfg.confidence_threshold = model.conf > 0.0f ? model.conf : params.conf;
    cfg.iou_threshold = model.iou > 0.0f ? model.iou : params.iou;

    if (cfg.input_width == 0) {
        cfg.input_width = 640;
    }
    if (cfg.input_height == 0) {
        cfg.input_height = 640;
    }

    (void)stream_id; // reserved for future customization
    return cfg;
}

va::core::EncoderConfig Application::buildEncoderConfig(const ProfileEntry& profile) const {
    va::core::EncoderConfig cfg;
    cfg.width = profile.enc_width;
    cfg.height = profile.enc_height;
    cfg.fps = profile.enc_fps;
    cfg.bitrate_kbps = profile.enc_bitrate_kbps;
    cfg.gop = profile.enc_gop;
    cfg.bframes = profile.enc_bframes;
    cfg.preset = profile.enc_preset;
    cfg.tune = profile.enc_tune;
    cfg.profile = profile.enc_profile;
    cfg.codec = profile.enc_codec.empty() ? "h264" : profile.enc_codec;
    cfg.zero_latency = profile.enc_zero_latency;
    return cfg;
}

va::core::TransportConfig Application::buildTransportConfig(const std::string& stream_id,
                                                            const ProfileEntry& profile) const {
    va::core::TransportConfig cfg;
    cfg.whip_url = expandTemplate(profile.publish_whip_template, stream_id);
    return cfg;
}

std::string Application::expandTemplate(const std::string& templ,
                                        const std::string& stream_id) const {
    std::string result = templ;
    auto replace_all = [](std::string& target, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = target.find(from, pos)) != std::string::npos) {
            target.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replace_all(result, "${stream}", stream_id);
    replace_all(result, "${whip_base}", app_config_.sfu_whip_base);
    replace_all(result, "${whep_base}", app_config_.sfu_whep_base);
    return result;
}

} // namespace va::app

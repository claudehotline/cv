#include "core/pipeline_builder.hpp"

#include "analyzer/analyzer.hpp"
#include "media/encoder.hpp"
#include "media/source.hpp"
#include "media/transport.hpp"

#include "core/logger.hpp"

namespace va::core {

PipelineBuilder::PipelineBuilder(const Factories& factories, EngineManager& engine_manager)
    : factories_(factories), engine_manager_(engine_manager) {}

std::shared_ptr<Pipeline> PipelineBuilder::build(const SourceConfig& source_cfg,
                                                 const FilterConfig& filter_cfg,
                                                 const EncoderConfig& encoder_cfg,
                                                 const TransportConfig& transport_cfg) const {
    auto source = factories_.make_source ? factories_.make_source(source_cfg) : nullptr;
    auto analyzer = factories_.make_filter ? factories_.make_filter(filter_cfg) : nullptr;
    auto encoder = factories_.make_encoder ? factories_.make_encoder(encoder_cfg) : nullptr;
    auto transport = factories_.make_transport ? factories_.make_transport(transport_cfg) : nullptr;

    if (!source) {
        VA_LOG_ERROR() << "[PipelineBuilder] failed to create source for URI " << source_cfg.uri;
        return nullptr;
    }
    if (!analyzer) {
        VA_LOG_ERROR() << "[PipelineBuilder] failed to create analyzer for model " << filter_cfg.model_id;
        return nullptr;
    }
    if (!encoder) {
        VA_LOG_ERROR() << "[PipelineBuilder] failed to create encoder for stream " << source_cfg.stream_id;
        return nullptr;
    }
    if (!transport) {
        VA_LOG_ERROR() << "[PipelineBuilder] failed to create transport for stream " << source_cfg.stream_id;
        return nullptr;
    }

    (void)engine_manager_; // future use for binding execution providers

    va::media::IEncoder::Settings encoder_settings;
    encoder_settings.width = encoder_cfg.width;
    encoder_settings.height = encoder_cfg.height;
    encoder_settings.fps = encoder_cfg.fps;
    encoder_settings.bitrate_kbps = encoder_cfg.bitrate_kbps;
    encoder_settings.gop = encoder_cfg.gop;
    encoder_settings.bframes = encoder_cfg.bframes;
    encoder_settings.zero_latency = encoder_cfg.zero_latency;
    encoder_settings.preset = encoder_cfg.preset;
    encoder_settings.tune = encoder_cfg.tune;
    encoder_settings.profile = encoder_cfg.profile;
    encoder_settings.codec = encoder_cfg.codec;

    if (!encoder->open(encoder_settings)) {
        VA_LOG_ERROR() << "[PipelineBuilder] encoder open failed for stream " << source_cfg.stream_id
                       << " (" << encoder_settings.width << "x" << encoder_settings.height << "@" << encoder_settings.fps
                       << ", codec=" << encoder_settings.codec << ")";
        return nullptr;
    }

    const std::string endpoint = transport_cfg.whip_url.empty() ? std::string() : transport_cfg.whip_url;
    if (!transport->connect(endpoint)) {
        VA_LOG_ERROR() << "[PipelineBuilder] transport connect failed";
        return nullptr;
    }

    auto pipeline = std::make_shared<Pipeline>(std::move(source),
                                               std::move(analyzer),
                                               std::move(encoder),
                                               std::move(transport),
                                               source_cfg.stream_id,
                                               filter_cfg.profile_id);
    return pipeline;
}

} // namespace va::core

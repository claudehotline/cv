#pragma once

#include <functional>
#include <memory>
#include <string>

namespace va::media {
class ISwitchableSource;
class IEncoder;
class ITransport;
}

namespace va::analyzer {
class Analyzer;
}

namespace va::core {

struct SourceConfig {
    std::string stream_id;
    std::string uri;
};

struct FilterConfig {
    std::string profile_id;
    std::string task;
    std::string model_id;
    std::string model_path;
    int input_width {0};
    int input_height {0};
    float confidence_threshold {0.0f};
    float iou_threshold {0.0f};
};

struct EncoderConfig {
    int width {0};
    int height {0};
    int fps {0};
    int bitrate_kbps {0};
    int gop {0};
    bool zero_latency {true};
    int bframes {0};
    std::string preset;
    std::string tune;
    std::string profile;
    std::string codec {"h264"};
};

struct TransportConfig {
    std::string whip_url;
};

struct Factories {
    std::function<std::shared_ptr<va::media::ISwitchableSource>(const SourceConfig&)> make_source;
    std::function<std::shared_ptr<va::analyzer::Analyzer>(const FilterConfig&)> make_filter;
    std::function<std::shared_ptr<va::media::IEncoder>(const EncoderConfig&)> make_encoder;
    std::function<std::shared_ptr<va::media::ITransport>(const TransportConfig&)> make_transport;
};

} // namespace va::core

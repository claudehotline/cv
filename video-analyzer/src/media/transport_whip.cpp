#include "media/transport_whip.hpp"

namespace va::media {

WhipTransport::WhipTransport() = default;

bool WhipTransport::connect(const std::string& endpoint) {
    endpoint_ = endpoint;
    connected_ = true;
    stats_ = {};
    stats_.connected = true;
    return true;
}

bool WhipTransport::send(const std::string& /*track_id*/, const uint8_t* data, size_t size) {
    if (!connected_) {
        return false;
    }
    stats_.packets += 1;
    stats_.bytes += static_cast<uint64_t>(size);
    return true;
}

void WhipTransport::disconnect() {
    connected_ = false;
    stats_.connected = false;
}

ITransport::Stats WhipTransport::stats() const {
    auto snapshot = stats_;
    snapshot.connected = connected_;
    return snapshot;
}

} // namespace va::media


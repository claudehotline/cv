#include "media/transport_whip.hpp"

namespace va::media {

WhipTransport::WhipTransport() = default;

bool WhipTransport::connect(const std::string& endpoint) {
    endpoint_ = endpoint;
    connected_ = true;
    sent_packets_ = 0;
    sent_bytes_ = 0;
    return true;
}

bool WhipTransport::send(const std::string& /*track_id*/, const uint8_t* data, size_t size) {
    if (!connected_) {
        return false;
    }
    sent_packets_++;
    sent_bytes_ += static_cast<uint64_t>(size);
    (void)data;
    return true;
}

void WhipTransport::disconnect() {
    connected_ = false;
}

ITransport::Stats WhipTransport::stats() const {
    Stats s;
    s.connected = connected_;
    s.packets = sent_packets_;
    s.bytes = sent_bytes_;
    return s;
}

} // namespace va::media

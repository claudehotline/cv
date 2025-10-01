#pragma once

#include "media/transport.hpp"

namespace va::media {

class WhipTransport : public ITransport {
public:
    WhipTransport();

    bool connect(const std::string& endpoint) override;
    bool send(const std::string& track_id, const uint8_t* data, size_t size) override;
    void disconnect() override;
    Stats stats() const override;

private:
    std::string endpoint_;
    bool connected_ {false};
    uint64_t sent_packets_ {0};
    uint64_t sent_bytes_ {0};
};

} // namespace va::media

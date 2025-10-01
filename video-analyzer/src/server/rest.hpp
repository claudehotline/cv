#pragma once

#include <functional>
#include <string>

namespace va::core {
class TrackManager;
}

namespace va::server {

struct RestServerOptions {
    std::string host {"0.0.0.0"};
    int port {8082};
};

class RestServer {
public:
    RestServer(RestServerOptions options, va::core::TrackManager& track_manager);

    bool start();
    void stop();

private:
    RestServerOptions options_;
    va::core::TrackManager& track_manager_;
    bool running_ {false};
};

} // namespace va::server

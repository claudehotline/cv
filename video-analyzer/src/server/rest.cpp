#include "server/rest.hpp"

namespace va::server {

RestServer::RestServer(RestServerOptions options, va::core::TrackManager& track_manager)
    : options_(std::move(options)), track_manager_(track_manager) {}

bool RestServer::start() {
    running_ = true;
    return true;
}

void RestServer::stop() {
    running_ = false;
}

} // namespace va::server

#pragma once

#include <memory>
#include <string>

namespace va::app {
class Application;
}

namespace va::server {

struct RestServerOptions {
    std::string host {"0.0.0.0"};
    int port {8082};
};

class RestServer {
public:
    RestServer(RestServerOptions options, va::app::Application& app);
    ~RestServer();

    bool start();
    void stop();

private:
    struct Impl;
    RestServerOptions options_;
    va::app::Application& app_;
    std::unique_ptr<Impl> impl_;
};

} // namespace va::server

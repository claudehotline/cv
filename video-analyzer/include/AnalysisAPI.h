#pragma once

#include "HTTPServer.h"
#include "app/application.hpp"

#include <json/json.h>
#include <memory>

class AnalysisAPI {
public:
    explicit AnalysisAPI(va::app::Application* app);
    ~AnalysisAPI();

    bool start(int port = 8082);
    void stop();
    bool isRunning() const;

private:
    va::app::Application* app_;
    std::unique_ptr<HTTPServer> http_server_;

    void setupRoutes();

    HTTPServer::Response notImplemented(const std::string& feature);
    HTTPServer::Response handleGetModels(const HTTPServer::Request& req);
    HTTPServer::Response handleGetProfiles(const HTTPServer::Request& req);
    HTTPServer::Response handleLoadModel(const HTTPServer::Request& req);
    HTTPServer::Response handleSubscribe(const HTTPServer::Request& req);
    HTTPServer::Response handleUnsubscribe(const HTTPServer::Request& req);
    HTTPServer::Response handlePipelines(const HTTPServer::Request& req);
    HTTPServer::Response handleSystemInfo(const HTTPServer::Request& req);
    HTTPServer::Response handleSystemStats(const HTTPServer::Request& req);

    Json::Value createSuccessResponse(const Json::Value& data = Json::Value());
    Json::Value createErrorResponse(const std::string& message);
};

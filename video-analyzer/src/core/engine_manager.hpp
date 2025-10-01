#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace va::core {

struct EngineDescriptor {
    std::string name;
    std::string provider;
    int device_index {0};
    std::unordered_map<std::string, std::string> options;
};

class EngineManager {
public:
    EngineManager();

    bool setEngine(EngineDescriptor descriptor);
    EngineDescriptor currentEngine() const;
    bool prewarm(const std::string& model_path);

private:
    mutable std::mutex mutex_;
    EngineDescriptor current_;
};

} // namespace va::core

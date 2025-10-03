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

struct EngineRuntimeStatus {
    std::string provider {"cpu"};
    bool gpu_active {false};
    bool io_binding {false};
    bool device_binding {false};
    bool cpu_fallback {false};
};

class EngineManager {
public:
    EngineManager();

    bool setEngine(EngineDescriptor descriptor);
    EngineDescriptor currentEngine() const;
    bool prewarm(const std::string& model_path);

    void updateRuntimeStatus(EngineRuntimeStatus status);
    EngineRuntimeStatus currentRuntimeStatus() const;

private:
    mutable std::mutex mutex_;
    EngineDescriptor current_;
    EngineRuntimeStatus runtime_status_;
};

} // namespace va::core

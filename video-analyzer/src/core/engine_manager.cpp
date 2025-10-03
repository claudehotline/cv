#include "core/engine_manager.hpp"

#include <utility>

namespace va::core {

EngineManager::EngineManager() = default;

bool EngineManager::setEngine(EngineDescriptor descriptor) {
    std::scoped_lock lock(mutex_);
    current_ = std::move(descriptor);
    runtime_status_.provider = current_.provider.empty() ? current_.name : current_.provider;
    runtime_status_.gpu_active = false;
    runtime_status_.io_binding = false;
    runtime_status_.device_binding = false;
    runtime_status_.cpu_fallback = false;
    return true;
}

EngineDescriptor EngineManager::currentEngine() const {
    std::scoped_lock lock(mutex_);
    return current_;
}

bool EngineManager::prewarm(const std::string& /*model_path*/) {
    // TODO: integrate ONNX/TensorRT prewarm in later stages
    return true;
}

void EngineManager::updateRuntimeStatus(EngineRuntimeStatus status) {
    std::scoped_lock lock(mutex_);
    runtime_status_ = std::move(status);
}

EngineRuntimeStatus EngineManager::currentRuntimeStatus() const {
    std::scoped_lock lock(mutex_);
    return runtime_status_;
}

} // namespace va::core

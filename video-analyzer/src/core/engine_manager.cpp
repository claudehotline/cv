#include "core/engine_manager.hpp"

#include <utility>

namespace va::core {

EngineManager::EngineManager() = default;

bool EngineManager::setEngine(EngineDescriptor descriptor) {
    std::scoped_lock lock(mutex_);
    current_ = std::move(descriptor);
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

} // namespace va::core

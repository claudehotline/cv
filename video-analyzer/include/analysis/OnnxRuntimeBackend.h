#pragma once

#ifdef USE_ONNXRUNTIME

#include <memory>
#include <string>
#include <vector>
#include <onnxruntime_cxx_api.h>

#include "analysis/Backend.h"

// Inference device types
enum class InferenceDevice {
    CPU,
    CUDA
};

// Inference configuration
struct InferenceConfig {
    InferenceDevice device = InferenceDevice::CPU;
    int cuda_device_id = 0;
    int num_threads = 4;
    size_t gpu_mem_limit = 2ULL * 1024 * 1024 * 1024;  // 2GB default
    bool enable_profiling = false;  // 启用时会显示详细的算子分配和 fallback 信息
    bool use_arena = true;
};

class OnnxRuntimeBackend : public IInferenceBackend {
public:
    OnnxRuntimeBackend();
    ~OnnxRuntimeBackend() override;

    bool loadModel(const std::string& model_path) override;
    bool infer(const std::vector<Tensor>& inputs, std::vector<Tensor>& outputs) override;

    std::vector<std::string> getInputNames() const override { return input_names_; }
    std::vector<std::string> getOutputNames() const override { return output_names_; }

    // GPU configuration methods
    void setInferenceConfig(const InferenceConfig& config);
    InferenceConfig getInferenceConfig() const { return config_; }

    // Static method to detect available devices
    static std::vector<InferenceDevice> getAvailableDevices();
    static std::string deviceToString(InferenceDevice device);

    // Diagnostic methods
    void enableVerboseLogging(bool enable = true);
    std::string getExecutionProviderInfo() const;

private:
    std::unique_ptr<Ort::Env> env_;
    std::unique_ptr<Ort::Session> session_;
    std::unique_ptr<Ort::SessionOptions> session_options_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;

    InferenceConfig config_;

    // Device configuration methods
    void configureCUDA();
    void configureCPU();
};

#endif // USE_ONNXRUNTIME


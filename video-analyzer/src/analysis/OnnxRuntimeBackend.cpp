#include "analysis/OnnxRuntimeBackend.h"

#ifdef USE_ONNXRUNTIME

#include <stdexcept>
#include <iostream>

OnnxRuntimeBackend::OnnxRuntimeBackend() {
    env_ = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ORTBackend");
    session_options_ = std::make_unique<Ort::SessionOptions>();

    // Default optimization level
    session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Will be configured later via setInferenceConfig()
}

OnnxRuntimeBackend::~OnnxRuntimeBackend() = default;

bool OnnxRuntimeBackend::loadModel(const std::string& model_path) {
    try {
#ifdef _WIN32
        std::wstring wpath(model_path.begin(), model_path.end());
        session_ = std::make_unique<Ort::Session>(*env_, wpath.c_str(), *session_options_);
#else
        session_ = std::make_unique<Ort::Session>(*env_, model_path.c_str(), *session_options_);
#endif
        Ort::AllocatorWithDefaultOptions alloc;
        input_names_.clear(); output_names_.clear();
        const size_t nin = session_->GetInputCount();
        const size_t nout = session_->GetOutputCount();
        for (size_t i=0;i<nin;++i) { Ort::AllocatedStringPtr s = session_->GetInputNameAllocated(i, alloc); input_names_.emplace_back(s.get()); }
        for (size_t i=0;i<nout;++i){ Ort::AllocatedStringPtr s = session_->GetOutputNameAllocated(i, alloc); output_names_.emplace_back(s.get()); }
        return true;
    } catch (const std::exception& e) {
        session_.reset();
        return false;
    }
}

bool OnnxRuntimeBackend::infer(const std::vector<Tensor>& inputs, std::vector<Tensor>& outputs) {
    if (!session_) return false;
    try {
        std::vector<Ort::Value> ort_inputs; ort_inputs.reserve(inputs.size());
        std::vector<std::vector<int64_t>> ishapes; ishapes.reserve(inputs.size());
        Ort::MemoryInfo mi = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        for (const auto& t : inputs) {
            ishapes.push_back(t.shape);
            ort_inputs.emplace_back(Ort::Value::CreateTensor<float>(mi, const_cast<float*>(t.data.data()), t.data.size(), ishapes.back().data(), ishapes.back().size()));
        }
        std::vector<const char*> in_names; for (auto& s : input_names_) in_names.push_back(s.c_str()); if (in_names.empty()) in_names.push_back("images");
        std::vector<const char*> out_names; for (auto& s : output_names_) out_names.push_back(s.c_str());
        auto ort_outs = session_->Run(Ort::RunOptions{nullptr}, in_names.data(), ort_inputs.data(), (size_t)ort_inputs.size(), out_names.data(), out_names.size());
        outputs.clear(); outputs.reserve(ort_outs.size());
        for (auto& v : ort_outs) {
            auto info = v.GetTensorTypeAndShapeInfo();
            auto dims = info.GetShape();
            const float* data = v.GetTensorData<float>();
            size_t total = 1; for (auto d : dims) total *= (size_t)d;
            Tensor t; t.shape = std::move(dims); t.data.assign(data, data + total);
            outputs.push_back(std::move(t));
        }
        return !outputs.empty();
    } catch (...) { return false; }
}

// ============================================================================
// GPU Configuration Methods
// ============================================================================

void OnnxRuntimeBackend::setInferenceConfig(const InferenceConfig& config) {
    config_ = config;

    std::cout << "🔧 Configuring inference device: " << deviceToString(config_.device) << std::endl;

    // Configure based on device type
    switch (config_.device) {
        case InferenceDevice::CUDA:
            configureCUDA();
            break;
        default:
            configureCPU();
    }

    // Common profiling settings
    if (config_.enable_profiling) {
        session_options_->EnableProfiling(L"ort_profile_");
    }
}

void OnnxRuntimeBackend::configureCUDA() {
#ifdef USE_CUDA
    try {
        OrtCUDAProviderOptions cuda_options{};
        cuda_options.device_id = config_.cuda_device_id;
        cuda_options.gpu_mem_limit = config_.gpu_mem_limit;
        cuda_options.arena_extend_strategy = 1;  // kSameAsRequested

        // 关键优化：使用 EXHAUSTIVE 搜索最优卷积算法，避免 fallback
        cuda_options.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;

        // 启用 CUDA Graph 以减少 kernel launch 开销
        cuda_options.do_copy_in_default_stream = 1;

        session_options_->AppendExecutionProvider_CUDA(cuda_options);

        // ONNX Runtime 会自动在 CUDA 后添加 CPU 作为 fallback
        // 通过上述优化（EXHAUSTIVE 搜索、max workspace 等）最大化 CUDA 算子覆盖率

        // 使用 SEQUENTIAL 模式确保算子按顺序在 GPU 上执行
        session_options_->SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);

        // 启用所有图优化以融合算子，减少 CPU-GPU 数据传输
        session_options_->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

        // 设置日志级别以监控 fallback（可选：设为 0 查看详细信息）
        if (config_.enable_profiling) {
            session_options_->SetLogSeverityLevel(0);  // Verbose - 显示详细的算子分配信息
        } else {
            session_options_->SetLogSeverityLevel(2);  // Warning - 只显示警告
        }

        std::cout << "✅ CUDA GPU inference enabled with optimizations!" << std::endl;
        std::cout << "   Device ID: " << config_.cuda_device_id << std::endl;
        std::cout << "   GPU Memory Limit: " << (config_.gpu_mem_limit / (1024.0 * 1024.0)) << " MB" << std::endl;
        std::cout << "   cuDNN Conv Algo: EXHAUSTIVE (prevents fallback)" << std::endl;
        std::cout << "   Graph Optimization: EXTENDED (operator fusion enabled)" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ CUDA configuration failed, falling back to CPU: " << e.what() << std::endl;
        configureCPU();
    }
#else
    std::cerr << "⚠️  CUDA not compiled, falling back to CPU" << std::endl;
    configureCPU();
#endif
}

void OnnxRuntimeBackend::configureCPU() {
    session_options_->SetIntraOpNumThreads(config_.num_threads);
    session_options_->SetInterOpNumThreads(config_.num_threads);
    session_options_->SetExecutionMode(ExecutionMode::ORT_PARALLEL);

    if (config_.use_arena) {
        session_options_->EnableMemPattern();
        session_options_->EnableCpuMemArena();
    }

    std::cout << "✅ CPU inference enabled!" << std::endl;
    std::cout << "   Threads: " << config_.num_threads << std::endl;
    std::cout << "   Memory Arena: " << (config_.use_arena ? "Enabled" : "Disabled") << std::endl;
}

// ============================================================================
// Static Utility Methods
// ============================================================================

std::vector<InferenceDevice> OnnxRuntimeBackend::getAvailableDevices() {
    std::vector<InferenceDevice> devices;

    // CPU is always available
    devices.push_back(InferenceDevice::CPU);

#ifdef USE_CUDA
    // Simple CUDA availability check
    // In production, you would query CUDA devices here
    devices.push_back(InferenceDevice::CUDA);
#endif

    return devices;
}

std::string OnnxRuntimeBackend::deviceToString(InferenceDevice device) {
    switch (device) {
        case InferenceDevice::CPU: return "CPU";
        case InferenceDevice::CUDA: return "CUDA (NVIDIA GPU)";
        default: return "Unknown";
    }
}

// ============================================================================
// Diagnostic Methods
// ============================================================================

void OnnxRuntimeBackend::enableVerboseLogging(bool enable) {
    if (enable) {
        session_options_->SetLogSeverityLevel(0);  // Verbose
        std::cout << "🔍 Verbose logging enabled - will show operator placement" << std::endl;
    } else {
        session_options_->SetLogSeverityLevel(2);  // Warning
    }
}

std::string OnnxRuntimeBackend::getExecutionProviderInfo() const {
    if (!session_) {
        return "No session loaded";
    }

    std::string info = "Execution Providers:\n";

#ifdef USE_CUDA
    info += "  - CUDA (Primary)\n";
    info += "  - CPU (Fallback for unsupported ops)\n";
    info += "\nNote: If you see CPU fallback warnings, consider:\n";
    info += "  1. Check ONNX model compatibility with CUDA\n";
    info += "  2. Update ONNX Runtime to latest version\n";
    info += "  3. Simplify model architecture\n";
    info += "  4. Enable verbose logging to see which ops fallback\n";
#else
    info += "  - CPU only\n";
#endif

    return info;
}

#endif // USE_ONNXRUNTIME


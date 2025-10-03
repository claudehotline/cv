#include "analyzer/ort_session.hpp"

#include "core/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

#ifdef USE_ONNXRUNTIME
#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>
#endif

namespace va::analyzer {

#ifdef USE_ONNXRUNTIME

namespace {
inline std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}
} // namespace

struct OrtModelSession::Impl {
    Options options;
    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::SessionOptions> session_options;
    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::IoBinding> io_binding;
    std::vector<std::string> input_names_storage;
    std::vector<const char*> input_names;
    std::vector<std::string> output_names_storage;
    std::vector<const char*> output_names;
    std::vector<Ort::Value> last_outputs;
    bool use_gpu {false};
    std::mutex mutex;
};

OrtModelSession::OrtModelSession() = default;
OrtModelSession::~OrtModelSession() = default;

void OrtModelSession::setOptions(const Options& options) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }
    std::scoped_lock lock(impl_->mutex);
    impl_->options = options;
}

bool OrtModelSession::loadModel(const std::string& model_path, bool use_gpu) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    std::scoped_lock lock(impl_->mutex);

    if (!impl_->env) {
        impl_->env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "VA_ONNX");
    }

    impl_->session_options = std::make_unique<Ort::SessionOptions>();
    impl_->session_options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    impl_->session_options->SetIntraOpNumThreads(1);

    if (impl_->options.enable_profiling) {
        impl_->session_options->EnableProfiling(L"ort_profile_");
    }

    std::string provider = toLower(impl_->options.provider);
    impl_->use_gpu = use_gpu || provider == "cuda" || provider == "gpu" || provider == "tensorrt";

    bool provider_appended = false;
    try {
        if (provider == "tensorrt") {
#if defined(USE_CUDA)
            VA_LOG_WARN() << "TensorRT provider requested but TensorRT support is not wired in this build. Falling back to CUDA provider.";
            provider = "cuda";
#else
            VA_LOG_WARN() << "TensorRT provider requested but CUDA support is not compiled. Falling back to CPU.";
#endif
        }

        if (!provider_appended && (impl_->use_gpu || provider == "cuda")) {
#if defined(USE_CUDA)
            OrtCUDAProviderOptions cuda_opts{};
            cuda_opts.device_id = impl_->options.device_id;
            cuda_opts.gpu_mem_limit = SIZE_MAX;
            cuda_opts.arena_extend_strategy = 1;
            cuda_opts.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchExhaustive;
            cuda_opts.do_copy_in_default_stream = 1;
            impl_->session_options->AppendExecutionProvider_CUDA(cuda_opts);
            provider_appended = true;
            impl_->use_gpu = true;
#else
            VA_LOG_WARN() << "CUDA provider requested but CUDA support is not compiled. Falling back to CPU.";
            impl_->use_gpu = false;
#endif
        }
    } catch (const std::exception& ex) {
        VA_LOG_WARN() << "Failed to configure requested execution provider: " << ex.what();
        provider_appended = false;
        impl_->use_gpu = false;
    }

    if (!provider_appended && !impl_->options.allow_cpu_fallback && impl_->use_gpu) {
        VA_LOG_ERROR() << "Execution provider configuration failed and CPU fallback disabled.";
        loaded_ = false;
        return false;
    }

    if (!provider_appended) {
        impl_->use_gpu = false;
    }

    try {
#ifdef _WIN32
        std::wstring wide_path(model_path.begin(), model_path.end());
        impl_->session = std::make_unique<Ort::Session>(*impl_->env, wide_path.c_str(), *impl_->session_options);
#else
        impl_->session = std::make_unique<Ort::Session>(*impl_->env, model_path.c_str(), *impl_->session_options);
#endif
    } catch (const Ort::Exception& ex) {
        VA_LOG_ERROR() << "ONNX Runtime failed to load model: " << ex.what();
        impl_->session.reset();
        loaded_ = false;
        return false;
    }

    Ort::AllocatorWithDefaultOptions allocator;
    impl_->input_names_storage.clear();
    impl_->input_names.clear();
    size_t input_count = impl_->session->GetInputCount();
    impl_->input_names_storage.reserve(input_count);
    impl_->input_names.reserve(input_count);
    for (size_t i = 0; i < input_count; ++i) {
        Ort::AllocatedStringPtr name = impl_->session->GetInputNameAllocated(i, allocator);
        impl_->input_names_storage.emplace_back(name.get());
        impl_->input_names.emplace_back(impl_->input_names_storage.back().c_str());
    }

    impl_->output_names_storage.clear();
    impl_->output_names.clear();
    size_t output_count = impl_->session->GetOutputCount();
    impl_->output_names_storage.reserve(output_count);
    impl_->output_names.reserve(output_count);
    for (size_t i = 0; i < output_count; ++i) {
        Ort::AllocatedStringPtr name = impl_->session->GetOutputNameAllocated(i, allocator);
        impl_->output_names_storage.emplace_back(name.get());
        impl_->output_names.emplace_back(impl_->output_names_storage.back().c_str());
    }

    if (impl_->options.use_io_binding && impl_->use_gpu) {
        try {
            impl_->io_binding = std::make_unique<Ort::IoBinding>(*impl_->session);
            VA_LOG_INFO() << "OrtModelSession IoBinding enabled (provider="
                          << (provider_appended ? provider : "cpu")
                          << ")";
        } catch (const std::exception& ex) {
            VA_LOG_WARN() << "Failed to initialize IoBinding: " << ex.what();
            impl_->io_binding.reset();
        }
    } else {
        impl_->io_binding.reset();
    }

    loaded_ = true;
    return true;
}

namespace {
core::TensorView makeTensorView(Ort::Value& value, bool on_gpu) {
    core::TensorView view;
    if (!value.IsTensor()) {
        return view;
    }

    Ort::TensorTypeAndShapeInfo shape_info = value.GetTensorTypeAndShapeInfo();
    view.shape = shape_info.GetShape();
    view.dtype = core::DType::F32;
    view.on_gpu = on_gpu;
    view.data = value.GetTensorMutableData<float>();
    return view;
}
}

bool OrtModelSession::run(const core::TensorView& input, std::vector<core::TensorView>& outputs) {
    if (!loaded_ || !impl_ || !impl_->session) {
        return false;
    }

    if (!input.data || input.shape.empty()) {
        return false;
    }

    std::scoped_lock lock(impl_->mutex);

    const size_t element_count = std::accumulate(input.shape.begin(), input.shape.end(), static_cast<size_t>(1), std::multiplies<size_t>());
    if (element_count == 0) {
        return false;
    }

    if (input.dtype != core::DType::F32) {
        VA_LOG_WARN() << "OrtModelSession only supports F32 tensors currently.";
        return false;
    }

    try {
        if (impl_->io_binding) {
            impl_->io_binding->ClearBoundInputs();
            impl_->io_binding->ClearBoundOutputs();

            std::vector<Ort::Value> input_holders;
            input_holders.reserve(1);

            Ort::MemoryInfo input_mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            if (impl_->options.use_io_binding && impl_->use_gpu && impl_->options.prefer_pinned_memory) {
                input_mem = Ort::MemoryInfo("CudaPinned", OrtDeviceAllocator, impl_->options.device_id, OrtMemTypeCPU);
            }

            input_holders.emplace_back(Ort::Value::CreateTensor<float>(
                input_mem,
                static_cast<float*>(input.data),
                element_count,
                const_cast<int64_t*>(input.shape.data()),
                input.shape.size()));

            const char* input_name = impl_->input_names.empty() ? "input" : impl_->input_names.front();
            impl_->io_binding->BindInput(input_name, input_holders.front());

            for (const char* output_name : impl_->output_names) {
                Ort::MemoryInfo out_mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
                if (impl_->options.use_io_binding && impl_->use_gpu && impl_->options.prefer_pinned_memory) {
                    out_mem = Ort::MemoryInfo("CudaPinned", OrtDeviceAllocator, impl_->options.device_id, OrtMemTypeCPUOutput);
                }
                impl_->io_binding->BindOutput(output_name, out_mem);
            }

            Ort::RunOptions run_opts;
            impl_->session->Run(run_opts, *impl_->io_binding);
            impl_->io_binding->SynchronizeOutputs();

            impl_->last_outputs = impl_->io_binding->GetOutputValues();
            outputs.clear();
            outputs.reserve(impl_->last_outputs.size());
            for (auto& value : impl_->last_outputs) {
                outputs.emplace_back(makeTensorView(value, false));
            }

            impl_->io_binding->ClearBoundInputs();
            impl_->io_binding->ClearBoundOutputs();
        } else {
            Ort::MemoryInfo input_mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                input_mem,
                static_cast<float*>(input.data),
                element_count,
                const_cast<int64_t*>(input.shape.data()),
                input.shape.size());

            impl_->last_outputs = impl_->session->Run(Ort::RunOptions{nullptr},
                                                       impl_->input_names.data(),
                                                       &input_tensor,
                                                       1,
                                                       impl_->output_names.data(),
                                                       impl_->output_names.size());
            outputs.clear();
            outputs.reserve(impl_->last_outputs.size());
            for (auto& value : impl_->last_outputs) {
                outputs.emplace_back(makeTensorView(value, false));
            }
        }
    } catch (const Ort::Exception& ex) {
        VA_LOG_ERROR() << "OrtModelSession inference failed: " << ex.what();
        return false;
    } catch (const std::exception& ex) {
        VA_LOG_ERROR() << "OrtModelSession inference failed: " << ex.what();
        return false;
    }

    return true;
}

#else // USE_ONNXRUNTIME

struct OrtModelSession::Impl {};

OrtModelSession::OrtModelSession() = default;
OrtModelSession::~OrtModelSession() = default;

bool OrtModelSession::loadModel(const std::string&, bool) {
    loaded_ = true;
    return true;
}

bool OrtModelSession::run(const core::TensorView&, std::vector<core::TensorView>& outputs) {
    outputs.clear();
    return loaded_;
}

#endif // USE_ONNXRUNTIME

} // namespace va::analyzer

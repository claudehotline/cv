#include "analyzer/ort_session.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <mutex>
#include <numeric>

#ifdef USE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#endif

namespace va::analyzer {

#ifdef USE_ONNXRUNTIME
struct OrtModelSession::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "VA_ONNX"};
    Ort::Session session{nullptr};
    Ort::SessionOptions session_options;
    std::vector<const char*> input_names;
    std::vector<const char*> output_names;
    std::vector<std::string> input_names_storage;
    std::vector<std::string> output_names_storage;
    bool use_gpu{false};
    std::mutex mutex;
};
#else
struct OrtModelSession::Impl {};
#endif

OrtModelSession::OrtModelSession() = default;
OrtModelSession::~OrtModelSession() = default;

#ifdef USE_ONNXRUNTIME
bool OrtModelSession::loadModel(const std::string& model_path, bool use_gpu) {
    if (!impl_) {
        impl_ = std::make_unique<Impl>();
    }

    std::scoped_lock lock(impl_->mutex);

    try {
        impl_->session_options = Ort::SessionOptions{};
        impl_->session_options.SetIntraOpNumThreads(1);
        impl_->session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef USE_CUDA
        if (use_gpu) {
            OrtCUDAProviderOptions cuda_opts;
            impl_->session_options.AppendExecutionProvider_CUDA(cuda_opts);
            impl_->use_gpu = true;
        } else {
            impl_->use_gpu = false;
        }
#else
        (void)use_gpu;
        impl_->use_gpu = false;
#endif

#ifdef _WIN32
        std::wstring wide_path(model_path.begin(), model_path.end());
        impl_->session = Ort::Session(impl_->env, wide_path.c_str(), impl_->session_options);
#else
        impl_->session = Ort::Session(impl_->env, model_path.c_str(), impl_->session_options);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        const size_t input_count = impl_->session.GetInputCount();
        impl_->input_names_storage.clear();
        impl_->input_names.clear();
        impl_->input_names_storage.reserve(input_count);
        impl_->input_names.reserve(input_count);
        for (size_t i = 0; i < input_count; ++i) {
            auto name = impl_->session.GetInputNameAllocated(i, allocator);
            impl_->input_names_storage.emplace_back(name.get());
            impl_->input_names.emplace_back(impl_->input_names_storage.back().c_str());
        }

        const size_t output_count = impl_->session.GetOutputCount();
        impl_->output_names_storage.clear();
        impl_->output_names.clear();
        impl_->output_names_storage.reserve(output_count);
        impl_->output_names.reserve(output_count);
        for (size_t i = 0; i < output_count; ++i) {
            auto name = impl_->session.GetOutputNameAllocated(i, allocator);
            impl_->output_names_storage.emplace_back(name.get());
            impl_->output_names.emplace_back(impl_->output_names_storage.back().c_str());
        }
    } catch (const Ort::Exception&) {
        loaded_ = false;
        return false;
    } catch (...) {
        loaded_ = false;
        return false;
    }

    loaded_ = true;
    return true;
}

static core::TensorView tensorFromOrt(Ort::Value& value) {
    core::TensorView view;
    if (!value.IsTensor()) {
        return view;
    }

    float* data = value.GetTensorMutableData<float>();
    view.data = data;
    view.dtype = core::DType::F32;
    view.on_gpu = false;

    Ort::TensorTypeAndShapeInfo shape_info = value.GetTensorTypeAndShapeInfo();
    view.shape = shape_info.GetShape();
    return view;
}

bool OrtModelSession::run(const core::TensorView& input, std::vector<core::TensorView>& outputs) {
    if (!loaded_ || !impl_) {
        return false;
    }

    if (!input.data) {
        return false;
    }

    std::scoped_lock lock(impl_->mutex);

    try {
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

        std::vector<int64_t> input_shape = input.shape;
        const size_t element_count = input_shape.empty() ? 0 : std::accumulate(input_shape.begin(), input_shape.end(), static_cast<size_t>(1), std::multiplies<size_t>());

        Ort::Value input_tensor = Ort::Value::CreateTensor(memory_info, static_cast<float*>(input.data), element_count, input_shape.data(), input_shape.size());

        std::vector<Ort::Value> ort_outputs = impl_->session.Run(Ort::RunOptions{nullptr},
                                                                 impl_->input_names.data(),
                                                                 &input_tensor,
                                                                 1,
                                                                 impl_->output_names.data(),
                                                                 impl_->output_names.size());

        outputs.clear();
        outputs.reserve(ort_outputs.size());
        for (auto& value : ort_outputs) {
            outputs.emplace_back(tensorFromOrt(value));
        }
    } catch (const Ort::Exception&) {
        return false;
    } catch (...) {
        return false;
    }

    return true;
}
#else
bool OrtModelSession::loadModel(const std::string&, bool) {
    loaded_ = true;
    return true;
}

bool OrtModelSession::run(const core::TensorView&, std::vector<core::TensorView>& outputs) {
    outputs.clear();
    return loaded_;
}
#endif

} // namespace va::analyzer

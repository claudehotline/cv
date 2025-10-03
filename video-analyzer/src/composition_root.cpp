#include "composition_root.hpp"

#include "analyzer/analyzer.hpp"
#include "analyzer/ort_session.hpp"
#include "analyzer/preproc_letterbox_cpu.hpp"
#include "analyzer/preproc_letterbox_cuda.hpp"
#include "analyzer/postproc_yolo_det.hpp"
#include "analyzer/postproc_yolo_seg.hpp"
#include "analyzer/postproc_detr.hpp"
#include "analyzer/renderer_passthrough.hpp"
#include "core/engine_manager.hpp"
#include "media/encoder_h264_ffmpeg.hpp"
#include "media/source_switchable_rtsp.hpp"
#include "media/transport_webrtc_datachannel.hpp"

#include "core/logger.hpp"

#include <algorithm>

namespace va {

va::core::Factories buildFactories(va::core::EngineManager& engine_manager) {
    va::core::Factories factories;

    factories.make_source = [](const va::core::SourceConfig& cfg) {
        return std::make_shared<va::media::SwitchableRtspSource>(cfg.uri);
    };

    factories.make_filter = [&engine_manager](const va::core::FilterConfig& cfg) {
        auto analyzer = std::make_shared<va::analyzer::Analyzer>();

        auto engine_desc = engine_manager.currentEngine();
        const std::string provider_source = !cfg.engine_provider.empty() ? cfg.engine_provider : engine_desc.provider;
        const bool hint_gpu = (!provider_source.empty() && (provider_source.find("cuda") != std::string::npos || provider_source.find("trt") != std::string::npos))
                               || cfg.use_io_binding;

        std::shared_ptr<va::analyzer::IPreprocessor> preprocessor;
#ifdef USE_CUDA
        if (hint_gpu) {
            preprocessor = std::make_shared<va::analyzer::LetterboxPreprocessorCUDA>(cfg.input_width, cfg.input_height);
        }
#endif
        if (!preprocessor) {
            preprocessor = std::make_shared<va::analyzer::LetterboxPreprocessorCPU>(cfg.input_width, cfg.input_height);
        }
        analyzer->setPreprocessor(preprocessor);

        auto session = std::make_shared<va::analyzer::OrtModelSession>();
#ifdef USE_ONNXRUNTIME
        va::analyzer::OrtModelSession::Options options;
        options.provider = !cfg.engine_provider.empty() ? cfg.engine_provider : engine_desc.provider;
        options.device_id = cfg.device_index;
        options.use_io_binding = cfg.use_io_binding;
        options.prefer_pinned_memory = cfg.prefer_pinned_memory;
        options.allow_cpu_fallback = cfg.allow_cpu_fallback;
        options.enable_profiling = cfg.enable_profiling;
        options.tensorrt_fp16 = cfg.tensorrt_fp16;
        options.tensorrt_int8 = cfg.tensorrt_int8;
        options.tensorrt_workspace_mb = cfg.tensorrt_workspace_mb;
        options.tensorrt_max_partition_iterations = cfg.tensorrt_max_partition_iterations;
        options.tensorrt_min_subgraph_size = cfg.tensorrt_min_subgraph_size;
        options.io_binding_input_bytes = cfg.io_binding_input_bytes;
        options.io_binding_output_bytes = cfg.io_binding_output_bytes;
        session->setOptions(options);
#endif

        const std::string& model_path = !cfg.model_path.empty() ? cfg.model_path : cfg.model_id;

        std::string provider_lower = provider_source;
        std::transform(provider_lower.begin(), provider_lower.end(), provider_lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        bool use_gpu = hint_gpu || provider_lower == "gpu";

        if (!session->loadModel(model_path, use_gpu)) {
            VA_LOG_ERROR() << "[Factories] failed to load model at " << model_path
                           << " (gpu=" << std::boolalpha << use_gpu << std::noboolalpha << ")";
            return std::shared_ptr<va::analyzer::Analyzer>{};
        }

        {
            auto runtime = session->runtimeInfo();
            va::core::EngineRuntimeStatus status;
            status.provider = runtime.provider;
            status.gpu_active = runtime.gpu_active;
            status.io_binding = runtime.io_binding_active;
            status.device_binding = runtime.device_binding_active;
            status.cpu_fallback = runtime.cpu_fallback;
            engine_manager.updateRuntimeStatus(std::move(status));
        }

        analyzer->setSession(session);
        analyzer->setUseGpuHint(use_gpu);

        std::shared_ptr<va::analyzer::IPostprocessor> postprocessor;
        if (cfg.task == "seg") {
            postprocessor = std::make_shared<va::analyzer::YoloSegmentationPostprocessor>();
        } else if (cfg.task == "detr") {
            postprocessor = std::make_shared<va::analyzer::DetrPostprocessor>();
        } else {
            postprocessor = std::make_shared<va::analyzer::YoloDetectionPostprocessor>();
        }
        analyzer->setPostprocessor(postprocessor);

        auto renderer = std::make_shared<va::analyzer::PassthroughRenderer>();
        analyzer->setRenderer(renderer);

        auto params = std::make_shared<va::analyzer::AnalyzerParams>();
        params->confidence_threshold = cfg.confidence_threshold;
        params->iou_threshold = cfg.iou_threshold;
        analyzer->updateParams(std::move(params));

        return analyzer;
    };

    factories.make_encoder = [](const va::core::EncoderConfig& cfg) {
        auto encoder = std::make_shared<va::media::FfmpegH264Encoder>();
        return encoder;
    };

    factories.make_transport = [](const va::core::TransportConfig& /*cfg*/) {
        auto transport = std::make_shared<va::media::WebRTCDataChannelTransport>();
        return transport;
    };

    return factories;
}

} // namespace va

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
#include "media/transport_whip.hpp"

namespace va {

va::core::Factories buildFactories(va::core::EngineManager& /*engine_manager*/) {
    va::core::Factories factories;

    factories.make_source = [](const va::core::SourceConfig& cfg) {
        return std::make_shared<va::media::SwitchableRtspSource>(cfg.uri);
    };

    factories.make_filter = [](const va::core::FilterConfig& cfg) {
        auto analyzer = std::make_shared<va::analyzer::Analyzer>();

        auto preprocessor = std::make_shared<va::analyzer::LetterboxPreprocessorCPU>(cfg.input_width, cfg.input_height);
        analyzer->setPreprocessor(preprocessor);

        auto session = std::make_shared<va::analyzer::OrtModelSession>();
        const std::string& model_path = !cfg.model_path.empty() ? cfg.model_path : cfg.model_id;
        session->loadModel(model_path, false);
        analyzer->setSession(session);

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

    factories.make_transport = [](const va::core::TransportConfig& cfg) {
        auto transport = std::make_shared<va::media::WhipTransport>();
        return transport;
    };

    return factories;
}

} // namespace va

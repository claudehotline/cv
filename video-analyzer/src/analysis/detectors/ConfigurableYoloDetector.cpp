#include "analysis/detectors/ConfigurableYoloDetector.h"

ConfigurableYoloDetector::ConfigurableYoloDetector(Options opts,
                                                   std::unique_ptr<IInferenceBackend> backend,
                                                   std::unique_ptr<IPreprocessor> pre,
                                                   std::unique_ptr<IDetectionPostprocessor> post)
    : BaseYoloDetector(std::move(backend), std::move(pre), std::move(post)), opts_(std::move(opts)) {}

ConfigurableYoloDetector::~ConfigurableYoloDetector() = default;


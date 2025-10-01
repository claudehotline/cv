#include "pipeline/Pipeline.h"

#include <chrono>
#include <iostream>
#include <thread>

Pipeline::Pipeline(const Options& options)
    : options_(options) {}

Pipeline::~Pipeline() {
    stop();
}

void Pipeline::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return;
    }
    state_.store(State::Prewarming);
    worker_ = std::thread(&Pipeline::runLoop, this);
}

void Pipeline::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    state_.store(State::Stopping);
    if (worker_.joinable()) {
        worker_.join();
    }
    state_.store(State::Idle);
    decoder_.close();
    decoder_open_ = false;
}

void Pipeline::updateSource(const std::string& url) {
    std::lock_guard<std::mutex> lk(mu_);
    options_.source_url = url;
    if (prewarm_done_.load()) {
        prewarm_done_.store(false);
    }
    frame_counter_ = 0;
    decoder_.close();
    decoder_open_ = false;
}

void Pipeline::updateModel(const std::string& model_id) {
    std::lock_guard<std::mutex> lk(mu_);
    options_.model_id = model_id;
    if (prewarm_done_.load()) {
        prewarm_done_.store(false);
    }
    frame_counter_ = 0;
}

void Pipeline::updateTask(AnalysisType task) {
    std::lock_guard<std::mutex> lk(mu_);
    options_.task = task;
}

Pipeline::Options Pipeline::snapshot() const {
    std::lock_guard<std::mutex> lk(mu_);
    return options_;
}

Pipeline::Metrics Pipeline::metrics() const {
    std::lock_guard<std::mutex> lk(mu_);
    return metrics_;
}

void Pipeline::setPrewarmCallback(std::function<bool()> callback) {
    std::lock_guard<std::mutex> lk(mu_);
    prewarm_callback_ = std::move(callback);
    prewarm_done_.store(false);
    prewarm_success_.store(false);
}

void Pipeline::runLoop() {
    auto last_account = std::chrono::steady_clock::now();
    int frames_since_last = 0;
    double latency_accumulator = 0.0;
    int latency_samples = 0;

    while (running_.load()) {
        if (!prewarm_done_.load()) {
            std::function<bool()> cb;
            {
                std::lock_guard<std::mutex> lk(mu_);
                cb = prewarm_callback_;
            }
            if (cb) {
                state_.store(State::Prewarming);
                bool ok = cb();
                prewarm_success_.store(ok);
                prewarm_done_.store(true);
                if (!ok) {
                    state_.store(State::Idle);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    continue;
                }
                state_.store(State::Running);
            } else {
                prewarm_done_.store(true);
                prewarm_success_.store(true);
                state_.store(State::Running);
            }
            continue;
        }

        if (state_.load() != State::Running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (!ensureDecoder()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        cv::Mat decoded;
        if (!decoder_.readFrame(decoded)) {
            decoder_.close();
            decoder_open_ = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        FrameStub frame;
        frame.id = ++frame_counter_;
        frame.timestamp = std::chrono::steady_clock::now();
        frame.frame = std::move(decoded);
        double processed_latency = processFrame(frame);

        frames_since_last++;
        latency_accumulator += processed_latency;
        latency_samples++;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_account);
        if (elapsed.count() >= 1) {
            std::lock_guard<std::mutex> lk(mu_);
            metrics_.last_frame_id = frame.id;
            if (elapsed.count() > 0) {
                metrics_.fps = frames_since_last / static_cast<double>(elapsed.count());
            }
            if (latency_samples > 0) {
                metrics_.average_latency_ms = latency_accumulator / latency_samples;
            }
            metrics_.last_update = now;
            frames_since_last = 0;
            latency_accumulator = 0.0;
            latency_samples = 0;
            last_account = now;
        }
    }
}

double Pipeline::processFrame(const FrameStub& frame) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - frame.timestamp).count();
    std::lock_guard<std::mutex> lk(mu_);
    std::cout << "[Pipeline] stream=" << options_.stream
              << " profile=" << options_.profile
              << " frame=" << frame.id
              << " latency=" << latency << "ms"
              << std::endl;
    (void)frame.frame;
    return static_cast<double>(latency);
}

bool Pipeline::ensureDecoder() {
    if (decoder_open_) return true;
    std::string url;
    {
        std::lock_guard<std::mutex> lk(mu_);
        url = options_.source_url;
    }
    if (url.empty()) return false;
    DecoderStage::Options opts;
    opts.url = url;
    decoder_open_ = decoder_.open(opts);
    return decoder_open_;
}

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <chrono>

#include <opencv2/opencv.hpp>

#include "analysis/Types.h"
#include "pipeline/DecoderStage.h"

class Pipeline {
public:
    enum class State {
        Idle,
        Prewarming,
        Running,
        Stopping
    };

    struct Options {
        std::string stream;
        std::string profile;
        std::string source_url;
        std::string model_id;
        AnalysisType task { AnalysisType::OBJECT_DETECTION };
    };

    struct FrameStub {
        int id {0};
        std::chrono::steady_clock::time_point timestamp;
        cv::Mat frame;
    };

    struct Metrics {
        int last_frame_id {0};
        double average_latency_ms {0.0};
        double fps {0.0};
        std::chrono::steady_clock::time_point last_update;
    };

    explicit Pipeline(const Options& options);
    ~Pipeline();

    void start();
    void stop();

    void updateSource(const std::string& url);
    void updateModel(const std::string& model_id);
    void updateTask(AnalysisType task);

    Options snapshot() const;
    bool isRunning() const { return running_.load(); }
    State state() const { return state_.load(); }

    void setPrewarmCallback(std::function<bool()> callback);

    bool wasPrewarmSuccessful() const { return prewarm_success_.load(); }

    Metrics metrics() const;

private:
    mutable std::mutex mu_;
    Options options_;
    std::thread worker_;
    std::atomic<bool> running_ {false};
    std::atomic<State> state_ {State::Idle};

    std::function<bool()> prewarm_callback_;
    std::atomic<bool> prewarm_done_ {false};
    std::atomic<bool> prewarm_success_ {false};

    int frame_counter_ {0};
    Metrics metrics_{};
    std::chrono::steady_clock::time_point last_frame_time_{};

    DecoderStage decoder_;
    bool decoder_open_ {false};

    void runLoop();
    double processFrame(const FrameStub& frame);
    bool ensureDecoder();
};

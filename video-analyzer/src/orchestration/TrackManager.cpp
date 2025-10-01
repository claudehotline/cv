#include "orchestration/TrackManager.h"

#include "pipeline/Pipeline.h"

#include <chrono>
#include <thread>

namespace {
TrackManager::Key makeKey(const std::string& stream, const std::string& profile) {
    return TrackManager::Key{ stream, profile };
}
}

TrackManager::TrackManager() = default;
TrackManager::~TrackManager() = default;

size_t TrackManager::KeyHash::operator()(const Key& k) const noexcept {
    return std::hash<std::string>{}(k.stream) ^ (std::hash<std::string>{}(k.profile) << 1);
}

bool TrackManager::KeyEq::operator()(const Key& a, const Key& b) const noexcept {
    return a.stream == b.stream && a.profile == b.profile;
}

bool TrackManager::subscribe(const std::string& stream, const std::string& profile) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        Context ctx;
        ctx.stream = stream;
        ctx.profile = profile;
        ctx.ref_count = 1;
        Pipeline::Options opts;
        opts.stream = stream;
        opts.profile = profile;
        opts.task = AnalysisType::OBJECT_DETECTION;
        ctx.pipeline = std::make_shared<Pipeline>(opts);
        ctx.pipeline->setPrewarmCallback([opts]() {
            // Placeholder for real prewarm (e.g., warm up inference session)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            return true;
        });
        ctx.pipeline->start();
        entries_.emplace(key, std::move(ctx));
    } else {
        it->second.ref_count++;
    }
    return true;
}

void TrackManager::unsubscribe(const std::string& stream, const std::string& profile) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) return;
    if (it->second.ref_count > 0) {
        it->second.ref_count--;
    }
    if (it->second.ref_count <= 0) {
        if (it->second.pipeline) {
            it->second.pipeline->stop();
        }
        entries_.erase(it);
    }
}

bool TrackManager::switchSource(const std::string& stream, const std::string& profile, const std::string& new_url) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    it->second.source_url = new_url;
    if (it->second.pipeline) {
        it->second.pipeline->updateSource(new_url);
    }
    return true;
}

bool TrackManager::switchModel(const std::string& stream, const std::string& profile, const std::string& model_id) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    it->second.model_id = model_id;
    if (it->second.pipeline) {
        it->second.pipeline->updateModel(model_id);
    }
    return true;
}

bool TrackManager::switchTask(const std::string& stream, const std::string& profile, AnalysisType task) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) return false;
    it->second.task = task;
    if (it->second.pipeline) {
        it->second.pipeline->updateTask(task);
    }
    return true;
}

std::optional<TrackManager::Context> TrackManager::getContext(const std::string& stream, const std::string& profile) const {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end()) return std::nullopt;
    return it->second;
}

std::vector<TrackManager::Context> TrackManager::listContexts() const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Context> list;
    list.reserve(entries_.size());
    for (const auto& pair : entries_) {
        list.push_back(pair.second);
    }
    return list;
}

bool TrackManager::setPrewarmCallback(const std::string& stream, const std::string& profile,
                                      std::function<bool()> callback) {
    std::lock_guard<std::mutex> lk(mu_);
    Key key = makeKey(stream, profile);
    auto it = entries_.find(key);
    if (it == entries_.end() || !it->second.pipeline) return false;
    it->second.pipeline->setPrewarmCallback(std::move(callback));
    return true;
}

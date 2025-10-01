#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "analysis/Types.h"

// Forward declaration for future implementation
class Pipeline;

class TrackManager {
public:
    struct Key {
        std::string stream;
        std::string profile;
    };

    struct Context {
        std::string stream;
        std::string profile;
        int ref_count {0};
        std::string source_url;
        std::string model_id;
        AnalysisType task { AnalysisType::OBJECT_DETECTION };
        std::shared_ptr<Pipeline> pipeline;
    };

    TrackManager();
    ~TrackManager();

    bool subscribe(const std::string& stream, const std::string& profile);
    void unsubscribe(const std::string& stream, const std::string& profile);

    bool switchSource(const std::string& stream, const std::string& profile, const std::string& new_url);
    bool switchModel(const std::string& stream, const std::string& profile, const std::string& model_id);
    bool switchTask(const std::string& stream, const std::string& profile, AnalysisType task);

    std::optional<Context> getContext(const std::string& stream, const std::string& profile) const;
    std::vector<Context> listContexts() const;
    bool setPrewarmCallback(const std::string& stream, const std::string& profile,
                            std::function<bool()> callback);

private:
    struct KeyHash {
        size_t operator()(const Key& k) const noexcept;
    };
    struct KeyEq {
        bool operator()(const Key& a, const Key& b) const noexcept;
    };

    std::unordered_map<Key, Context, KeyHash, KeyEq> entries_;
    mutable std::mutex mu_;
};

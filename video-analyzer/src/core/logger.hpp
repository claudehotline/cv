#pragma once

#include "ConfigLoader.hpp"

#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace va::core {

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    class Stream {
    public:
        Stream(Logger& logger, LogLevel level);
        Stream(Stream&& other) noexcept;
        ~Stream();

        template <typename T>
        Stream& operator<<(const T& value) {
            buffer_ << value;
            return *this;
        }

        Stream& operator<<(std::ostream& (*manip)(std::ostream&));

    private:
        Logger* logger_ {nullptr};
        LogLevel level_ {LogLevel::Info};
        std::ostringstream buffer_;
        bool moved_ {false};
    };

    static Logger& instance();

    void configure(const ObservabilityConfig& config);
    Stream stream(LogLevel level);

    bool isEnabled(LogLevel level) const;

private:
    Logger() = default;

    void log(LogLevel level, const std::string& message);
    std::string levelToString(LogLevel level) const;
    std::string timestamp() const;
    void openLogFile();
    void rotateIfNeeded(size_t incoming_bytes);
    LogLevel parseLevel(const std::string& level) const;
    void installRedirects();

    mutable std::mutex mutex_;
    LogLevel level_threshold_ {LogLevel::Info};
    bool console_enabled_ {true};
    std::string file_path_;
    size_t file_max_size_bytes_ {0};
    int file_max_files_ {0};
    std::ofstream file_stream_;

    class StreambufRedirect;
    std::unique_ptr<StreambufRedirect> cout_redirect_;
    std::unique_ptr<StreambufRedirect> cerr_redirect_;
    std::streambuf* original_cout_ {nullptr};
    std::streambuf* original_cerr_ {nullptr};
    bool redirects_installed_ {false};
};

} // namespace va::core

#define VA_LOG_STREAM(level) ::va::core::Logger::instance().stream(level)
#define VA_LOG_TRACE() VA_LOG_STREAM(::va::core::LogLevel::Trace)
#define VA_LOG_DEBUG() VA_LOG_STREAM(::va::core::LogLevel::Debug)
#define VA_LOG_INFO()  VA_LOG_STREAM(::va::core::LogLevel::Info)
#define VA_LOG_WARN()  VA_LOG_STREAM(::va::core::LogLevel::Warn)
#define VA_LOG_ERROR() VA_LOG_STREAM(::va::core::LogLevel::Error)


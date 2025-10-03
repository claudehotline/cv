#include "core/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace va::core {

namespace {

std::string toLowerCopy(const std::string& value) {
    std::string result(value.size(), '\0');
    std::transform(value.begin(), value.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

}

Logger::Stream::Stream(Logger& logger, LogLevel level)
    : logger_(&logger), level_(level) {}

Logger::Stream::Stream(Stream&& other) noexcept
    : logger_(other.logger_), level_(other.level_), buffer_(std::move(other.buffer_)), moved_(false) {
    other.moved_ = true;
    other.logger_ = nullptr;
}

Logger::Stream::~Stream() {
    if (!logger_ || moved_) {
        return;
    }
    logger_->log(level_, buffer_.str());
}

Logger::Stream& Logger::Stream::operator<<(std::ostream& (*manip)(std::ostream&)) {
    manip(buffer_);
    return *this;
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::configure(const ObservabilityConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    level_threshold_ = parseLevel(config.log_level);
    console_enabled_ = config.console;

    file_path_ = config.file_path;
    file_max_size_bytes_ = config.file_max_size_kb > 0 ? static_cast<size_t>(config.file_max_size_kb) * 1024 : 0;
    file_max_files_ = config.file_max_files;

    if (!file_path_.empty()) {
        openLogFile();
    } else if (file_stream_.is_open()) {
        file_stream_.close();
    }

    installRedirects();
}

Logger::Stream Logger::stream(LogLevel level) {
    return Stream(*this, level);
}

bool Logger::isEnabled(LogLevel level) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(level) >= static_cast<int>(level_threshold_);
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_threshold_)) {
        return;
    }

    const auto ts = timestamp();
    const auto level_str = levelToString(level);
    const std::string payload = ts + " [" + level_str + "] " + message + '\n';

    if (console_enabled_) {
        FILE* target = level >= LogLevel::Warn ? stderr : stdout;
        std::fwrite(payload.data(), 1, payload.size(), target);
        std::fflush(target);
    }

    if (file_stream_.is_open()) {
        rotateIfNeeded(payload.size());
        file_stream_ << payload;
        file_stream_.flush();
    }
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        default: return "INFO";
    }
}

std::string Logger::timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    auto fractional = std::chrono::duration_cast<std::chrono::milliseconds>(now - seconds).count();

    std::time_t tt = std::chrono::system_clock::to_time_t(now);
#ifdef _MSC_VER
    std::tm tm_buf;
    localtime_s(&tm_buf, &tt);
    const std::tm* tm = &tm_buf;
#else
    std::tm tm_buf = *std::localtime(&tt);
    const std::tm* tm = &tm_buf;
#endif
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << fractional;
    return oss.str();
}

void Logger::openLogFile() {
    if (file_path_.empty()) {
        return;
    }

    try {
        if (auto parent = fs::path(file_path_).parent_path(); !parent.empty()) {
            fs::create_directories(parent);
        }
        file_stream_.open(file_path_, std::ios::out | std::ios::app);
    } catch (...) {
        file_stream_.close();
    }
}

void Logger::rotateIfNeeded(size_t incoming_bytes) {
    if (!file_stream_.is_open() || file_max_size_bytes_ == 0) {
        return;
    }

    file_stream_.flush();

    std::error_code ec;
    auto size = fs::file_size(file_path_, ec);
    if (ec) {
        return;
    }

    if (size + incoming_bytes <= file_max_size_bytes_) {
        return;
    }

    file_stream_.close();

    if (file_max_files_ > 0) {
        for (int idx = file_max_files_ - 1; idx >= 1; --idx) {
            fs::path target = file_path_ + "." + std::to_string(idx);
            fs::path next = file_path_ + "." + std::to_string(idx + 1);
            if (fs::exists(target, ec)) {
                std::error_code rename_ec;
                fs::rename(target, next, rename_ec);
            }
        }
    }

    fs::rename(file_path_, file_path_ + ".1", ec);
    openLogFile();
}

LogLevel Logger::parseLevel(const std::string& level) const {
    const auto value = toLowerCopy(level);
    if (value == "trace") return LogLevel::Trace;
    if (value == "debug") return LogLevel::Debug;
    if (value == "info")  return LogLevel::Info;
    if (value == "warn" || value == "warning") return LogLevel::Warn;
    if (value == "error" || value == "err") return LogLevel::Error;
    return LogLevel::Info;
}

class Logger::StreambufRedirect : public std::streambuf {
public:
    StreambufRedirect(Logger& logger, LogLevel level)
        : logger_(logger), level_(level) {}

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) {
            return sync();
        }
        buffer_.push_back(static_cast<char>(ch));
        if (ch == '\n') {
            flush();
        }
        return ch;
    }

    int sync() override {
        flush();
        return 0;
    }

private:
    void flush() {
        if (buffer_.empty()) {
            return;
        }
        std::string payload = buffer_;
        buffer_.clear();
        if (!payload.empty() && payload.back() == '\n') {
            payload.pop_back();
        }
        if (!payload.empty()) {
            logger_.log(level_, payload);
        }
    }

    Logger& logger_;
    LogLevel level_;
    std::string buffer_;
};

void Logger::installRedirects() {
    if (redirects_installed_) {
        return;
    }

    original_cout_ = std::cout.rdbuf();
    original_cerr_ = std::cerr.rdbuf();

    cout_redirect_ = std::make_unique<StreambufRedirect>(*this, LogLevel::Info);
    cerr_redirect_ = std::make_unique<StreambufRedirect>(*this, LogLevel::Error);

    std::cout.rdbuf(cout_redirect_.get());
    std::cerr.rdbuf(cerr_redirect_.get());

    redirects_installed_ = true;
}

} // namespace va::core

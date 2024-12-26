#ifndef CAPTURER_LOGGING_H
#define CAPTURER_LOGGING_H

#include <filesystem>
#include <fmt/format.h>
#include <glog/logging.h>
#include <probe/thread.h>
extern "C" {
#include <libavutil/log.h>
}

class Logger
{
public:
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    static Logger& init(const char *argv0, const std::string& dir)
    {
        static Logger logger(argv0, dir);
        return logger;
    }

    ~Logger() { google::ShutdownGoogleLogging(); }

private:
    explicit Logger(const char *argv0, const std::string& log_dir)
    {
        if (!std::filesystem::is_directory(log_dir) || std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }

        // clang-format off
        google::InitGoogleLogging(
            argv0,
            [](std::ostream& _stream, const google::LogMessageInfo& _info, void*)
            {
                std::string _file_line = _info.filename + std::string(":") + std::to_string(_info.line_number);
                _file_line = _file_line.substr(std::max<int64_t>(0, _file_line.size() - 24), 24);

                _stream
                    << std::setw(4) << 1900 + _info.time.year() << '-'
                    << std::setw(2) << 1 + _info.time.month() << '-'
                    << std::setw(2) << _info.time.day()
                    << ' '
                    << std::setw(2) << _info.time.hour() << ':'
                    << std::setw(2) << _info.time.min() << ':'
                    << std::setw(2) << _info.time.sec() << "."
                    << std::setw(3) << _info.time.usec() / 1000
                    << std::setfill(' ')
                    << ' '
                    << std::setw(7) << _info.severity
                    << ' '
                    << std::setw(5) << _info.thread_id
                    << " -- ["
                    << std::setw(24) << _file_line
                    << "] ["
#ifdef _WIN32
                    <<  std::setw(32) << probe::thread::name().substr(0, 32) << "]:";
#else
                    <<  std::setw(15) << probe::thread::name().substr(0, 15) << "]:";
#endif
            }
        );
        // clang-format on

        FLAGS_log_dir                   = log_dir;
        FLAGS_max_log_size              = 32; // MB
        FLAGS_stop_logging_if_full_disk = true;
        FLAGS_logbufsecs                = 0;  // s
        FLAGS_colorlogtostderr          = true;
        google::SetStderrLogging(google::GLOG_INFO);
        google::SetLogDestination(google::GLOG_INFO, std::string{ log_dir + "/CAPTURER-I-" }.c_str());
        google::SetLogDestination(google::GLOG_WARNING, std::string{ log_dir + "/CAPTURER-W-" }.c_str());
        google::SetLogDestination(google::GLOG_ERROR, std::string{ log_dir + "/CAPTURER-E-" }.c_str());
        google::SetLogDestination(google::GLOG_FATAL, std::string{ log_dir + "/CAPTURER-F-" }.c_str());
        google::SetLogFilenameExtension(".log");
        google::EnableLogCleaner(3); // days
        google::InstallFailureSignalHandler();
        google::InstallFailureWriter(
            [](const char *data, size_t size) { LOG(ERROR) << std::string(data, size); });

        // ffmpeg
        av_log_set_callback([](void *avcl, int level, const char *fmt, va_list args) {
            if (level > AV_LOG_INFO) return;

            std::string name{ "FFMPEG-L" + std::to_string(level >> 3) };
            if (AVClass *avc = avcl ? *(AVClass **)avcl : nullptr; avc) {
                name += "@";
                name += (avc->item_name ? avc->item_name : av_default_item_name)(avcl);
            }

            char buffer[1024]{};
            vsnprintf(buffer, 1024, fmt, args);

            LOG_IF(ERROR, level == AV_LOG_PANIC) << fmt::format("[{:>11}] {}", name, buffer);
            LOG_IF(ERROR, level == AV_LOG_FATAL) << fmt::format("[{:>11}] {}", name, buffer);
            LOG_IF(ERROR, level == AV_LOG_ERROR) << fmt::format("[{:>11}] {}", name, buffer);
            LOG_IF(WARNING, level == AV_LOG_WARNING) << fmt::format("[{:>11}] {}", name, buffer);
            LOG_IF(INFO, level == AV_LOG_INFO) << fmt::format("[{:>11}] {}", name, buffer);
        });
    }
};

#define NOTNULL(VAL) CHECK_NOTNULL(VAL)

#define logd(FMT, ...) DLOG(INFO) << fmt::format(FMT, ##__VA_ARGS__)
#define logi(FMT, ...) LOG(INFO) << fmt::format(FMT, ##__VA_ARGS__)
#define logw(FMT, ...) LOG(WARNING) << fmt::format(FMT, ##__VA_ARGS__)
#define loge(FMT, ...) LOG(ERROR) << fmt::format(FMT, ##__VA_ARGS__)
#define logf(FMT, ...) LOG(FATAL) << fmt::format(FMT, ##__VA_ARGS__)

#define logd_if(C, FMT, ...) DLOG_IF(INFO, C) << fmt::format(FMT, ##__VA_ARGS__)
#define logi_if(C, FMT, ...) LOG_IF(INFO, C) << fmt::format(FMT, ##__VA_ARGS__)
#define logw_if(C, FMT, ...) LOG_IF(WARNING, C) << fmt::format(FMT, ##__VA_ARGS__)
#define loge_if(C, FMT, ...) LOG_IF(ERROR, C) << fmt::format(FMT, ##__VA_ARGS__)
#define logf_if(C, FMT, ...) LOG_IF(FATAL, C) << fmt::format(FMT, ##__VA_ARGS__)

#endif //! CAPTURER_LOGGING_H

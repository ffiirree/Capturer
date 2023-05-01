#ifndef LOGGING_H
#define LOGGING_H

#include "probe/util.h"

#include <QRect>
#include <QSize>
#include <QStringList>
#include <filesystem>
#include <glog/logging.h>

class Logger
{
public:
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    static Logger& init(const char *argv0)
    {
        static Logger logger(argv0);
        return logger;
    }

    ~Logger() { google::ShutdownGoogleLogging(); }

private:
    explicit Logger(const char *argv0)
    {
#ifdef _WIN32
        const std::string log_dir = "logs";
#elif __linux__
        const std::string log_dir = std::string{ ::getenv("HOME") } + "/.local/logs/capturer";
#endif
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
                    <<  std::setw(32) << probe::util::thread_get_name().substr(0, 32) << "]:";
#else
                    <<  std::setw(15) << probe::util::thread_get_name().substr(0, 15) << "]:";
#endif
            }
        );
        // clang-format on

        FLAGS_log_dir                   = log_dir;
        FLAGS_max_log_size              = 32; // MB
        FLAGS_stop_logging_if_full_disk = true;
        FLAGS_logbufsecs                = 0; // s
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
    }
};

inline std::ostream& operator<<(std::ostream& out, const QString& string)
{
    return out << string.toStdString();
}

inline std::ostream& operator<<(std::ostream& out, const QStringList& list)
{
    foreach(QString str, list) {
        out << str.toStdString();
    }
    return out;
}

inline std::ostream& operator<<(std::ostream& out, const QPoint& point)
{
    return out << "<" << point.x() << ", " << point.y() << ">";
}

inline std::ostream& operator<<(std::ostream& out, const QSize& size)
{
    return out << "<" << size.width() << " x " << size.height() << ">";
}

inline std::ostream& operator<<(std::ostream& out, const QRect& rect)
{
    return out << "<<" << rect.left() << ", " << rect.top() << ">, <" << rect.width() << " x "
               << rect.height() << ">>";
}

#endif // LOGGING_H

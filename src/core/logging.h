#ifndef LOGGING_H
#define LOGGING_H

#include <glog/logging.h>
#include <QSize>
#include <QRect>
#include <QStringList>
#include <filesystem>
#include "platform.h"

constexpr auto GLOG_DIR = "logs";

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static Logger& init(char** argv) {
        static Logger logger(argv);
        return logger;
    }

    ~Logger() {
        google::ShutdownGoogleLogging();
    }

private:
    explicit Logger(char** argv) {
        if (!std::filesystem::is_directory(GLOG_DIR) || std::filesystem::exists(GLOG_DIR)) {
            std::filesystem::create_directory(GLOG_DIR);
        }

        google::InitGoogleLogging(
            argv[0],
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
                    << " - "
                    << std::setfill(' ') << std::setw(5) << _info.thread_id
                    << " - "
                    << _info.severity[0]
                    << " - "
                    << std::setw(24)
                    << _file_line << ']';
            }
        );

        FLAGS_log_dir = GLOG_DIR;
        FLAGS_max_log_size = 32;                    // MB
        FLAGS_stop_logging_if_full_disk = true;
        FLAGS_logbufsecs = 0;                       // s
        FLAGS_stderrthreshold = google::GLOG_INFO;
        FLAGS_colorlogtostderr = true;
        google::SetLogFilenameExtension(".txt");
        google::EnableLogCleaner(7);                // days
    }
};

inline std::ostream& operator<<(std::ostream& out, const QString& string) 
{
    return out << string.toStdString();
}

inline std::ostream& operator<<(std::ostream& out, const QStringList& list) 
{
    foreach (QString str, list) {
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
    return out << "<<" << rect.left() << ", " << rect.top() << ">, <"<< rect.width() << " x " << rect.height() << ">>";
}

inline std::ostream& operator<<(std::ostream& out, const platform::version_t& ver)
{
    out << ver.major << "." << ver.minor << "." << ver.patch << "." << ver.build;

    if (!ver.codename.empty()) {
        out << " (" << ver.codename << ")";
    }

    return out;
}

inline std::ostream& operator<<(std::ostream& out, const platform::cpu::architecture_t& arch)
{
    switch (arch)
    {
    case platform::cpu::architecture_t::x86: out << "x86"; break;
    case platform::cpu::architecture_t::ia64: out << "ia64"; break;
    case platform::cpu::architecture_t::x64: out << "x64"; break;
    case platform::cpu::architecture_t::arm: out << "arm"; break;
    case platform::cpu::architecture_t::arm64: out << "arm64"; break;
    case platform::cpu::architecture_t::unknown:
    default: out << "unknown"; break;
    }

    return out;
}

#endif // LOGGING_H

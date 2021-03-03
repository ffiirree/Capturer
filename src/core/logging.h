#ifndef LOGGING_H
#define LOGGING_H

#include <glog/logging.h>
#include <QSize>
#include <QRect>
#include <QStringList>
#include <QDir>

#define GLOG_DIR    "logs"


class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger& init(char** argv) {
        static Logger logger(argv);
        return logger;
    }

    ~Logger() {
        google::ShutdownGoogleLogging();
    }

private:
    Logger(char** argv) {
        if (!QDir(GLOG_DIR).exists()) QDir().mkdir(GLOG_DIR);

        FLAGS_log_dir = GLOG_DIR;
        google::InitGoogleLogging(argv[0]);
        FLAGS_max_log_size = 1024;              // MB
        FLAGS_logbufsecs = 0;
        FLAGS_colorlogtostderr = true;

        google::EnableLogCleaner(3);
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

inline std::ostream& operator<<(std::ostream& out, const QSize& size) 
{
    return out << "<" << size.width() << " x " << size.height() << ">";
}

inline std::ostream& operator<<(std::ostream& out, const QRect& rect)
{
    return out << "<<" << rect.left() << ", " << rect.top() << ">, <"<< rect.width() << " x " << rect.height() << ">>";
}
#endif // LOGGING_H

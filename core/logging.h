#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>

// Log
#ifndef LOG
    #define ERROR       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).critical().nospace().noquote()
    #define INFO        QMessageLogger(__FILE__, __LINE__, __FUNCTION__).info().nospace().noquote()
    #define DEBUG       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).debug().nospace().noquote()
    #define WARNING     QMessageLogger(__FILE__, __LINE__, __FUNCTION__).warning().nospace().noquote()
    #define FATAL       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).fatal().nospace().noquote()

    #define LOG(X)      X
#endif

inline QDebug operator<<(QDebug out, const std::string& str)
{
    out << QString::fromStdString(str);
    return out;
}

#endif // LOGGING_H

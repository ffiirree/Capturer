#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>

// Log
#ifndef LOG
    #define ERROR       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).critical()
    #define INFO        QMessageLogger(__FILE__, __LINE__, __FUNCTION__).info()
    #define DEBUG       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).debug()
    #define WARNING     QMessageLogger(__FILE__, __LINE__, __FUNCTION__).warning()
    #define FATAL       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).fatal()

    #define LOG(X)      X
#endif


#endif // LOGGING_H

#ifndef CAPTURER_H
#define CAPTURER_H

#include <QDebug>

#ifndef st
#define st(X) do{X}while(0)
#endif

enum Graph:unsigned int {
    NONE,
    RECTANGLE   = 0x0001,
    CIRCLE      = 0x0002,
    ELLIPSE     = CIRCLE,
    LINE        = 0x0004,
    CURVES      = 0x0008,
    MOSAIC      = 0x0010,
    ARROW       = 0x0020,
    TEXT        = 0x0040,
    ERASER      = 0x0080,
    BROKEN_LINE = 0x0100,
};

// Log
#ifndef LOG
    #define ERROR       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).critical()
    #define INFO        QMessageLogger(__FILE__, __LINE__, __FUNCTION__).info()
    #define DEBUG       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).debug()
    #define WARNING     QMessageLogger(__FILE__, __LINE__, __FUNCTION__).warning()
    #define FATAL       QMessageLogger(__FILE__, __LINE__, __FUNCTION__).fatal()

    #define LOG(X)      X
#endif

#define LOAD_QSS(X, Y)			QFile file(Y);						\
								file.open(QFile::ReadOnly);			\
																	\
								if (file.isOpen()) {				\
									auto style = file.readAll();	\
									X->setStyleSheet(style);		\
									file.close();					\
								}	

#endif // CAPTURER_H

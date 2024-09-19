#ifndef CAPTURER_MESSAGE_H
#define CAPTURER_MESSAGE_H

#include "framelesswindow.h"

#include <QTimer>

class Message final : public FramelessWindow
{
    Q_OBJECT

public:
    enum class MessageLevel
    {
        LEVEL_MESSAGE,
        LEVEL_SUCCESS,
        LEVEL_WARNING,
        LEVEL_ERROR,
    };

public:
    Message(const QString& text, MessageLevel level, int ms, QWidget *parent = nullptr);

    static void message(const QString& text, int ms = 1500);
    static void success(const QString& text, int ms = 1500);
    static void warning(const QString& text, int ms = 1500);
    static void error(const QString& text, int ms = 1500);

private:
    QTimer timer_;
};

#endif //! CAPTURER_MESSAGE_H
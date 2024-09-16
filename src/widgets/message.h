#ifndef CAPTURER_MESSAGE_H
#define CAPTURER_MESSAGE_H

#include <QTimer>
#include "framelesswindow.h"

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
    Message(const QString& text, MessageLevel level, QWidget *parent = nullptr);

    static void message(QWidget *parent, const QString& text);
    static void success(QWidget *parent, const QString& text);
    static void warning(QWidget *parent, const QString& text);
    static void error(QWidget *parent, const QString& text);

private:
    QTimer timer_;
};

#endif //! CAPTURER_MESSAGE_H
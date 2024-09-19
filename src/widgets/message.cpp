#include "message.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>

Message::Message(const QString& text, MessageLevel level, int ms, QWidget *parent)
    : FramelessWindow(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_DeleteOnClose);

    //
    const auto layout = new QHBoxLayout();
    layout->setContentsMargins({  });
    layout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(layout);

    layout->addWidget(new QLabel(text));

    switch (level) {
    case Message::MessageLevel::LEVEL_SUCCESS: setObjectName("success"); break;
    case Message::MessageLevel::LEVEL_WARNING: setObjectName("warning"); break;
    case Message::MessageLevel::LEVEL_ERROR:   setObjectName("error"); break;
    case Message::MessageLevel::LEVEL_MESSAGE:
    default:                                   setObjectName("message"); break;
    }

    //
    const auto geo = QApplication::screenAt(QCursor::pos())->geometry();
    move(geo.center() - rect().center());

    show();

    connect(&timer_, &QTimer::timeout, this, &Message::close);
    timer_.start(ms);
}

void Message::message(const QString& text, int ms)
{
    new Message(text, MessageLevel::LEVEL_MESSAGE, ms, nullptr);
}

void Message::success(const QString& text, int ms)
{
    new Message(text, MessageLevel::LEVEL_SUCCESS, ms, nullptr);
}

void Message::warning(const QString& text, int ms)
{
    new Message(text, MessageLevel::LEVEL_WARNING, ms, nullptr);
}

void Message::error(const QString& text, int ms)
{
    new Message(text, MessageLevel::LEVEL_ERROR, ms, nullptr);
}
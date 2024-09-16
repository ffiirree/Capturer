#include "message.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QScreen>

Message::Message(const QString& text, MessageLevel level, QWidget *parent)
    : FramelessWindow(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
{
    switch (level) {
    case MessageLevel::LEVEL_SUCCESS: setObjectName("success"); break;
    case MessageLevel::LEVEL_WARNING: setObjectName("warning"); break;
    case MessageLevel::LEVEL_ERROR:   setObjectName("error"); break;
    case MessageLevel::LEVEL_MESSAGE:
    default:                          setObjectName("message"); break;
    }

    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_DeleteOnClose);

    const auto layout = new QHBoxLayout();
    layout->setContentsMargins({ 10, 15, 10, 15 });
    layout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(layout);

    layout->addWidget(new QLabel(text));

    connect(&timer_, &QTimer::timeout, this, &Message::close);
    timer_.start(1500);

    show();

    const auto geo = (parent && (parent->windowFlags() & Qt::Window) && parent->isVisible())
                         ? parent->geometry()
                         : QApplication::screenAt(QCursor::pos())->geometry();
    move(geo.width() / 2 + geo.left() - width() / 2, geo.top() + geo.height() / 4 - height() / 2);
}

void Message::message(QWidget *parent, const QString& text)
{
    new Message(text, MessageLevel::LEVEL_MESSAGE, parent);
}

void Message::success(QWidget *parent, const QString& text)
{
    new Message(text, MessageLevel::LEVEL_SUCCESS, parent);
}

void Message::warning(QWidget *parent, const QString& text)
{
    new Message(text, MessageLevel::LEVEL_WARNING, parent);
}

void Message::error(QWidget *parent, const QString& text)
{
    new Message(text, MessageLevel::LEVEL_ERROR, parent);
}
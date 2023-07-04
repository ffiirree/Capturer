#ifndef CAPTURER_FRAMELESS_WINDOW_H
#define CAPTURER_FRAMELESS_WINDOW_H

#include <QWidget>

class FramelessWindow : public QWidget
{
public:
    explicit FramelessWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray& eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray& eventType, void *message, long *result) override;
#endif
};

#endif // !CAPTURER_FRAMELESS_WINDOW_H

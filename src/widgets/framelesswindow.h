#ifndef CAPTURER_FRAMELESS_WINDOW_H
#define CAPTURER_FRAMELESS_WINDOW_H

#include <QWidget>

class FramelessWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr);

public slots:
    void maximize(bool = true);
    void minimize(bool = true);
    void fullscreen(bool = true);

signals:
    void hidden();
    void closed();

    void normalized();
    void maximized();
    void minimized();
    void fullscreened();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void closeEvent(QCloseEvent *) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *) override;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray& eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray& eventType, void *message, long *result) override;
#endif

#ifdef Q_OS_LINUX
    QPoint moving_begin_{ -1, -1 };
#endif
};

#endif //! CAPTURER_FRAMELESS_WINDOW_H

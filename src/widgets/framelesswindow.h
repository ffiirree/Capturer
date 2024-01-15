#ifndef CAPTURER_FRAMELESS_WINDOW_H
#define CAPTURER_FRAMELESS_WINDOW_H

#include <QWidget>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define Q_NATIVE_EVENT_RESULT qintptr
#else
#define Q_NATIVE_EVENT_RESULT long
#endif

class FramelessWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

    [[nodiscard]] bool isSizeFixed() const;

public slots:
    void maximize(bool = true);
    void toggleMaximized();
    void minimize(bool = true);
    void fullscreen(bool = true);
    void toggleFullScreen();

signals:
    void hidden();
    void closed();

    void normalized();
    void maximized();
    void minimized();
    void fullscreened();

protected:
    void mousePressEvent(QMouseEvent *event) override;

    void closeEvent(QCloseEvent *) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *) override;

    bool nativeEvent(const QByteArray& eventType, void *message, Q_NATIVE_EVENT_RESULT *result) override;
};

#endif //! CAPTURER_FRAMELESS_WINDOW_H

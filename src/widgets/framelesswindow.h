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
    explicit FramelessWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());

    [[nodiscard]] bool isSizeFixed() const;

public slots:
    void maximize(bool = true);
    void toggleMaximized();
    void minimize(bool = true);
    void fullscreen(bool = true);
    void toggleFullScreen();
    void toggleTransparentInput();

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

    bool transparent_input_{};

#ifdef Q_OS_LINUX
    Qt::Edges edges_{};
    void      updateCursor(Qt::Edges edges);
    bool      event(QEvent *event) override;
#endif

#ifdef Q_OS_WIN
    int ResizeHandleHeight(HWND hWnd);

    bool nativeEvent(const QByteArray& eventType, void *message, Q_NATIVE_EVENT_RESULT *result) override;
#endif
};

#endif //! CAPTURER_FRAMELESS_WINDOW_H

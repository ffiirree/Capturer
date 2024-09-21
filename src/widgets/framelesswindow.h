#ifndef CAPTURER_FRAMELESS_WINDOW_H
#define CAPTURER_FRAMELESS_WINDOW_H

#include <QPointer>
#include <QWidget>

class TitleBar;

class FramelessWindow : public QWidget
{
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());

    [[nodiscard]] bool isSizeFixed() const;

    TitleBar *titlebar();

public slots:
    void toggleTransparentInput();

signals:
    void hidden();
    void closed();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void closeEvent(QCloseEvent *) override;
    void hideEvent(QHideEvent *event) override;

    QPointer<TitleBar> titlebar_{};

    bool transparent_input_{};

    bool dragmove_{ true };
    int  dragmove_status_{};

#ifdef Q_OS_LINUX
    Qt::Edges edges_{};
    void      updateCursor(Qt::Edges edges);
    bool      event(QEvent *event) override;
#endif

#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void *message, qintptr *result) override;
#endif
};

#endif //! CAPTURER_FRAMELESS_WINDOW_H

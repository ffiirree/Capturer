#ifndef CAPTURER_COLOR_WINDOW_H
#define CAPTURER_COLOR_WINDOW_H

#include "framelesswindow.h"

#include <memory>
#include <QColor>
#include <QLineEdit>
#include <QMimeData>

enum class number_t : int
{
    integral = 1,
    floating = 2,
    hybrid   = 3,
};

class ColorWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit ColorWindow(QWidget *parent = nullptr);
    explicit ColorWindow(const std::shared_ptr<QMimeData>& data, QWidget *parent = nullptr);

public slots:
    void refresh(number_t);

protected:
    void closeEvent(QCloseEvent *) override;

private:
    std::shared_ptr<QMimeData> data_{};

    QColor color_{};

    QLineEdit *hex_{};
    QLineEdit *rgb_{};
    QLineEdit *bgr_{};
    QLineEdit *hsv_{};
    QLineEdit *hsl_{};

    number_t format_{ 1 };
};

#endif // !CAPTURER_COLOR_WINDOW_H

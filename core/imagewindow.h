#ifndef CAPTURER_FIXEDWINDOW_H
#define CAPTURER_FIXEDWINDOW_H

#include <QWidget>

class ImageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ImageWindow(QWidget *parent = nullptr);
    virtual ~ImageWindow() override = default;

    void fix(QPixmap image);

private:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void paintEvent(QPaintEvent * event) override;

    QPoint begin_{0, 0};

    qreal scale_ = 1.0;
    QPixmap pixmap_;
    QSize size_{0, 0};
    QPoint center_{0, 0};
};

#endif //! CAPTURER_FIXEDWINDOW_H

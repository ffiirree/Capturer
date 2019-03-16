#ifndef CAPTURER_FIXEDWINDOW_H
#define CAPTURER_FIXEDWINDOW_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

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
    virtual void keyPressEvent(QKeyEvent *event) override;

    QHBoxLayout *layout_ = nullptr;
    QLabel *label_= nullptr;

    QPoint begin_{0, 0};
};

#endif //! CAPTURER_FIXEDWINDOW_H

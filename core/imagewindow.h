#ifndef CAPTURER_FIXEDWINDOW_H
#define CAPTURER_FIXEDWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QPoint>

class ImageWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ImageWindow(QWidget *parent = nullptr);
    ~ImageWindow();

    void fix(QPixmap image);

private:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);


    QHBoxLayout *layout_ = nullptr;
    QLabel *label_= nullptr;

    QPoint begin_, end_;
};

#endif //! CAPTURER_FIXEDWINDOW_H

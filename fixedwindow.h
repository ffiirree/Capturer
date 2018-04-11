#ifndef CAPTURER_FIXEDWINDOW_H
#define CAPTURER_FIXEDWINDOW_H

#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QPoint>

class FixImageWindow : public QDialog
{
    Q_OBJECT

public:
    explicit FixImageWindow(QWidget *parent = nullptr);
    ~FixImageWindow();

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

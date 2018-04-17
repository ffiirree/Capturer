#ifndef TESTCAPTURER_H
#define TESTCAPTURER_H

#include <selector.h>
#include <QTextEdit>
#include "mainmenu.h"
#include "graphmenu.h"
#include "magnifier.h"

class ScreenCapturer : public Selector
{
    Q_OBJECT

public:
    enum EditStatus {
        NONE,
        START_PAINTING_RECTANGLE, PAINTING_RECTANGLE, END_PAINTING_RECTANGLE,
        START_PAINTING_CIRCLE, PAINTING_CIRCLE, END_PAINTING_CIRCLE,
        START_PAINTING_ARROW, PAINTING_ARROW, END_PAINTING_ARROW,
        START_PAINTING_LINE, PAINTING_LINE, END_PAINTING_LINE,
        START_PAINTING_CURVES, PAINTING_CURVES, END_PAINTING_CURVES,
        START_PAINTING_TEXT, PAINTING_TEXT, END_PAINTING_TEXT
    };

public:
    explicit ScreenCapturer(QWidget *parent = nullptr);

Q_SIGNALS:
    void CAPTURE_SCREEN_DONE(QPixmap image);
    void FIX_IMAGE(QPixmap image);

public slots:
    virtual void start();

    void save_image();
    void copy2clipboard();
    void fix_image();
    void exit_capture();

protected:
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void paintEvent(QPaintEvent *event);

private:
    void getArrowPoints(QPoint begin, QPoint end_, QPoint* points);

    QPixmap captured_screen_, captured_image_;

    EditStatus edit_status_ = NONE;
    int pen_width_ = 1;
    bool fill_ = false;
    QColor color_ = Qt::cyan;

    QTextEdit * text_edit_ = nullptr;
    QPoint text_pos_{0, 0};

    MainMenu * menu_ = nullptr;
    GraphMenu * gmenu_ = nullptr;
    Magnifier * magnifier_ = nullptr;

    QPoint rectangle_begin_{0, 0}, rectangle_end_{0, 0};
    QPoint circle_begin_{0, 0}, circle_end_{0, 0};
    QPoint arrow_begin_{0, 0}, arrow_end_{0, 0};
    QPoint line_begin_{0, 0}, line_end_{0, 0};
    QPoint curves_begin_{0, 0}, curves_end_{0, 0};
};

#endif // TESTCAPTURER_H

#ifndef TESTCAPTURER_H
#define TESTCAPTURER_H

#include <selector.h>
#include <QTextEdit>
#include "mainmenu.h"
#include "graphmenu.h"
#include "magnifier.h"
#include "textedit.h"
#include "fontmenu.h"
#include "command.h"
#include "resizer.h"

#define  DO(COMMAND) do{ auto __command__ = COMMAND; focus_ = __command__; undo_stack_.push(__command__); redo_stack_.clear(); }while(0)

class ScreenShoter : public Selector
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
        START_PAINTING_TEXT, PAINTING_TEXT, END_PAINTING_TEXT,
        GRAPH_MOVING, GRAPH_RESIZING
    };

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void CAPTURE_SCREEN_DONE(QPixmap image);
    void FIX_IMAGE(QPixmap image);

public slots:
    virtual void start() override;
    virtual void exit() override;

    void save_image();
    void copy2clipboard();
    void pin_image();

    void undo();
    void redo();

private slots:
    void updateMenuPosition();

protected:
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void keyPressEvent(QKeyEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;

private:
    void registerShortcuts();
    void getArrowPoints(QPoint begin, QPoint end_, QPoint* points);

    void getCursorByPos(const QPoint& pos);
    void setCursorByPos(Resizer::PointPosition pos, const QCursor & default_cursor = Qt::CrossCursor);

    void upadateMagnifierPosition();

    QPixmap captured_screen_, captured_image_;

    Resizer::PointPosition cursor_graph_pos_ = Resizer::OUTSIDE;

    Command * command_ = nullptr;
    Command * focus_ = nullptr;

    EditStatus edit_status_ = NONE, last_edit_status_ = NONE;
    int pen_width_ = 3;
    bool fill_ = false;
    QColor color_ = Qt::red;

    TextEdit * text_edit_ = nullptr;
    QString font_family_ = "宋体";
    QString font_style_ = "regular";
    int font_size_ = 11;
    QColor font_color_ = Qt::red;

    MainMenu * menu_ = nullptr;
    GraphMenu * gmenu_ = nullptr;
    FontMenu * fmenu_ = nullptr;
    Magnifier * magnifier_ = nullptr;

    QPoint painting_begin_{0, 0}, painting_end_{0, 0};
    QPoint move_begin_{0, 0}, move_end_{0, 0};
    QPoint resize_begin_{0, 0}, resize_end_{0, 0};
    std::vector<QPoint> curves_;

    CommandStack undo_stack_;
    CommandStack redo_stack_;
};

#endif // TESTCAPTURER_H

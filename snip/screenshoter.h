#ifndef TESTCAPTURER_H
#define TESTCAPTURER_H

#include <selector.h>
#include <QTextEdit>
#include <QPixmap>
#include <QSystemTrayIcon>
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
        START_PAINTING_RECTANGLE    = 0x0100, PAINTING_RECTANGLE    = 0x0200, END_PAINTING_RECTANGLE    = 0x0400,
        START_PAINTING_CIRCLE       = 0x0101, PAINTING_CIRCLE       = 0x0201, END_PAINTING_CIRCLE       = 0x0401,
        START_PAINTING_ARROW        = 0x0102, PAINTING_ARROW        = 0x0202, END_PAINTING_ARROW        = 0x0402,
        START_PAINTING_LINE         = 0x0103, PAINTING_LINE         = 0x0203, END_PAINTING_LINE         = 0x0403,
        START_PAINTING_CURVES       = 0x0104, PAINTING_CURVES       = 0x0204, END_PAINTING_CURVES       = 0x0404,
        START_PAINTING_MOSAIC       = 0x0105, PAINTING_MOSAIC       = 0x0205, END_PAINTING_MOSAIC       = 0x0405,
        START_PAINTING_TEXT         = 0x0106, PAINTING_TEXT         = 0x0206, END_PAINTING_TEXT         = 0x0406,
        GRAPH_MOVING, GRAPH_RESIZING,
        END = 0x0600
    };

public:
    explicit ScreenShoter(QWidget *parent = nullptr);

signals:
    void CAPTURE_SCREEN_DONE(QPixmap image);
    void FIX_IMAGE(QPixmap image);
    void SHOW_MESSAGE(const QString &title, const QString &msg,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int msecs = 10000);

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
    QImage mosaic();
    void registerShortcuts();
    void getArrowPoints(QPoint begin, QPoint end_, QPoint* points);

    shared_ptr<Command> getCursorPos(const QPoint& pos);
    void setCursorByPos(Resizer::PointPosition pos, const QCursor & default_cursor = Qt::CrossCursor);

    void upadateMagnifierPosition();

    QPixmap captured_screen_, captured_image_;

    Resizer::PointPosition cursor_graph_pos_ = Resizer::OUTSIDE;

    shared_ptr<Command> command_ = nullptr;
    shared_ptr<Command> focus_ = nullptr;

    uint32_t edit_status_ = NONE, last_edit_status_ = NONE;
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

    QPoint move_begin_{0, 0}, move_end_{0, 0};
    QPoint resize_begin_{0, 0}, resize_end_{0, 0};
    std::vector<QPoint> curves_;

    CommandStack undo_stack_;
    CommandStack redo_stack_;
};

#endif // TESTCAPTURER_H
